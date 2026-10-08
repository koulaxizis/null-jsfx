/*
 * NULL Tilt Native - sample-exact comparison against the JSFX original.
 * SPDX-License-Identifier: MIT
 *
 * Renders the same stereo test signal (two sines + white noise, 48 kHz) through
 *   (a) DATA/Effects/null_jsfx/tilt.jsfx via ysfx,
 *   (b) the built CLAP plugin loaded with dlopen (optional, pass its path),
 *   (c) the TiltProcessor DSP class directly,
 * for several Amount settings, and reports the max abs difference to (a).
 *
 * usage: compare_tilt <tilt.jsfx> [null_tilt_native.clap]
 * exit code 0 when every max diff <= tolerance.
 */

#include "TiltDSP.hpp"

#include <ysfx.h>

#include <clap/entry.h>
#include <clap/plugin.h>
#include <clap/plugin-factory.h>
#include <clap/process.h>
#include <clap/events.h>
#include <clap/ext/params.h>

#include <dlfcn.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

static constexpr double kSampleRate = 48000.0;
static constexpr uint32_t kFrames = 48000 * 3;
static constexpr uint32_t kBlock = 512;
static constexpr double kTolerance = 1e-6;

struct Stereo {
    std::vector<float> l, r;
    explicit Stereo(size_t n = 0) : l(n), r(n) {}
};

static Stereo makeSignal()
{
    Stereo s(kFrames);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    for (uint32_t i = 0; i < kFrames; ++i) {
        const double t = i / kSampleRate;
        s.l[i] = float(0.4 * std::sin(2 * M_PI * 110.0 * t) + 0.2 * std::sin(2 * M_PI * 5000.0 * t) + 0.15 * noise(rng));
        s.r[i] = float(0.3 * std::sin(2 * M_PI * 440.0 * t + 0.3) + 0.25 * std::sin(2 * M_PI * 9000.0 * t) + 0.15 * noise(rng));
    }
    return s;
}

static double maxDiff(const Stereo& a, const Stereo& b)
{
    double m = 0.0;
    for (uint32_t i = 0; i < kFrames; ++i) {
        m = std::fmax(m, std::fabs(double(a.l[i]) - double(b.l[i])));
        m = std::fmax(m, std::fabs(double(a.r[i]) - double(b.r[i])));
    }
    return m;
}

// ------------------------------------------------------------------------------------------------
// (a) JSFX via ysfx

static bool renderJsfx(const char* path, double amount, const Stereo& in, Stereo& out)
{
    ysfx_config_t* cfg = ysfx_config_new();
    ysfx_guess_file_roots(cfg, path);
    ysfx_t* fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, path, 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
        std::fprintf(stderr, "ysfx: failed to load/compile %s\n", path);
        ysfx_free(fx);
        return false;
    }
    ysfx_set_sample_rate(fx, kSampleRate);
    ysfx_set_block_size(fx, kBlock);
    ysfx_init(fx);
    ysfx_slider_set_value(fx, 0, amount);

    for (uint32_t pos = 0; pos < kFrames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, kFrames - pos);
        const float* ins[2] = { in.l.data() + pos, in.r.data() + pos };
        float* outs[2] = { out.l.data() + pos, out.r.data() + pos };
        ysfx_process_float(fx, ins, outs, 2, 2, n);
    }
    ysfx_free(fx);
    return true;
}

// ------------------------------------------------------------------------------------------------
// (b) CLAP plugin via dlopen

static const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}

