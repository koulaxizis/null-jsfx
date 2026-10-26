/*
 * NULL JSFX - renders a reference JSFX and a second implementation (a CLAP plugin or another
 * JSFX) with the same signal and slider values, and reports the largest sample difference.
 * SPDX-License-Identifier: MIT
 *
 * usage: compare <ref.jsfx> <other.clap|other.jsfx|module:id> [--tol 1e-5] [--values v1,v2,...]
 *                [--rpl presets.jsfx.rpl]
 *
 * module:<id> (only in the module_compare build of this file) renders the Rack's module <id>
 * (plugins/rack/modules/<id>.hpp) instead of a plugin, set up the way the Rack drives it.
 *
 * The signal is 3 s of stereo music-like material (kick, bass, chord, hats, plus noise) whose
 * level steps between -30 and 0 dBFS so dynamics processors move. The sliders / parameters are
 * set before processing starts; the k-th slider the JSFX declares is the k-th CLAP parameter.
 * With one slider: min, max, default and the quartiles of its range (or --values). With more:
 * all defaults, then each slider at its min and at its max with the others at default, then
 * random settings (rounded to each slider's step). With --rpl: every preset of that REAPER
 * preset library instead (the factory presets, tools/build_presets.py), also printing the output
 * level against the input's. Every setting runs at 44.1 and 96 kHz.
 * Exit code 0 when every max diff <= tolerance.
 *
 * Sidechain plugins (a JSFX with more than two input pins, a CLAP with a second input port):
 * pins 3/4 (the sidechain port) get a separate key signal, a kick-like pattern with level steps.
 * File sliders (slider:/folder:default:Name) stay at their default and are not CLAP parameters:
 * the k-th other slider is the k-th CLAP parameter.
 *
 * Every output pin the reference JSFX declares is compared (two for most plugins, eight for the
 * Band Splitter); the CLAP's output ports are numbered across like the pins.
 *
 * Instruments (a JSFX without input pins) get no audio but MIDI notes on the General MIDI drum
 * map (one every 1/8 s, varied velocities) and a playing transport at 120 bpm in 4/4 from beat 0,
 * so their sequencer runs too, merged in time order with the MIDI effects' stream below (note-offs,
 * note-ons with velocity 0, the same key again, sustain, all notes off, pitch bend, pressure,
 * program changes); the CLAP gets the same MIDI and transport.
 *
 * MIDI effects (a JSFX in //category: MIDI) get, besides the audio, a stream of MIDI on three
 * channels (chords of overlapping notes, repeated note-ons, note-ons with velocity 0, poly and
 * channel pressure, pitch bend, sustain, program changes and all notes off) and every message
 * they send is compared too: the same messages, bytes and sample, in the same order.
 */

#include <ysfx.h>

#include <clap/entry.h>
#include <clap/ext/audio-ports.h>
#include <clap/events.h>
#include <clap/ext/params.h>
#include <clap/fixedpoint.h>
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

// the rendered output: one buffer per output pin
using Channels = std::vector<std::vector<float>>;

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

// key signal for the sidechain inputs: kick-like hits every 0.6 s, level steps every 0.75 s
// (0, -24, -9 dB, silence) so a ducker triggers, holds and releases
static Stereo makeSidechain(double sr)
{
    const uint32_t frames = uint32_t(sr * 3);
    Stereo s(frames);
    for (uint32_t n = 0; n < frames; ++n) {
        const double t = n / sr, tk = std::fmod(t, 0.6);
        const double hit = 0.9 * std::exp(-tk * 9) * std::sin(2 * M_PI * (60 + 120 * std::exp(-tk * 30)) * tk);
        static const double steps[] = { 1.0, 0.063, 0.355, 0.0 };
        const double g = steps[std::min<uint32_t>(3, uint32_t(t / 0.75))];
        s.l[n] = float(g * hit);
        s.r[n] = float(g * hit * 0.7 + g * 0.05 * std::sin(2 * M_PI * 3000 * t));
    }
    return s;
}

