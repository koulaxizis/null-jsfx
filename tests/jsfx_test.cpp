// Render test for NULL JSFX plugins, built on ysfx (the EEL2/JSFX engine used by REAPER-compatible hosts).
// For every .jsfx given on the command line: compile, then render test audio at several sample rates
// and slider settings, and report errors (compile failure, NaN/Inf, blow-ups, silence).
// Instruments (no input pins) also get MIDI notes over the General MIDI drum map, and the host
// transport plays at 120 bpm as for every plugin. MIDI effects (//category: MIDI) get notes with
// their note-offs instead: their audio must pass through unchanged (or delayed by the latency
// they report), they must send MIDI, and every
// note they start must be ended again (note-offs match note-ons) by the end, after an all notes
// off on both channels in the last block, as a host sends at stop.
// Exit code is the number of plugins that failed.
#include "ysfx.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <random>

static std::string g_log;
static void reporter(intptr_t, ysfx_log_level level, const char *msg)
{
    if (level == ysfx_log_error || level == ysfx_log_warning) {
        g_log += ysfx_log_level_string(level);
        g_log += ": ";
        g_log += msg;
        g_log += "\n";
    }
}

struct Result {
    bool fail = false;
    std::vector<std::string> errors, warnings;
};

enum Preset { DEFAULTS, MINS, MAXS, RANDOM };

static void apply_preset(ysfx_t *fx, Preset p, std::mt19937 &rng)
{
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i) {
        if (!ysfx_slider_exists(fx, i))
            continue;
        ysfx_slider_range_t r;
        ysfx_slider_get_range(fx, i, &r);
        double v = r.def;
        if (p == MINS) v = r.min;
        else if (p == MAXS) v = r.max;
        else if (p == RANDOM) {
            std::uniform_real_distribution<double> d(r.min, r.max);
            v = d(rng);
            if (r.inc > 0) v = r.min + std::round((v - r.min) / r.inc) * r.inc;
        }
        ysfx_slider_set_value(fx, i, v);
    }
}

// Renders `seconds` of a stereo test signal; returns output peak and RMS per channel.
struct Render { double peak = 0, rms[2] = {0, 0}, diff = 0; bool nonfinite = false; size_t midiSent = 0; int unbalanced = 0; };

// meters with no listening option: the audio passes through untouched by design
static bool is_pure_meter(const char *path)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    return !strncmp(base, "multichannel_loudness", 21);
}

static bool is_midi_effect(const char *path)
{
    FILE *f = fopen(path, "rb");
    char line[256];
    bool yes = false;
    for (int i = 0; f && i < 6 && fgets(line, sizeof line, f); ++i)
        if (!strncmp(line, "//category: MIDI", 16)) yes = true;
    if (f) fclose(f);
    return yes;
}