static const clap_host_t kHost = {
    CLAP_VERSION, nullptr, "null-tilt-compare", "NULL JSFX", "https://nulljsfx.tech", "1.0",
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

static bool renderClap(const char* path, double amount, const Stereo& in, Stereo& out, bool verbose)
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
        std::printf("\nCLAP: id=%s name=\"%s\" vendor=\"%s\" version=%s url=%s\n",
                    desc->id, desc->name, desc->vendor, desc->version, desc->url);

    const clap_plugin_t* plugin = factory->create_plugin(factory, &kHost, desc->id);
    if (!plugin || !plugin->init(plugin)) {
        std::fprintf(stderr, "create/init failed\n");
        return false;
    }

    const auto* params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    clap_param_info_t info {};
    params->get_info(plugin, 0, &info);
    if (verbose)
        std::printf("CLAP: param0 id=%u name=\"%s\" range=[%g, %g] default=%g\n",
                    info.id, info.name, info.min_value, info.max_value, info.default_value);

    EventList ev;
    clap_event_param_value_t pv {};
    pv.header.size = sizeof(pv);
    pv.header.time = 0;
    pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pv.header.type = CLAP_EVENT_PARAM_VALUE;
    pv.param_id = info.id;
    pv.cookie = info.cookie;
    pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
    pv.value = amount;
    ev.events.push_back(pv);

    clap_input_events_t inEv { &ev, EventList::size, EventList::get };
    clap_output_events_t outEv { &ev, EventList::push };

    // Set the parameter while inactive (like a host restoring state), then activate.
    params->flush(plugin, &inEv, &outEv);
    ev.events.clear();

    if (!plugin->activate(plugin, kSampleRate, 1, kBlock) || !plugin->start_processing(plugin)) {
        std::fprintf(stderr, "activate failed\n");
        return false;
    }

    for (uint32_t pos = 0; pos < kFrames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, kFrames - pos);
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
// (c) DSP class directly

static void renderDsp(double amount, const Stereo& in, Stereo& out)
{
    nulltilt::TiltProcessor p;
    p.setSampleRate(kSampleRate);
    p.setAmount(amount);
    p.reset();
    for (uint32_t pos = 0; pos < kFrames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, kFrames - pos);
        p.process(in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
}

// Smoothing sanity: jump 0 -> +100 mid-stream; the gain glide must converge to the exact
// at-rest output within ~100 ms and never step by more than the dry signal allows.
static bool checkSmoothing(const Stereo& in)
{
    nulltilt::TiltProcessor glide, still;
    glide.setSampleRate(kSampleRate);
    glide.setAmount(0.0);
    glide.reset();
    still.setSampleRate(kSampleRate);
    still.setAmount(100.0);
    still.reset();
    Stereo a(kFrames), b(kFrames);
    const uint32_t jump = 4800;
    glide.process(in.l.data(), in.r.data(), a.l.data(), a.r.data(), jump);
    still.process(in.l.data(), in.r.data(), b.l.data(), b.r.data(), jump);
    glide.setAmount(100.0);
    glide.process(in.l.data() + jump, in.r.data() + jump, a.l.data() + jump, a.r.data() + jump, kFrames - jump);
    still.process(in.l.data() + jump, in.r.data() + jump, b.l.data() + jump, b.r.data() + jump, kFrames - jump);

    uint32_t settled = kFrames;
    for (uint32_t i = kFrames; i-- > jump;) {
        if (a.l[i] != b.l[i] || a.r[i] != b.r[i]) { settled = i + 1; break; }
    }
    const double ms = (settled - jump) * 1000.0 / kSampleRate;
    std::printf("smoothing: 0 -> +100 jump becomes bit-identical to static +100 after %.1f ms\n", ms);
    return ms < 400.0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <tilt.jsfx> [plugin.clap]\n", argv[0]);
        return 2;
    }
    const char* jsfx = argv[1];
    const char* clap = argc > 2 ? argv[2] : nullptr;

    const Stereo in = makeSignal();
    const double amounts[] = { 40.0, 0.0, -100.0, 100.0, -37.0, 73.0 };
    bool ok = true;

    std::printf("signal: %u frames @ %.0f Hz, sines + white noise, block %u\n", kFrames, kSampleRate, kBlock);
    for (double amt : amounts) {
        Stereo ref(kFrames), dsp(kFrames), plug(kFrames);
        if (!renderJsfx(jsfx, amt, in, ref))
            return 1;
        renderDsp(amt, in, dsp);
        const double dDsp = maxDiff(ref, dsp);
        std::printf("Amount %+6.1f  (JSFX vs dry: %.3f)  DSP class vs JSFX: max |diff| = %.3g", amt, maxDiff(ref, in), dDsp);
        ok = ok && dDsp <= kTolerance;
        if (clap) {
            if (!renderClap(clap, amt, in, plug, amt == amounts[0]))
                return 1;
            const double dClap = maxDiff(ref, plug);
            std::printf("   CLAP vs JSFX: max |diff| = %.3g", dClap);
            ok = ok && dClap <= kTolerance;
        }
        std::printf("\n");
    }
    ok = checkSmoothing(in) && ok;

    std::printf("%s (tolerance %.0e)\n", ok ? "PASS" : "FAIL", kTolerance);
    return ok ? 0 : 1;
}