static double maxDiff(const Channels& a, const Channels& b, size_t* where, size_t* channel)
{
    double m = 0.0;
    for (size_t c = 0; c < a.size(); ++c)
        for (size_t i = 0; i < a[c].size(); ++i) {
            const double d = std::fabs(double(a[c][i]) - b[c][i]);
            if (d > m) { m = d; *where = i; *channel = c; }
        }
    return m;
}

// the notes an instrument gets: (sample, note, velocity)
struct TestNote { uint32_t at; uint8_t note, vel; };
static std::vector<TestNote> testNotes(double sr, uint32_t frames)
{
    static const uint8_t notes[] = { 36, 42, 38, 46, 39, 37, 41, 47, 56, 49, 35, 44, 40, 45, 48, 51 };
    std::vector<TestNote> out;
    const uint32_t every = uint32_t(sr / 8);
    for (uint32_t k = 0; k * every < frames; ++k)
        out.push_back({ k * every, notes[k % 16], uint8_t(40 + (k * 37) % 88) });
    return out;
}

// the MIDI a MIDI effect gets: (sample, status, data 1, data 2), in time order
struct MidiMsg { uint32_t at; uint8_t st, d1, d2; uint32_t size; };
static std::vector<MidiMsg> testMidi(double sr, uint32_t frames)
{
    std::vector<MidiMsg> out;
    uint32_t seed = 2024;
    auto rnd = [&seed](uint32_t n) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % n; };
    std::vector<std::pair<uint8_t, uint8_t>> held; // (channel, note)
    bool allOff = false;
    const uint32_t step = uint32_t(sr / 40);
    for (uint32_t at = uint32_t(sr * 0.003); at < frames; at += step / 2 + rnd(step)) {
        const uint8_t ch = uint8_t(rnd(10) < 7 ? 0 : rnd(2) ? 1 : 9);
        const uint32_t kind = rnd(100);
        if (at > frames * 9 / 10 && !allOff) {
            allOff = true;
            out.push_back({ at, 0xB0, 123, 0, 3 }); // all notes off near the end, then a few more notes
            held.erase(std::remove_if(held.begin(), held.end(), [](const std::pair<uint8_t, uint8_t>& h) { return h.first == 0; }), held.end());
        }
        if (kind < 45 || held.empty()) {
            const uint8_t note = uint8_t(24 + rnd(84));
            const uint8_t vel = uint8_t(1 + rnd(127));
            out.push_back({ at, uint8_t(0x90 | ch), note, vel, 3 });
            held.push_back({ ch, note });
            if (rnd(4) == 0) { // a second key at the same sample
                const uint8_t n2 = uint8_t(24 + rnd(84));
                out.push_back({ at, uint8_t(0x90 | ch), n2, uint8_t(1 + rnd(127)), 3 });
                held.push_back({ ch, n2 });
            }
        } else if (kind < 80) {
            const size_t k = rnd(uint32_t(held.size()));
            const bool zero = rnd(3) == 0; // a note-on with velocity 0 ends it as well
            out.push_back({ at, uint8_t((zero ? 0x90 : 0x80) | held[k].first), held[k].second, uint8_t(zero ? 0 : rnd(128)), 3 });
            held.erase(held.begin() + long(k));
        } else if (kind < 84) {
            const size_t k = rnd(uint32_t(held.size()));
            out.push_back({ at, uint8_t(0xA0 | held[k].first), held[k].second, uint8_t(rnd(128)), 3 });
        } else if (kind < 88) {
            out.push_back({ at, uint8_t(0xB0 | ch), 64, uint8_t(rnd(2) ? 127 : 0), 3 });
        } else if (kind < 92) {
            out.push_back({ at, uint8_t(0xE0 | ch), uint8_t(rnd(128)), uint8_t(rnd(128)), 3 });
        } else if (kind < 95) {
            out.push_back({ at, uint8_t(0xD0 | ch), uint8_t(rnd(128)), 0, 2 });
        } else if (kind < 97) {
            out.push_back({ at, uint8_t(0xC0 | ch), uint8_t(rnd(128)), 0, 2 });
        } else {
            const size_t k = rnd(uint32_t(held.size())); // the same key again, without a note-off
            out.push_back({ at, uint8_t(0x90 | held[k].first), held[k].second, uint8_t(1 + rnd(127)), 3 });
        }
    }
    return out;
}