static Render render(ysfx_t *fx, double sr, double seconds, int signal, bool midifx = false)
{
    const uint32_t block = 256;
    std::vector<double> inL(block), inR(block), outL(block), outR(block), sc(block);
    // plugins with sidechain pins (inputs 3/4) get a kick-like pulse there: 60 Hz, 4 hits per second
    const uint32_t nin = ysfx_get_num_inputs(fx) > 2 ? 4 : 2;
    const double *ins[4] = {inL.data(), inR.data(), sc.data(), sc.data()};
    double *outs[2] = {outL.data(), outR.data()};
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
    // instruments: a note every 1/8 s, cycling through the GM drum map with varied velocities
    const bool instrument = ysfx_get_num_inputs(fx) == 0;
    static const uint8_t notes[] = {36, 42, 38, 46, 39, 37, 41, 47, 56, 49, 35, 44, 40, 45, 48, 51};
    const uint64_t every = (uint64_t)(sr / 8);
    uint8_t msgs[64][3];
    // MIDI effects: a note every 1/8 s (pitches 48..83, two channels), ended 1/16 s later; what
    // they send is counted per channel and note (+1 a note-on, -1 a note-off)
    std::vector<int> held(16 * 128, 0);
    std::vector<double> histL, histR; // a MIDI effect's input so far (for its latency)
    Render res;
    double acc[2] = {0, 0}, dacc = 0;
    uint64_t n = 0, total = (uint64_t)(sr * seconds);
    std::mt19937 rng(1);
    std::uniform_real_distribution<double> noise(-1, 1);
    while (n < total) {
        for (uint32_t i = 0; i < block; ++i) {
            double t = (double)(n + i) / sr;
            double env = (std::fmod(t, 0.5) < 0.25) ? 1.0 : 0.3; // some dynamics for gates/compressors
            double l = 0.25 * env * (std::sin(2 * M_PI * 220 * t) + 0.3 * noise(rng));
            double r = 0.25 * env * (std::sin(2 * M_PI * 330 * t) + 0.3 * noise(rng));
            inL[i] = (signal == 2) ? 0 : l;  // signal 1: left only ; 2: right only
            inR[i] = (signal == 1) ? 0 : r;
            double tk = std::fmod(t, 0.25);
            sc[i] = 0.8 * std::exp(-tk * 20) * std::sin(2 * M_PI * 60 * tk);
        }
        ti.time_position = n / sr;
        ti.beat_position = ti.time_position * 2;
        ysfx_set_time_info(fx, &ti);
        if (instrument) {
            int m = 0;
            for (uint64_t k = (n + every - 1) / every; k * every < n + block && m < 64; ++k, ++m) {
                msgs[m][0] = 0x99;
                msgs[m][1] = notes[k % 16];
                msgs[m][2] = (uint8_t)(40 + (k * 37) % 88);
                ysfx_midi_event_t ev = {0, (uint32_t)(k * every - n), 3, msgs[m]};
                ysfx_send_midi(fx, &ev);
            }
        }
        if (midifx) {
            int m = 0;
            for (uint64_t k = n / (every / 2); k * (every / 2) < n + block && m < 64; ++k) {
                const uint64_t at = k * (every / 2), on = k / 2;
                if (at < n || at + every / 2 >= total + every / 2) continue;
                if (k % 2 == 0 && on * every + every / 2 >= total) continue; // no room for its note-off
                msgs[m][0] = (uint8_t)((k % 2 ? 0x80 : 0x90) | (on % 3 == 2 ? 1 : 0));
                msgs[m][1] = (uint8_t)(48 + (on * 7) % 36);
                msgs[m][2] = (uint8_t)(k % 2 ? 0 : 30 + (on * 37) % 97);
                ysfx_midi_event_t ev = {0, (uint32_t)(at - n), 3, msgs[m]};
                ysfx_send_midi(fx, &ev);
                ++m;
            }
            // the last block: all notes off on both channels, as a host sends at stop (a latched
            // arpeggio or a note still waiting must end too). It comes at the block's last sample,
            // after this block's notes: a host hands a plugin its events in time order, and no
            // note follows the stop
            if (n + block >= total) {
                static const uint8_t off[2][3] = {{0xB0, 123, 0}, {0xB1, 123, 0}};
                for (int c = 0; c < 2; ++c) {
                    ysfx_midi_event_t ev = {0, block - 1, 3, off[c]};
                    ysfx_send_midi(fx, &ev);
                }
            }
        }
        ysfx_process_double(fx, ins, outs, nin, 2, block);
        ysfx_midi_event_t me;
        while (ysfx_receive_midi(fx, &me)) {
            ++res.midiSent;
            if (me.size < 3) continue;
            const int st = me.data[0] & 0xF0, key = (me.data[0] & 0x0F) * 128 + me.data[1];
            if (st == 0x90 && me.data[2] > 0) held[key] += 1;
            else if (st == 0x80 || st == 0x90) { held[key] -= 1; if (held[key] < 0) res.unbalanced += 1; }
        }
        // a MIDI effect's audio may pass through delayed by its reported latency (pdc_delay),
        // in line with its MIDI: it is compared with the input that much earlier
        if (midifx) { histL.insert(histL.end(), inL.begin(), inL.end()); histR.insert(histR.end(), inR.begin(), inR.end()); }
        const uint64_t pdc = midifx ? (uint64_t)std::max(0.0, (double)ysfx_get_pdc_delay(fx)) : 0;
        for (uint32_t i = 0; i < block; ++i) {
            double o[2] = {outL[i], outR[i]};
            for (int c = 0; c < 2; ++c) {
                if (!std::isfinite(o[c])) { res.nonfinite = true; continue; }
                res.peak = std::fmax(res.peak, std::fabs(o[c]));
                acc[c] += o[c] * o[c];
                const uint64_t at = n + i;
                const double ref = !midifx ? (c ? inR[i] : inL[i]) : at < pdc ? 0.0 : (c ? histR[at - pdc] : histL[at - pdc]);
                double d = o[c] - ref;
                dacc += d * d;
            }
        }
        n += block;
    }
    for (int h : held) res.unbalanced += h != 0;
    res.rms[0] = std::sqrt(acc[0] / n);
    res.rms[1] = std::sqrt(acc[1] / n);
    res.diff = std::sqrt(dacc / (2 * n));
    return res;
}

static std::string fmt(double v)
{
    char b[32];
    snprintf(b, sizeof b, "%.3g", v);
    return b;
}

