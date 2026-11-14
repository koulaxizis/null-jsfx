/*
 * NULL JSFX - checks that a CLAP plugin's saved state comes back exactly, however the host
 * splits the stream into reads and writes.
 * SPDX-License-Identifier: MIT
 *
 * usage: state_test <plugin.clap>
 *
 * One instance gets random parameter values (on each parameter's step), and its state is saved
 * in one write. For each chunk size (1, 2, 7, 17, 23, 64, 511, 512, 4096 bytes and the whole
 * state) a fresh instance loads that state through a stream that returns at most that many bytes
 * per read, and saves it again through a stream that takes at most that many bytes per write.
 * Every parameter value and the saved bytes must match the first instance. Before each load the
 * stack below the caller is filled with zero bytes and then with 0xfe bytes, so a parser that
 * reads past the bytes a read returned sees the worst case (DPF did: clap-validator's
 * state-reproducibility-buffered, which reads 17 bytes at a time). Three rounds of random values.
 *
 * Then hostile states: the saved state with every parameter value replaced by "nan", "inf",
 * "-inf", "1e30", "-1e30" or "2147483647" (a shared project or preset file can carry anything).
 * Each must load to values inside every parameter's range, and the plugin must then process
 * 64 blocks of noise without crashing (run under a sanitizer, a read out of bounds fails too).
 * Exit code 0 when everything matches.
 */

#include <clap/entry.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/plugin-factory.h>
#include <clap/plugin.h>
#include <clap/stream.h>

#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}

static const clap_host_t kHost = {
    CLAP_VERSION, nullptr, "null-jsfx-state-test", "NULL JSFX", "https://nulljsfx.tech", "1.0",
    hostGetExtension, hostNoop, hostNoop, hostNoop,
};

struct ParamEvents {
    std::vector<clap_event_param_value_t> events;
    static uint32_t size(const clap_input_events_t* l) { return uint32_t(static_cast<ParamEvents*>(l->ctx)->events.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i)
    {
        return &static_cast<ParamEvents*>(l->ctx)->events[i].header;
    }
    static bool push(const clap_output_events_t*, const clap_event_header_t*) { return true; }
};

// a stream that moves at most `chunk` bytes per call (0 = no limit)
struct Stream {
    std::vector<uint8_t> data;
    size_t pos = 0;
    size_t chunk = 0;

    static int64_t read(const clap_istream_t* s, void* buffer, uint64_t size)
    {
        auto* self = static_cast<Stream*>(s->ctx);
        size_t n = std::min<size_t>(size_t(size), self->data.size() - self->pos);
        if (self->chunk)
            n = std::min(n, self->chunk);
        std::memcpy(buffer, self->data.data() + self->pos, n);
        self->pos += n;
        return int64_t(n);
    }
    static int64_t write(const clap_ostream_t* s, const void* buffer, uint64_t size)
    {
        auto* self = static_cast<Stream*>(s->ctx);
        size_t n = size_t(size);
        if (self->chunk)
            n = std::min(n, self->chunk);
        const auto* b = static_cast<const uint8_t*>(buffer);
        self->data.insert(self->data.end(), b, b + n);
        return int64_t(n);
    }
};

struct Instance {
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;

    bool create(const clap_plugin_factory_t* factory, const char* id)
    {
        plugin = factory->create_plugin(factory, &kHost, id);
        if (!plugin || !plugin->init(plugin))
            return false;
        params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
        return params && state;
    }
    void destroy()
    {
        if (plugin)
            plugin->destroy(plugin);
        plugin = nullptr;
    }
    std::vector<double> values() const
    {
        std::vector<double> v(params->count(plugin));
        for (uint32_t k = 0; k < v.size(); ++k) {
            clap_param_info_t info {};
            params->get_info(plugin, k, &info);
            params->get_value(plugin, info.id, &v[k]);
        }
        return v;
    }
    bool save(Stream& s) const
    {
        clap_ostream_t os { &s, Stream::write };
        return state->save(plugin, &os);
    }
};

// leave `fill` in the stack the next call will use
__attribute__((noinline)) static void fillStack(uint8_t fill)
{
    volatile uint8_t pad[64 * 1024];
    for (size_t k = 0; k < sizeof(pad); ++k)
        pad[k] = fill;
}

__attribute__((noinline)) static bool loadChunked(const Instance& in, Stream& s, uint8_t fill)
{
    fillStack(fill);
    clap_istream_t is { &s, Stream::read };
    return in.state->load(in.plugin, &is);
}

static double nextRandom(uint32_t& seed); // below

// the state with every value of DPF's parameter section ("__dpf_parameters_begin__", then
// symbol and value pairs, each ended by a NUL, then "__dpf_parameters_end__") set to `value`
static std::vector<uint8_t> hostileState(const std::vector<uint8_t>& saved, const char* value)
{
    static const char kBegin[] = "__dpf_parameters_begin__";
    static const char kEnd[] = "__dpf_parameters_end__";
    std::vector<std::string> parts;
    std::string cur;
    for (uint8_t b : saved) {
        if (b == 0) { parts.push_back(cur); cur.clear(); }
        else cur += char(b);
    }
    std::vector<uint8_t> out;
    bool inParams = false;
    size_t k = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        std::string p = parts[i];
        if (p == kEnd || p.find(kEnd) != std::string::npos)
            inParams = false;
        else if (inParams && (k++ % 2) == 1)
            p = value;
        if (p.size() >= sizeof(kBegin) - 1 && p.compare(p.size() - (sizeof(kBegin) - 1), std::string::npos, kBegin) == 0) {
            inParams = true;
            k = 0;
        }
        out.insert(out.end(), p.begin(), p.end());
        out.push_back(0);
    }
    out.insert(out.end(), cur.begin(), cur.end()); // bytes after the last NUL
    return out;
}