static bool g_midifx = false; // the reference JSFX is a MIDI effect (//category: MIDI)

static bool endsWith(const std::string& s, const char* suf)
{
    const size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

// ------------------------------------------------------------------------------------------------
// JSFX via ysfx

struct Slider { uint32_t index; double min, max, def, inc; std::string name; bool path; };

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

static bool g_instrument = false; // the reference JSFX has no input pins

// the MIDI a plugin gets, in time order: a MIDI effect the stream above, an instrument its notes
// and that stream (at the same sample, the note first)
static std::vector<MidiMsg> testInput(double sr, uint32_t frames)
{
    if (g_midifx)
        return testMidi(sr, frames);
    std::vector<MidiMsg> out;
    if (g_instrument) {
        for (const TestNote& n : testNotes(sr, frames))
            out.push_back({ n.at, 0x99, n.note, n.vel, 3 });
        const std::vector<MidiMsg> more = testMidi(sr, frames);
        out.insert(out.end(), more.begin(), more.end());
        std::stable_sort(out.begin(), out.end(), [](const MidiMsg& a, const MidiMsg& b) { return a.at < b.at; });
    }
    return out;
}

static bool sliders(const char* path, std::vector<Slider>& out, uint32_t* numOutputs)
{
    ysfx_t* fx = loadJsfx(path);
    if (!fx)
        return false;
    *numOutputs = std::max<uint32_t>(2, ysfx_get_num_outputs(fx));
    g_instrument = ysfx_get_num_inputs(fx) == 0;
    if (std::FILE* f = std::fopen(path, "rb")) {
        char line[256];
        for (int i = 0; i < 6 && std::fgets(line, sizeof(line), f); ++i)
            if (!std::strncmp(line, "//category: MIDI", 16))
                g_midifx = true;
        std::fclose(f);
    }
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
        if (!ysfx_slider_exists(fx, i))
            continue;
        ysfx_slider_range_t rg {};
        ysfx_slider_get_range(fx, i, &rg);
        const char* name = ysfx_slider_get_name(fx, i);
        out.push_back({ i, rg.min, rg.max, rg.def, rg.inc, name ? name : "", ysfx_slider_is_path(fx, i) });
    }
    ysfx_free(fx);
    return true;
}

