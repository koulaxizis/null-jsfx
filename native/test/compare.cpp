/*
 * NULL JSFX - renders a reference JSFX and a second implementation (a CLAP plugin or another
 * JSFX) with the same signal and slider values, and reports the largest sample difference.
 * SPDX-License-Identifier: MIT
 *
 * usage: compare <ref.jsfx> <other.clap|other.jsfx> [--tol 1e-5] [--values v1,v2,...]
 *
 * The signal is 3 s of stereo music-like material (kick, bass, chord, hats, plus noise) whose
 * level steps between -30 and 0 dBFS so dynamics processors move. Slider 1 / parameter 0 is
 * set before processing starts. Default values: min, max, default and the quartiles of the
 * reference's slider 1 range. Every value runs at 44.1 and 96 kHz.
 * Exit code 0 when every max diff <= tolerance.
 */

#include <ysfx.h>

#include <clap/entry.h>
#include <clap/events.h>
#include <clap/ext/params.h>
#include <clap/plugin-factory.h>
#include <clap/plugin.h>
#include <clap/process.h>

#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static constexpr uint32_t kBlock = 512;

struct Stereo {
    std::vector<float> l, r;
    explicit Stereo(size_t n = 0) : l(n), r(n) {}
};

static Stereo makeSignal(double sr)
{
    const uint32_t frames = uint32_t(sr * 3);
    Stereo s(frames);
    uint32_t seed = 1234;
    for (uint32_t n = 0; n < frames; ++n) {
        const double t = n / sr, tk = std::fmod(t, 0.5), th = std::fmod(t, 0.125);
        seed = seed * 1664525u + 1013904223u;
        const double noise = (seed >> 9) / 4194304.0 - 1.0;
        const double kick = 0.7 * std::exp(-tk * 18) * std::sin(2 * M_PI * (50 + 90 * std::exp(-tk * 40)) * tk);
        const double bass = 0.18 * std::sin(2 * M_PI * 55 * t);
        const double chL = 0.06 * (std::sin(2 * M_PI * 220 * t) + std::sin(2 * M_PI * 277.2 * t) + std::sin(2 * M_PI * 329.6 * t));
        const double chR = 0.06 * (std::sin(2 * M_PI * 220.7 * t + 0.4) + std::sin(2 * M_PI * 276.6 * t + 1.1) + std::sin(2 * M_PI * 330.4 * t + 2.0));
        const double hat = 0.08 * std::exp(-th * 60) * noise;
        // level steps every 0.75 s: 0, -12, -30, -6 dB
        static const double steps[] = { 1.0, 0.251, 0.0316, 0.501 };
        const double g = steps[std::min<uint32_t>(3, uint32_t(t / 0.75))];
        s.l[n] = float(g * (kick + bass + chL + hat + 0.02 * noise));
        s.r[n] = float(g * (kick + bass + chR - 0.6 * hat - 0.02 * noise));
    }
    return s;
}

static double maxDiff(const Stereo& a, const Stereo& b, size_t* where)
{
    double m = 0.0;
    for (size_t i = 0; i < a.l.size(); ++i) {
        const double d = std::fmax(std::fabs(double(a.l[i]) - b.l[i]), std::fabs(double(a.r[i]) - b.r[i]));
        if (d > m) { m = d; *where = i; }
    }
    return m;
}

static bool endsWith(const std::string& s, const char* suf)
{
    const size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

// ------------------------------------------------------------------------------------------------
// JSFX via ysfx

struct SliderRange { double min = 0, max = 100, def = 0; };

static ysfx_t* loadJsfx(const char* path)
{
    ysfx_config_t* cfg = ysfx_config_new();
    ysfx_guess_file_roots(cfg, path);
    ysfx_t* fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, path, 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
        std::fprintf(stderr, "ysfx: failed to load/compile %s\n", path);
        ysfx_free(fx);
        return nullptr;
    }
    return fx;
}

static bool sliderRange(const char* path, SliderRange& r)
{
    ysfx_t* fx = loadJsfx(path);
    if (!fx)
        return false;
    ysfx_slider_range_t rg {};
    ysfx_slider_get_range(fx, 0, &rg);
    r = { rg.min, rg.max, rg.def };
    ysfx_free(fx);
    return true;
}

static bool renderJsfx(const char* path, double sr, double value, const Stereo& in, Stereo& out)
{
    ysfx_t* fx = loadJsfx(path);
    if (!fx)
        return false;
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, kBlock);
    ysfx_init(fx);
    ysfx_slider_set_value(fx, 0, value);
    const uint32_t frames = uint32_t(in.l.size());
    for (uint32_t pos = 0; pos < frames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, frames - pos);
        const float* ins[2] = { in.l.data() + pos, in.r.data() + pos };
        float* outs[2] = { out.l.data() + pos, out.r.data() + pos };
        ysfx_process_float(fx, ins, outs, 2, 2, n);
    }
    ysfx_free(fx);
    return true;
}

// ------------------------------------------------------------------------------------------------
// CLAP plugin via dlopen

static const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}