static uint32_t noEvents(const clap_input_events_t*) { return 0; }
static const clap_event_header_t* noEvent(const clap_input_events_t*, uint32_t) { return nullptr; }

// activates the instance and runs `blocks` blocks of noise through every audio port
static void processNoise(const Instance& in, int blocks, uint32_t& seed)
{
    const auto* ports = static_cast<const clap_plugin_audio_ports_t*>(in.plugin->get_extension(in.plugin, CLAP_EXT_AUDIO_PORTS));
    const uint32_t frames = 256;
    std::vector<std::vector<float>> mem;
    std::vector<std::vector<float*>> ptrs[2];
    std::vector<clap_audio_buffer_t> bufs[2];
    for (int dir = 0; dir < 2; ++dir) {
        const uint32_t n = ports ? ports->count(in.plugin, dir == 0) : 0;
        ptrs[dir].resize(n);
        for (uint32_t k = 0; k < n; ++k) {
            clap_audio_port_info_t info {};
            ports->get(in.plugin, k, dir == 0, &info);
            for (uint32_t c = 0; c < info.channel_count; ++c)
                mem.emplace_back(frames, 0.0f);
        }
    }
    size_t m = 0;
    for (int dir = 0; dir < 2; ++dir) {
        for (size_t k = 0; k < ptrs[dir].size(); ++k) {
            clap_audio_port_info_t info {};
            ports->get(in.plugin, uint32_t(k), dir == 0, &info);
            for (uint32_t c = 0; c < info.channel_count; ++c)
                ptrs[dir][k].push_back(mem[m++].data());
        }
        for (auto& p : ptrs[dir]) {
            clap_audio_buffer_t b {};
            b.data32 = p.data();
            b.channel_count = uint32_t(p.size());
            bufs[dir].push_back(b);
        }
    }
    if (!in.plugin->activate(in.plugin, 48000.0, 1, frames))
        return;
    in.plugin->start_processing(in.plugin);
    ParamEvents none;
    clap_input_events_t inEv { &none, noEvents, noEvent };
    clap_output_events_t outEv { &none, ParamEvents::push };
    clap_process_t pr {};
    pr.steady_time = -1;
    pr.frames_count = frames;
    pr.audio_inputs = bufs[0].data();
    pr.audio_inputs_count = uint32_t(bufs[0].size());
    pr.audio_outputs = bufs[1].data();
    pr.audio_outputs_count = uint32_t(bufs[1].size());
    pr.in_events = &inEv;
    pr.out_events = &outEv;
    for (int b = 0; b < blocks; ++b) {
        for (auto& p : ptrs[0])
            for (float* ch : p)
                for (uint32_t i = 0; i < frames; ++i)
                    ch[i] = float(nextRandom(seed) * 2.0 - 1.0);
        in.plugin->process(in.plugin, &pr);
    }
    in.plugin->stop_processing(in.plugin);
    in.plugin->deactivate(in.plugin);
}