static bool renderJsfx(const char* path, double sr, const std::vector<Slider>& sl, const std::vector<double>& values,
                       const Stereo& in, const Stereo& sc, Channels& out, std::vector<MidiMsg>* midiOut = nullptr)
{
    ysfx_t* fx = loadJsfx(path);
    if (!fx)
        return false;
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, kBlock);
    ysfx_init(fx);
    // transport stopped at 120 bpm, 4/4, like a DAW that is not playing (the CLAP host gives
    // no transport either); ysfx would otherwise report "playing", frozen at beat 0
    ysfx_time_info_t ti {};
    ti.tempo = 120;
    ti.playback_state = ysfx_playback_paused;
    ti.time_signature[0] = 4;
    ti.time_signature[1] = 4;
    ysfx_set_time_info(fx, &ti);
    for (size_t k = 0; k < sl.size(); ++k)
        ysfx_slider_set_value(fx, sl[k].index, values[k]);
    const uint32_t frames = uint32_t(in.l.size());
    const uint32_t numIns = ysfx_get_num_inputs(fx) > 2 ? 4 : 2; // pins 3/4: the sidechain
    const std::vector<MidiMsg> fxMidi = testInput(sr, frames);
    size_t nextFx = 0;
    for (uint32_t pos = 0; pos < frames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, frames - pos);
        for (; nextFx < fxMidi.size() && fxMidi[nextFx].at < pos + n; ++nextFx) {
            const uint8_t msg[3] = { fxMidi[nextFx].st, fxMidi[nextFx].d1, fxMidi[nextFx].d2 };
            ysfx_midi_event_t ev { 0, fxMidi[nextFx].at - pos, fxMidi[nextFx].size, msg };
            ysfx_send_midi(fx, &ev);
        }
        if (g_instrument) {
            ti.playback_state = ysfx_playback_playing;
            ti.time_position = pos / sr;
            ti.beat_position = pos / sr * 2.0;
            ysfx_set_time_info(fx, &ti);
        }
        const float* ins[4] = { in.l.data() + pos, in.r.data() + pos, sc.l.data() + pos, sc.r.data() + pos };
        std::vector<float*> outs(out.size());
        for (size_t c = 0; c < out.size(); ++c)
            outs[c] = out[c].data() + pos;
        ysfx_process_float(fx, ins, outs.data(), numIns, uint32_t(out.size()), n);
        ysfx_midi_event_t me;
        while (ysfx_receive_midi(fx, &me))
            if (midiOut && me.size >= 1)
                midiOut->push_back({ pos + me.offset, me.data[0], uint8_t(me.size > 1 ? me.data[1] : 0),
                                     uint8_t(me.size > 2 ? me.data[2] : 0), me.size });
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

struct MidiList {
    std::vector<clap_event_midi_t> events;
    static uint32_t size(const clap_input_events_t* l) { return uint32_t(static_cast<MidiList*>(l->ctx)->events.size()); }
    static const clap_event_header_t* get(const clap_input_events_t* l, uint32_t i)
    {
        return &static_cast<MidiList*>(l->ctx)->events[i].header;
    }
};

// what a MIDI effect sends: every CLAP_EVENT_MIDI, at the block position + its time
struct MidiCollector {
    std::vector<MidiMsg>* out = nullptr;
    uint32_t pos = 0;
    bool ordered = true;
    static bool push(const clap_output_events_t* l, const clap_event_header_t* h)
    {
        auto* self = static_cast<MidiCollector*>(l->ctx);
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_MIDI && self->out) {
            const auto* e = reinterpret_cast<const clap_event_midi_t*>(h);
            if (!self->out->empty() && self->out->back().at > self->pos + h->time)
                self->ordered = false;
            const uint8_t st = e->data[0];
            const uint32_t sz = (st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0 ? 2 : 3;
            self->out->push_back({ self->pos + h->time, st, e->data[1], uint8_t(sz > 2 ? e->data[2] : 0), sz });
        }
        return true;
    }
};