static const clap_host_t kHost = {
    CLAP_VERSION, nullptr, "null-jsfx-compare", "NULL JSFX", "https://nulljsfx.tech", "1.0",
    hostGetExtension, hostNoop, hostNoop, hostNoop,
};

struct EventList {
    std::vector<clap_event_param_value_t> events;
    static uint32_t size(const clap_input_events_t* l) { return uint32_t(static_cast<EventList*>(l->ctx)->events.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i)
    {
        return &static_cast<EventList*>(l->ctx)->events[i].header;
    }
    static bool push(const clap_output_events_t*, const clap_event_header_t*) { return true; }
};

static bool renderClap(const char* path, double sr, double value, const Stereo& in, Stereo& out, bool verbose)
{
    void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        std::fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return false;
    }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
    if (!entry || !entry->init(path)) {
        std::fprintf(stderr, "clap_entry missing or init failed\n");
        return false;
    }
    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    if (verbose)
        std::printf("CLAP: id=%s name=\"%s\" vendor=\"%s\"\n", desc->id, desc->name, desc->vendor);

    const clap_plugin_t* plugin = factory->create_plugin(factory, &kHost, desc->id);
    if (!plugin || !plugin->init(plugin)) {
        std::fprintf(stderr, "create/init failed\n");
        return false;
    }

    const auto* params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    clap_param_info_t info {};
    params->get_info(plugin, 0, &info);

    EventList ev;
    clap_event_param_value_t pv {};
    pv.header.size = sizeof(pv);
    pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pv.header.type = CLAP_EVENT_PARAM_VALUE;
    pv.param_id = info.id;
    pv.cookie = info.cookie;
    pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
    pv.value = value;
    ev.events.push_back(pv);
    clap_input_events_t inEv { &ev, EventList::size, EventList::get };
    clap_output_events_t outEv { &ev, EventList::push };

    // set the parameter while inactive (like a host restoring state), then activate
    params->flush(plugin, &inEv, &outEv);
    ev.events.clear();

    if (!plugin->activate(plugin, sr, 1, kBlock) || !plugin->start_processing(plugin)) {
        std::fprintf(stderr, "activate failed\n");
        return false;
    }
    const uint32_t frames = uint32_t(in.l.size());
    for (uint32_t pos = 0; pos < frames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, frames - pos);
        float* ins[2] = { const_cast<float*>(in.l.data()) + pos, const_cast<float*>(in.r.data()) + pos };
        float* outs[2] = { out.l.data() + pos, out.r.data() + pos };
        clap_audio_buffer_t ib {}, ob {};
        ib.data32 = ins; ib.channel_count = 2;
        ob.data32 = outs; ob.channel_count = 2;
        clap_process_t proc {};
        proc.steady_time = pos;
        proc.frames_count = n;
        proc.audio_inputs = &ib; proc.audio_inputs_count = 1;
        proc.audio_outputs = &ob; proc.audio_outputs_count = 1;
        proc.in_events = &inEv; proc.out_events = &outEv;
        if (plugin->process(plugin, &proc) == CLAP_PROCESS_ERROR) {
            std::fprintf(stderr, "process error\n");
            return false;
        }
    }
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    entry->deinit();
    dlclose(lib);
    return true;
}

// ------------------------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <ref.jsfx> <other.clap|other.jsfx> [--tol X] [--values a,b,...]\n", argv[0]);
        return 2;
    }
    const char* ref = argv[1];
    const std::string other = argv[2];
    double tol = 1e-5;
    std::vector<double> values;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--tol"))
            tol = std::atof(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--values")) {
            std::string v = argv[i + 1];
            for (size_t p = 0; p < v.size();) {
                size_t q = v.find(',', p);
                values.push_back(std::atof(v.substr(p, q - p).c_str()));
                p = q == std::string::npos ? v.size() : q + 1;
            }
        }
    }
    if (values.empty()) {
        SliderRange r;
        if (!sliderRange(ref, r))
            return 1;
        values = { r.min, r.max, r.def, r.min + (r.max - r.min) * 0.25, r.min + (r.max - r.min) * 0.5,
                   r.min + (r.max - r.min) * 0.75 };
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    }

    const bool isJsfx = endsWith(other, ".jsfx");
    bool ok = true, first = true;
    for (double sr : { 44100.0, 96000.0 }) {
        const Stereo in = makeSignal(sr);
        for (double v : values) {
            Stereo a(in.l.size()), b(in.l.size());
            if (!renderJsfx(ref, sr, v, in, a))
                return 1;
            if (isJsfx ? !renderJsfx(other.c_str(), sr, v, in, b) : !renderClap(other.c_str(), sr, v, in, b, first))
                return 1;
            first = false;
            size_t at = 0;
            const double d = maxDiff(a, b, &at);
            const bool pass = d <= tol && std::isfinite(d);
            std::printf("%6.0f Hz  value %8.2f  max |diff| = %.3g%s\n", sr, v, d,
                        pass ? "" : (" at sample " + std::to_string(at) + "  FAIL").c_str());
            ok = ok && pass;
        }
    }
    std::printf("%s (tolerance %.0e)\n", ok ? "PASS" : "FAIL", tol);
    return ok ? 0 : 1;
}