static Result test_plugin(const char *path)
{
    Result res;
    // NULL_TEST_RATES="8000,22050,..." replaces the default rates (CI also runs the odd ones)
    static std::vector<double> rates = [] {
        std::vector<double> v;
        const char *env = getenv("NULL_TEST_RATES");
        for (const char *p = env; p && *p;) {
            char *end;
            double r = strtod(p, &end);
            if (end == p) break;
            if (r > 0) v.push_back(r);
            p = *end == ',' ? end + 1 : end;
        }
        if (v.empty()) v = {44100, 48000, 96000};
        return v;
    }();
    static const char *pname[] = {"defaults", "min", "max", "random"};
    bool changes_sound = false;
    const bool midifx = is_midi_effect(path);
    for (double sr : rates) {
        std::mt19937 rng(42);
        for (int p = DEFAULTS; p <= RANDOM + 2; ++p) {
            Preset preset = (Preset)std::min(p, (int)RANDOM);
            ysfx_config_t *cfg = ysfx_config_new();
            ysfx_guess_file_roots(cfg, path);
            ysfx_set_log_reporter(cfg, &reporter);
            ysfx_t *fx = ysfx_new(cfg);
            ysfx_config_free(cfg);
            g_log.clear();
            if (!ysfx_load_file(fx, path, 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
                res.fail = true;
                res.errors.push_back("does not compile:\n" + g_log);
                ysfx_free(fx);
                return res;
            }
            ysfx_set_sample_rate(fx, sr);
            ysfx_set_block_size(fx, 256);
            ysfx_init(fx);
            apply_preset(fx, preset, rng);
            char where[96];
            snprintf(where, sizeof where, "%s @ %.6g Hz", pname[preset], sr);
            Render r = render(fx, sr, 2.0, 0, midifx);
            if (r.diff > 1e-4) changes_sound = true;
            if (midifx) {
                // a MIDI effect: the audio passes through, MIDI goes out, every note started is ended
                if (r.diff != 0) { res.fail = true; res.errors.push_back(std::string("a MIDI effect changes the audio (") + where + ")"); }
                if (preset == DEFAULTS && r.midiSent == 0) { res.fail = true; res.errors.push_back(std::string("a MIDI effect sends no MIDI (") + where + ")"); }
                if (r.unbalanced) { res.fail = true; res.errors.push_back(std::string("note-offs do not match note-ons: ") + std::to_string(r.unbalanced) + " notes (" + where + ")"); }
            }
            if (r.nonfinite) { res.fail = true; res.errors.push_back(std::string("NaN/Inf output (") + where + ")"); }
            if (r.peak > 100) { res.fail = true; res.errors.push_back(std::string("output blows up, peak ") + fmt(r.peak) + " (" + where + ")"); }
            if (preset == DEFAULTS && r.rms[0] < 1e-6 && r.rms[1] < 1e-6) { res.fail = true; res.errors.push_back(std::string("silent output (") + where + ")"); }
            else if (preset == DEFAULTS && (r.rms[0] < 1e-6 || r.rms[1] < 1e-6)) res.warnings.push_back(std::string("one channel silent (") + where + ")");
            if (preset == DEFAULTS && sr == 48000 && ysfx_get_num_inputs(fx) > 0) {
                // channel separation: feed one side only and look at the other side
                ysfx_init(fx);
                Render lo = render(fx, sr, 1.0, 1);
                ysfx_init(fx);
                Render ro = render(fx, sr, 1.0, 2);
                if (lo.rms[0] > 1e-6 && lo.rms[1] > 0.5 * lo.rms[0]) res.warnings.push_back("left input leaks to right output (ok for stereo/mono effects)");
                if (ro.rms[1] > 1e-6 && ro.rms[0] > 0.5 * ro.rms[1]) res.warnings.push_back("right input leaks to left output (ok for stereo/mono effects)");
                if (lo.rms[0] < 1e-6 && lo.rms[1] < 1e-6) res.warnings.push_back("no output with left-only input");
            }
            ysfx_free(fx);
        }
    }
    if (!changes_sound && !midifx && !is_pure_meter(path)) { res.fail = true; res.errors.push_back("output always equals input: the plugin has no effect"); }
    return res;
}

int main(int argc, char **argv)
{
    int failed = 0;
    for (int i = 1; i < argc; ++i) {
        Result r = test_plugin(argv[i]);
        std::string name = argv[i];
        name = name.substr(name.find_last_of('/') + 1);
        // de-duplicate messages
        std::vector<std::string> seen;
        auto print = [&](const std::vector<std::string> &v, const char *tag) {
            for (auto &m : v) {
                bool dup = false;
                for (auto &s : seen) dup |= (s == m);
                if (dup) continue;
                seen.push_back(m);
                printf("    %s %s\n", tag, m.c_str());
            }
        };
        printf("%s %s\n", r.fail ? "FAIL" : (r.warnings.empty() ? "ok  " : "warn"), name.c_str());
        print(r.errors, "error:");
        print(r.warnings, "note: ");
        failed += r.fail;
    }
    printf("\n%d of %d plugins failed\n", failed, argc - 1);
    return failed;
}