static bool renderClap(const char* path, double sr, const std::vector<double>& values, const Stereo& in,
                       const Stereo& sc, Channels& out, bool verbose, std::vector<MidiMsg>* midiOut = nullptr)
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
    if (params->count(plugin) != values.size()) {
        std::fprintf(stderr, "the CLAP has %u parameters, the JSFX %zu sliders (without file sliders)\n", params->count(plugin), values.size());
        return false;
    }

    EventList ev;
    for (uint32_t k = 0; k < values.size(); ++k) {
        clap_param_info_t info {};
        params->get_info(plugin, k, &info);
        clap_event_param_value_t pv {};
        pv.header.size = sizeof(pv);
        pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        pv.header.type = CLAP_EVENT_PARAM_VALUE;
        pv.param_id = info.id;
        pv.cookie = info.cookie;
        pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
        pv.value = values[k];
        ev.events.push_back(pv);
    }
    clap_input_events_t inEv { &ev, EventList::size, EventList::get };
    clap_output_events_t outEv { &ev, EventList::push };

    // set the parameter while inactive (like a host restoring state), then activate
    params->flush(plugin, &inEv, &outEv);
    ev.events.clear();

    if (!plugin->activate(plugin, sr, 1, kBlock) || !plugin->start_processing(plugin)) {
        std::fprintf(stderr, "activate failed\n");
        return false;
    }
    // input ports: the main stereo pair, plus a sidechain port on sidechain plugins; channels
    // are numbered across the ports like JSFX pins (3/4 = the sidechain), extra ones get silence
    // output ports likewise: their channels are the output pins in order, extra ones are dropped
    std::vector<uint32_t> inChannels { 2 }, outChannels { 2 };
    if (const auto* ports = static_cast<const clap_plugin_audio_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS))) {
        for (bool isIn : { true, false }) {
            std::vector<uint32_t>& chans = isIn ? inChannels : outChannels;
            chans.clear();
            for (uint32_t p = 0; p < ports->count(plugin, isIn); ++p) {
                clap_audio_port_info_t pi {};
                ports->get(plugin, p, isIn, &pi);
                chans.push_back(pi.channel_count);
            }
        }
    }
    uint32_t totalOuts = 0;
    for (uint32_t c : outChannels)
        totalOuts += c;
    std::vector<std::vector<float>> spare(totalOuts, std::vector<float>(kBlock));
    std::vector<float*> outPtrs(std::max<uint32_t>(totalOuts, 1));
    std::vector<clap_audio_buffer_t> obs(outChannels.size());
    uint32_t totalIns = 0;
    for (uint32_t c : inChannels)
        totalIns += c;
    std::vector<float> silence(kBlock, 0.0f);
    std::vector<float*> inPtrs(std::max<uint32_t>(totalIns, 1));
    std::vector<clap_audio_buffer_t> ibs(inChannels.size());

    const uint32_t frames = uint32_t(in.l.size());
    const std::vector<MidiMsg> fxMidi = testInput(sr, frames);
    size_t nextFx = 0;
    MidiList midi;
    clap_input_events_t midiEv { &midi, MidiList::size, MidiList::get };
    MidiCollector collect;
    collect.out = midiOut;
    clap_output_events_t midiOutEv { &collect, MidiCollector::push };
    clap_event_transport_t transport {};
    for (uint32_t pos = 0; pos < frames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, frames - pos);
        midi.events.clear();
        for (; nextFx < fxMidi.size() && fxMidi[nextFx].at < pos + n; ++nextFx) {
            clap_event_midi_t me {};
            me.header.size = sizeof(me);
            me.header.time = fxMidi[nextFx].at - pos;
            me.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            me.header.type = CLAP_EVENT_MIDI;
            me.port_index = 0;
            me.data[0] = fxMidi[nextFx].st;
            me.data[1] = fxMidi[nextFx].d1;
            me.data[2] = fxMidi[nextFx].d2;
            midi.events.push_back(me);
        }
        collect.pos = pos;
        if (g_instrument) {
            transport.header.size = sizeof(transport);
            transport.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            transport.header.type = CLAP_EVENT_TRANSPORT;
            transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE
                            | CLAP_TRANSPORT_HAS_TIME_SIGNATURE | CLAP_TRANSPORT_IS_PLAYING;
            transport.song_pos_beats = clap_beattime(std::llround(pos / sr * 2.0 * double(CLAP_BEATTIME_FACTOR)));
            transport.song_pos_seconds = clap_sectime(std::llround(pos / sr * double(CLAP_SECTIME_FACTOR)));
            transport.tempo = 120.0;
            transport.tsig_num = 4;
            transport.tsig_denom = 4;
        }
        float* srcs[4] = { const_cast<float*>(in.l.data()) + pos, const_cast<float*>(in.r.data()) + pos,
                           const_cast<float*>(sc.l.data()) + pos, const_cast<float*>(sc.r.data()) + pos };
        for (uint32_t c = 0; c < totalIns; ++c)
            inPtrs[c] = c < 4 ? srcs[c] : silence.data();
        for (uint32_t p = 0, c = 0; p < inChannels.size(); c += inChannels[p], ++p) {
            ibs[p] = clap_audio_buffer_t {};
            ibs[p].data32 = inPtrs.data() + c;
            ibs[p].channel_count = inChannels[p];
        }
        for (uint32_t c = 0; c < totalOuts; ++c)
            outPtrs[c] = c < out.size() ? out[c].data() + pos : spare[c].data();
        for (uint32_t p = 0, c = 0; p < outChannels.size(); c += outChannels[p], ++p) {
            obs[p] = clap_audio_buffer_t {};
            obs[p].data32 = outPtrs.data() + c;
            obs[p].channel_count = outChannels[p];
        }
        clap_process_t proc {};
        proc.steady_time = pos;
        proc.frames_count = n;
        proc.audio_inputs = ibs.data(); proc.audio_inputs_count = uint32_t(ibs.size());
        proc.audio_outputs = obs.data(); proc.audio_outputs_count = uint32_t(obs.size());
        proc.in_events = g_instrument || g_midifx ? &midiEv : &inEv; proc.out_events = g_midifx ? &midiOutEv : &outEv;
        proc.transport = g_instrument ? &transport : nullptr;
        if (plugin->process(plugin, &proc) == CLAP_PROCESS_ERROR) {
            std::fprintf(stderr, "process error\n");
            return false;
        }
    }
    if (!collect.ordered)
        std::fprintf(stderr, "the CLAP sent MIDI out of time order\n");
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    entry->deinit();
    dlclose(lib);
    return true;
}