// the exact Park-Miller step the suite uses for rand()
static double nextRandom(uint32_t& seed)
{
    seed = uint32_t((uint64_t(seed) * 16807u) % 2147483647u);
    return double(seed) / 2147483647.0;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <plugin.clap>\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
    if (!entry || !entry->init(path)) {
        std::fprintf(stderr, "clap_entry missing or init failed\n");
        return 1;
    }
    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    const char* id = factory->get_plugin_descriptor(factory, 0)->id;

    static const size_t kChunks[] = { 1, 2, 7, 17, 23, 64, 511, 512, 4096, 0 };
    static const uint8_t kFills[] = { 0x00, 0xfe };
    uint32_t seed = 12345;
    int failures = 0, checks = 0;

    for (int round = 0; round < 3; ++round) {
        Instance ref;
        if (!ref.create(factory, id)) {
            std::fprintf(stderr, "create/init failed\n");
            return 1;
        }
        ParamEvents ev;
        const uint32_t count = ref.params->count(ref.plugin);
        for (uint32_t k = 0; k < count; ++k) {
            clap_param_info_t info {};
            ref.params->get_info(ref.plugin, k, &info);
            if (info.flags & CLAP_PARAM_IS_READONLY)
                continue;
            double v = info.min_value + nextRandom(seed) * (info.max_value - info.min_value);
            if (info.flags & CLAP_PARAM_IS_STEPPED)
                v = std::round(v);
            clap_event_param_value_t pv {};
            pv.header.size = sizeof(pv);
            pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            pv.header.type = CLAP_EVENT_PARAM_VALUE;
            pv.param_id = info.id;
            pv.cookie = info.cookie;
            pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
            pv.value = v;
            ev.events.push_back(pv);
        }
        clap_input_events_t inEv { &ev, ParamEvents::size, ParamEvents::get };
        clap_output_events_t outEv { &ev, ParamEvents::push };
        ref.params->flush(ref.plugin, &inEv, &outEv);

        Stream saved;
        if (!ref.save(saved)) {
            std::fprintf(stderr, "save failed\n");
            return 1;
        }
        const std::vector<double> expected = ref.values();
        ref.destroy();

        for (size_t chunk : kChunks) {
            for (uint8_t fill : kFills) {
                ++checks;
                Instance in;
                if (!in.create(factory, id)) {
                    std::fprintf(stderr, "create/init failed\n");
                    return 1;
                }
                Stream src;
                src.data = saved.data;
                src.chunk = chunk;
                const bool loaded = loadChunked(in, src, fill);
                const std::vector<double> got = in.values();
                Stream again;
                again.chunk = chunk;
                const bool resaved = in.save(again);
                in.destroy();

                std::string why;
                if (!loaded)
                    why = "load failed";
                else if (!resaved)
                    why = "save failed";
                else {
                    for (size_t k = 0; k < got.size(); ++k)
                        if (got[k] != expected[k]) {
                            char line[160];
                            std::snprintf(line, sizeof(line), "parameter %zu: %.9g, saved %.9g", k, got[k], expected[k]);
                            why = line;
                            break;
                        }
                    if (why.empty() && again.data != saved.data)
                        why = "the state saved again differs from the first";
                }
                if (!why.empty()) {
                    ++failures;
                    std::printf("FAIL round %d, %zu-byte reads, stack 0x%02x: %s\n", round, chunk, unsigned(fill), why.c_str());
                }
            }
        }
    }
    std::printf("%s: %d of %d state round trips match\n", id, checks - failures, checks);

    // hostile states
    int bad = 0, tries = 0;
    {
        Instance ref;
        if (!ref.create(factory, id)) {
            std::fprintf(stderr, "create/init failed\n");
            return 1;
        }
        Stream saved;
        ref.save(saved);
        ref.destroy();
        static const char* const kHostile[] = { "nan", "inf", "-inf", "1e30", "-1e30", "2147483647" };
        for (const char* value : kHostile) {
            ++tries;
            Instance in;
            if (!in.create(factory, id)) {
                std::fprintf(stderr, "create/init failed\n");
                return 1;
            }
            Stream src;
            src.data = hostileState(saved.data, value);
            clap_istream_t is { &src, Stream::read };
            in.state->load(in.plugin, &is); // a refusal is fine too
            std::string why;
            const uint32_t count = in.params->count(in.plugin);
            for (uint32_t k = 0; k < count && why.empty(); ++k) {
                clap_param_info_t info {};
                in.params->get_info(in.plugin, k, &info);
                double v = 0.0;
                in.params->get_value(in.plugin, info.id, &v);
                if (!(v >= info.min_value && v <= info.max_value)) {
                    char line[200];
                    std::snprintf(line, sizeof(line), "parameter %u (%s): %.9g outside %.9g..%.9g", k, info.name, v,
                                  info.min_value, info.max_value);
                    why = line;
                }
            }
            processNoise(in, 64, seed);
            in.destroy();
            if (!why.empty()) {
                ++bad;
                std::printf("FAIL hostile state \"%s\": %s\n", value, why.c_str());
            }
        }
    }
    entry->deinit();
    std::printf("%s: %d of %d hostile states held in range\n", id, tries - bad, tries);
    return (failures || bad) ? 1 : 0;
}