// ------------------------------------------------------------------------------------------------
// a Rack module, linked in (module_compare only)

#ifdef NULL_MODULE_TEST
#include "NullModules.hpp"

static bool renderModule(const std::string& id, double sr, const std::vector<double>& values, const Stereo& in,
                         Channels& out)
{
    const nulljsfx::ModuleDesc* desc = nullptr;
    for (uint32_t i = 0; i < nullrack::kNumModules; ++i)
        if (id == nullrack::kModules[i]->id)
            desc = nullrack::kModules[i];
    if (!desc) {
        std::fprintf(stderr, "no Rack module named %s\n", id.c_str());
        return false;
    }
    if (desc->numSliders != values.size()) {
        std::fprintf(stderr, "module %s has %u sliders, the JSFX %zu (without file sliders)\n", id.c_str(), desc->numSliders, values.size());
        return false;
    }
    nulljsfx::Module* m = desc->create();
    // like the CLAP test: parameters (as float) first, then the sample rate (activate)
    for (uint32_t k = 0; k < values.size(); ++k)
        m->setValue(k, float(values[k]));
    m->setSampleRate(sr);
    const nulljsfx::ModuleTime time; // no transport, like the CLAP test host
    const uint32_t frames = uint32_t(in.l.size());
    for (uint32_t pos = 0; pos < frames; pos += kBlock) {
        const uint32_t n = std::min(kBlock, frames - pos);
        m->process(in.l.data() + pos, in.r.data() + pos, out[0].data() + pos, out[1].data() + pos, n, time);
    }
    delete m;
    return true;
}
#endif

// ------------------------------------------------------------------------------------------------

// the presets of a REAPER preset library (.rpl) as written by tools/build_presets.py: per preset
// its name and the value of every slider of sl ('-' in the library keeps the default)
static bool readRpl(const char* path, const std::vector<Slider>& sl, std::vector<std::vector<double>>& sets,
                    std::vector<std::string>& names)
{
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return false;
    }
    std::string text;
    char buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;)
        text.append(buf, n);
    std::fclose(f);

    auto decode = [](const std::string& b64) {
        std::string out;
        uint32_t acc = 0;
        int bits = 0;
        for (char c : b64) {
            const char* tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            const char* p = std::strchr(tab, c);
            if (!p || !c)
                continue;
            acc = (acc << 6) | uint32_t(p - tab);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out.push_back(char((acc >> bits) & 0xff));
            }
        }
        return out;
    };

    size_t pos = 0;
    while ((pos = text.find("<PRESET `", pos)) != std::string::npos) {
        const size_t q = text.find('`', pos + 9);
        names.push_back(text.substr(pos + 9, q - pos - 9));
        const size_t body = text.find('\n', q) + 1, end = text.find("\n  >", body);
        std::string state = decode(text.substr(body, end - body));
        state = state.substr(0, state.find('\0'));
        std::vector<std::string> tok;
        for (size_t a = 0; a < state.size() && state[a] != '"';) {
            size_t b = state.find(' ', a);
            if (b == std::string::npos)
                b = state.size();
            tok.push_back(state.substr(a, b - a));
            a = b + 1;
        }
        std::vector<double> set;
        for (const Slider& s : sl)
            set.push_back(s.index < tok.size() && tok[s.index] != "-" ? std::atof(tok[s.index].c_str()) : s.def);
        sets.push_back(set);
        pos = end;
    }
    if (sets.empty())
        std::fprintf(stderr, "no presets in %s\n", path);
    return !sets.empty();
}

static double rmsDb(const std::vector<float>& l, const std::vector<float>& r)
{
    double e = 0.0;
    for (size_t i = 0; i < l.size(); ++i)
        e += double(l[i]) * l[i] + double(r[i]) * r[i];
    return 10.0 * std::log10(std::max(e / double(2 * l.size()), 1e-20));
}

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
    const char* rpl = nullptr;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--tol"))
            tol = std::atof(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--rpl"))
            rpl = argv[i + 1];
        else if (!std::strcmp(argv[i], "--values")) {
            std::string v = argv[i + 1];
            for (size_t p = 0; p < v.size();) {
                size_t q = v.find(',', p);
                values.push_back(std::atof(v.substr(p, q - p).c_str()));
                p = q == std::string::npos ? v.size() : q + 1;
            }
        }
    }
    std::vector<Slider> sl;
    uint32_t numOuts = 2;
    if (!sliders(ref, sl, &numOuts) || sl.empty())
        return 1;

    // the settings to test, each with a label
    std::vector<std::vector<double>> sets;
    std::vector<std::string> labels;
    std::vector<double> defs;
    for (const Slider& s : sl)
        defs.push_back(s.def);
    char buf[160];
    if (rpl) {
        if (!readRpl(rpl, sl, sets, labels))
            return 1;
    } else if (sl.size() == 1) {
        const Slider& r = sl[0];
        if (values.empty()) {
            values = { r.min, r.max, r.def, r.min + (r.max - r.min) * 0.25, r.min + (r.max - r.min) * 0.5,
                       r.min + (r.max - r.min) * 0.75 };
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
        }
        for (double v : values) {
            sets.push_back({ v });
            std::snprintf(buf, sizeof(buf), "value %8.2f", v);
            labels.push_back(buf);
        }
    } else {
        sets.push_back(defs);
        labels.push_back("all defaults");
        for (size_t k = 0; k < sl.size(); ++k)
            for (double v : { sl[k].min, sl[k].max }) {
                if (v == sl[k].def || sl[k].path)
                    continue;
                std::vector<double> set = defs;
                set[k] = v;
                sets.push_back(set);
                std::snprintf(buf, sizeof(buf), "%s = %g", sl[k].name.c_str(), v);
                labels.push_back(buf);
            }
        uint32_t seed = 4321;
        for (int r = 0; r < 6; ++r) {
            std::vector<double> set;
            for (const Slider& s : sl) {
                if (s.path) {
                    set.push_back(s.def);
                    continue;
                }
                seed = seed * 1664525u + 1013904223u;
                double v = s.min + (s.max - s.min) * ((seed >> 8) / 16777216.0);
                if (s.inc > 0)
                    v = std::fmin(s.max, s.min + std::round((v - s.min) / s.inc) * s.inc);
                set.push_back(v);
            }
            sets.push_back(set);
            labels.push_back("random #" + std::to_string(r + 1));
        }
    }

    const bool isJsfx = endsWith(other, ".jsfx");
    bool ok = true, first = true;
    for (double sr : { 44100.0, 96000.0 }) {
        const Stereo in = makeSignal(sr), sc = makeSidechain(sr);
        for (size_t i = 0; i < sets.size(); ++i) {
            Channels a(numOuts, std::vector<float>(in.l.size())), b = a;
            // the CLAP gets every slider's value except the file sliders'
            std::vector<double> params;
            for (size_t k = 0; k < sl.size(); ++k)
                if (!sl[k].path)
                    params.push_back(sets[i][k]);
            // a Rack module has no sidechain bus: its JSFX hears the track on pins 3/4 as well
            const bool toModule = other.compare(0, 7, "module:") == 0;
            std::vector<MidiMsg> ma, mb;
            if (!renderJsfx(ref, sr, sl, sets[i], in, toModule ? in : sc, a, &ma))
                return 1;
            bool rendered;
            if (isJsfx)
                rendered = renderJsfx(other.c_str(), sr, sl, sets[i], in, sc, b, &mb);
#ifdef NULL_MODULE_TEST
            else if (other.compare(0, 7, "module:") == 0)
                rendered = renderModule(other.substr(7), sr, params, in, b);
#endif
            else
                rendered = renderClap(other.c_str(), sr, params, in, sc, b, first, &mb);
            if (!rendered)
                return 1;
            first = false;
            size_t at = 0, ch = 0;
            const double d = maxDiff(a, b, &at, &ch);
            bool pass = d <= tol && std::isfinite(d);
            std::string level;
            if (g_midifx) {
                // the MIDI out: the same messages at the same samples, in the same order
                size_t same = 0;
                while (same < ma.size() && same < mb.size() && ma[same].at == mb[same].at && ma[same].st == mb[same].st
                       && ma[same].d1 == mb[same].d1 && (ma[same].size < 3 || ma[same].d2 == mb[same].d2) && ma[same].size == mb[same].size)
                    ++same;
                const bool midiOk = same == ma.size() && same == mb.size() && !ma.empty();
                std::snprintf(buf, sizeof(buf), "  midi %zu/%zu %s", ma.size(), mb.size(), midiOk ? "same" : "DIFFER");
                level = buf;
                if (!midiOk) {
                    if (same < ma.size())
                        std::printf("          first difference, message %zu: ref %u %02X %u %u", same, ma[same].at, ma[same].st, ma[same].d1, ma[same].d2);
                    if (same < mb.size())
                        std::printf("  other %u %02X %u %u", mb[same].at, mb[same].st, mb[same].d1, mb[same].d2);
                    std::printf("\n");
                }
                pass = pass && midiOk;
            }
            if (rpl && !g_midifx) {
                std::snprintf(buf, sizeof(buf), "  level %+6.1f dB", rmsDb(a[0], a[numOuts > 1 ? 1 : 0]) - rmsDb(in.l, in.r));
                level = buf;
            }
            std::printf("%6.0f Hz  %-28s max |diff| = %.3g%s%s\n", sr, labels[i].c_str(), d, level.c_str(),
                        pass ? "" : (" at sample " + std::to_string(at) + (numOuts > 2 ? " (output " + std::to_string(ch + 1) + ")" : "") + "  FAIL").c_str());
            if (!pass && sl.size() > 1) {
                std::string vals;
                for (size_t k = 0; k < sl.size(); ++k)
                    vals += (k ? ", " : "") + sl[k].name + "=" + std::to_string(sets[i][k]);
                std::printf("          settings: %s\n", vals.c_str());
            }
            ok = ok && pass;
        }
    }
    std::printf("%s (tolerance %.0e)\n", ok ? "PASS" : "FAIL", tol);
    return ok ? 0 : 1;
}
