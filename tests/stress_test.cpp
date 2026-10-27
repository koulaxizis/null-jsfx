// Stress test for NULL JSFX plugins, built on ysfx like jsfx_test. Three checks that the render
// test (jsfx_test) does not make:
//   1. numerics: hard signals (a +12 dBFS square with DC, sines near Nyquist, a tone then 3.5 s of
//      silence, DC alone, and music while every slider jumps to a random value every 50 ms) with
//      default, minimum, maximum and random settings, at 44.1 and 192 kHz. Fails on NaN/Inf or
//      an output above 1e4.
//   2. memory: REAPER gives a JSFX 8M memory slots unless it sets options:maxmem. Each plugin is
//      rendered for 5 s at 384 kHz (default and maximum settings) as it is and as a copy that asks for
//      32M slots; the two must match. A difference means the plugin reads or writes past its
//      memory at high sample rates, where EEL2 silently reads 0 and drops writes.
//   3. slider cost: at 48 kHz with 256-sample blocks, every slider is set to 3 random values and
//      the block after each change is timed (it runs @slider). Fails when the median of a
//      slider's 3 blocks is over the block's real-time budget (5.33 ms); notes it above half.
// MIDI effects are skipped (jsfx_test covers them). Instruments get notes, and the tone-then-
// silence signal ends with an all notes off. Tails that do not decay (generators, freeze, long
// releases, patterns) are by design and not checked.
// Usage: stress_test [--no-numerics] [--no-memory] [--no-cost] plugin.jsfx ...
// Exit code is the number of plugins that failed.
#include "ysfx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

static void reporter(intptr_t, ysfx_log_level, const char *) {}

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

static ysfx_t *load(const char *path, double sr)
{
    ysfx_config_t *cfg = ysfx_config_new();
    ysfx_guess_file_roots(cfg, path);
    ysfx_set_log_reporter(cfg, &reporter);
    ysfx_t *fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, path, 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
        ysfx_free(fx);
        return nullptr;
    }
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, 256);
    ysfx_init(fx);
    return fx;
}

enum Preset { DEFAULTS, MINS, MAXS, RANDOM };
static const char *kPreset[] = {"defaults", "min", "max", "random"};

static void apply_preset(ysfx_t *fx, int p, std::mt19937 &rng)
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

// ---- 1. numerics ------------------------------------------------------------------------------

enum Signal { HOT, NYQUIST, SILENCE, AUTOMATION, DC, NUM_SIGNALS };
static const char *kSignal[] = {"+12 dBFS square with DC", "sines near Nyquist", "tone then silence",
                                "slider automation", "DC"};

struct Render { bool nonfinite = false; double nonfiniteAt = 0, peak = 0; };

static Render render(ysfx_t *fx, double sr, int sig, std::mt19937 &rng)
{
    const uint32_t block = 256;
    std::vector<double> inL(block), inR(block), outL(block), outR(block), sc(block);
    const uint32_t nin = ysfx_get_num_inputs(fx) > 2 ? 4 : 2;
    const double *ins[4] = {inL.data(), inR.data(), sc.data(), sc.data()};
    double *outs[2] = {outL.data(), outR.data()};
    const bool instrument = ysfx_get_num_inputs(fx) == 0;
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
    const uint64_t total = (uint64_t)(sr * (sig == SILENCE ? 4.0 : 2.0)), every = (uint64_t)(sr / 8);
    const uint64_t silence = (uint64_t)(sr * 0.5), jump = (uint64_t)(sr * 0.05);
    std::uniform_real_distribution<double> noise(-1, 1);
    uint8_t msgs[64][3];
    Render res;
    for (uint64_t n = 0; n < total; n += block) {
        for (uint32_t i = 0; i < block; ++i) {
            const double t = (double)(n + i) / sr;
            double l = 0, r = 0;
            if (sig == HOT) { l = 4.0 * (std::fmod(t * 100, 1) < 0.5 ? 1 : -1) + 0.5; r = -l; }
            else if (sig == NYQUIST) { l = 0.9 * std::sin(2 * M_PI * 0.45 * sr * t); r = 0.9 * std::sin(2 * M_PI * 0.49 * sr * t); }
            else if (sig == SILENCE) { l = r = n + i < silence ? 0.5 * std::sin(2 * M_PI * 440 * t) : 0.0; }
            else if (sig == AUTOMATION) { l = 0.3 * (std::sin(2 * M_PI * 220 * t) + 0.3 * noise(rng)); r = 0.3 * (std::sin(2 * M_PI * 330 * t) + 0.3 * noise(rng)); }
            else { l = r = 0.5; }
            inL[i] = l;
            inR[i] = r;
            const double tk = std::fmod(t, 0.25);
            sc[i] = sig == SILENCE && n + i >= silence ? 0.0 : 0.8 * std::exp(-tk * 20) * std::sin(2 * M_PI * 60 * tk);
        }
        // a new random value for every slider, every 50 ms (each block crossing a 50 ms mark)
        if (sig == AUTOMATION && n / jump != (n + block) / jump)
            apply_preset(fx, RANDOM, rng);
        ti.time_position = n / sr;
        ti.beat_position = ti.time_position * 2;
        ysfx_set_time_info(fx, &ti);
        if (instrument) {
            int m = 0;
            if (sig != SILENCE || n < silence)
                for (uint64_t k = (n + every - 1) / every; k * every < n + block && m < 63; ++k, ++m) {
                    msgs[m][0] = 0x99;
                    msgs[m][1] = (uint8_t)(36 + (k * 5) % 40);
                    msgs[m][2] = 127;
                    ysfx_midi_event_t ev = {0, (uint32_t)(k * every - n), 3, msgs[m]};
                    ysfx_send_midi(fx, &ev);
                }
            // the first block past the tone: all notes off on both channels used
            if (sig == SILENCE && n >= silence && n < silence + block) {
                static const uint8_t off[2][3] = {{0xB9, 123, 0}, {0xB0, 123, 0}};
                for (int c = 0; c < 2; ++c) {
                    ysfx_midi_event_t ev = {0, 0, 3, off[c]};
                    ysfx_send_midi(fx, &ev);
                }
            }
        }
        ysfx_process_double(fx, ins, outs, nin, 2, block);
        ysfx_midi_event_t me;
        while (ysfx_receive_midi(fx, &me)) {}
        for (uint32_t i = 0; i < block; ++i)
            for (double v : {outL[i], outR[i]}) {
                if (!std::isfinite(v)) {
                    if (!res.nonfinite) res.nonfiniteAt = (n + i) / sr;
                    res.nonfinite = true;
                    continue;
                }
                res.peak = std::fmax(res.peak, std::fabs(v));
            }
    }
    return res;
}

static void check_numerics(const char *path, std::vector<std::string> &errors)
{
    for (double sr : {44100.0, 192000.0})
        for (int sig = 0; sig < NUM_SIGNALS; ++sig)
            for (int p = DEFAULTS; p <= (sig == AUTOMATION ? DEFAULTS : RANDOM); ++p) {
                ysfx_t *fx = load(path, sr);
                if (!fx) { errors.push_back("does not compile"); return; }
                // instruments ignore their (absent) input: one setting is enough for the input signals
                if (ysfx_get_num_inputs(fx) == 0 && p != DEFAULTS && sig != SILENCE) { ysfx_free(fx); continue; }
                std::mt19937 rng(7 + p);
                apply_preset(fx, p, rng);
                const Render r = render(fx, sr, sig, rng);
                ysfx_free(fx);
                char where[128];
                snprintf(where, sizeof where, "%s, %s, %.0f Hz", kSignal[sig], kPreset[p], sr);
                if (r.nonfinite) { char m[200]; snprintf(m, sizeof m, "NaN/Inf output (%s), first at %.3f s", where, r.nonfiniteAt); errors.push_back(m); }
                if (r.peak > 1e4) { char m[200]; snprintf(m, sizeof m, "output blows up, peak %.3g (%s)", r.peak, where); errors.push_back(m); }
            }
}

// ---- 2. memory --------------------------------------------------------------------------------

// a copy of the plugin that asks for 32M memory slots
static std::string copy_with_32m(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream out;
    std::string line;
    bool done = false;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (!line.compare(0, 8, "options:") && line.find("maxmem=") != std::string::npos) {
            const size_t a = line.find("maxmem=") + 7, b = line.find_first_of(" \t\r", a);
            line = line.substr(0, a) + "33554432" + (b == std::string::npos ? "" : line.substr(b));
            done = true;
        }
        out << line << "\n";
        if (n == 5 && !done) { out << "options:maxmem=33554432\n"; done = true; }
    }
    // in the temp folder: the plain plugins import nothing
    std::string name = path;
    name = name.substr(name.find_last_of('/') + 1);
    const char *tmp = getenv("TMPDIR");
    const std::string p = std::string(tmp && *tmp ? tmp : "/tmp") + "/" + std::to_string(getpid()) + "_" + name;
    std::ofstream(p, std::ios::binary) << out.str();
    return p;
}

static std::vector<double> render_memory(const char *path, double sr, int p)
{
    std::vector<double> out;
    ysfx_t *fx = load(path, sr);
    if (!fx) return out;
    std::mt19937 rng(3);
    apply_preset(fx, p, rng);
    const uint32_t block = 256;
    std::vector<double> inL(block), inR(block), outL(block), outR(block);
    const uint32_t nin = std::min(ysfx_get_num_inputs(fx), 4u);
    const double *ins[4] = {inL.data(), inR.data(), inL.data(), inR.data()};
    double *outs[2] = {outL.data(), outR.data()};
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
    std::uniform_real_distribution<double> noise(-1, 1);
    const uint64_t every = (uint64_t)(sr / 8);
    uint8_t msgs[64][3];
    for (uint64_t n = 0; n < (uint64_t)(sr * 5); n += block) { // 5 s: more than a bar at 120 bpm
        for (uint32_t i = 0; i < block; ++i) {
            const double t = (n + i) / sr;
            inL[i] = 0.3 * std::sin(2 * M_PI * 220 * t) + 0.1 * noise(rng);
            inR[i] = 0.3 * std::sin(2 * M_PI * 331 * t) + 0.1 * noise(rng);
        }
        ti.time_position = n / sr;
        ti.beat_position = ti.time_position * 2;
        ysfx_set_time_info(fx, &ti);
        int m = 0;
        for (uint64_t k = (n + every - 1) / every; k * every < n + block && m < 64; ++k, ++m) {
            msgs[m][0] = (uint8_t)(0x90 | (k % 2 ? 9 : 0));
            msgs[m][1] = (uint8_t)(36 + (k * 5) % 40);
            msgs[m][2] = 100;
            ysfx_midi_event_t ev = {0, (uint32_t)(k * every - n), 3, msgs[m]};
            ysfx_send_midi(fx, &ev);
        }
        ysfx_process_double(fx, ins, outs, nin, 2, block);
        ysfx_midi_event_t me;
        while (ysfx_receive_midi(fx, &me)) {}
        for (uint32_t i = 0; i < block; ++i) { out.push_back(outL[i]); out.push_back(outR[i]); }
    }
    ysfx_free(fx);
    return out;
}

static void check_memory(const char *path, std::vector<std::string> &errors)
{
    const std::string big = copy_with_32m(path);
    const double sr = 384000;
    for (int p : {DEFAULTS, MAXS}) {
        const std::vector<double> a = render_memory(path, sr, p), b = render_memory(big.c_str(), sr, p);
        double d = a.size() == b.size() ? 0.0 : 1e9;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            const bool fa = std::isfinite(a[i]), fb = std::isfinite(b[i]);
            if (fa && fb) d = std::fmax(d, std::fabs(a[i] - b[i]));
            else if (fa != fb) d = 1e9;
        }
        if (d > 1e-9) {
            char m[200];
            snprintf(m, sizeof m, "uses memory past REAPER's default 8M slots at %.0f Hz (%s): differs by %.3g from the same plugin with options:maxmem", sr, kPreset[p], d);
            errors.push_back(m);
        }
    }
    unlink(big.c_str());
}

// ---- 3. slider cost ---------------------------------------------------------------------------

static void check_cost(const char *path, std::vector<std::string> &errors, std::vector<std::string> &notes)
{
    const double sr = 48000, budget = 256 / sr * 1000;
    const uint32_t block = 256;
    ysfx_t *fx = load(path, sr);
    if (!fx) { errors.push_back("does not compile"); return; }
    std::vector<double> inL(block), inR(block), outL(block), outR(block);
    const uint32_t nin = std::min(ysfx_get_num_inputs(fx), 4u);
    const double *ins[4] = {inL.data(), inR.data(), inL.data(), inR.data()};
    double *outs[2] = {outL.data(), outR.data()};
    std::mt19937 rng(1);
    std::uniform_real_distribution<double> noise(-0.3, 0.3);
    auto run_block = [&]() {
        for (uint32_t i = 0; i < block; ++i) { inL[i] = noise(rng); inR[i] = noise(rng); }
        const auto t0 = std::chrono::steady_clock::now();
        ysfx_process_double(fx, ins, outs, nin, 2, block);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ysfx_midi_event_t me;
        while (ysfx_receive_midi(fx, &me)) {}
        return ms;
    };
    for (int i = 0; i < 200; ++i) run_block();
    double worst = 0;
    std::string worstName;
    for (uint32_t s = 0; s < ysfx_max_sliders; ++s) {
        if (!ysfx_slider_exists(fx, s))
            continue;
        ysfx_slider_range_t r;
        ysfx_slider_get_range(fx, s, &r);
        std::vector<double> t;
        for (int k = 0; k < 3; ++k) {
            std::uniform_real_distribution<double> d(r.min, r.max);
            double v = d(rng);
            if (r.inc > 0) v = r.min + std::round((v - r.min) / r.inc) * r.inc;
            ysfx_slider_set_value(fx, s, v);
            t.push_back(run_block());
            run_block();
        }
        std::sort(t.begin(), t.end());
        if (t[1] > worst) {
            worst = t[1];
            const char *name = ysfx_slider_get_name(fx, s);
            worstName = name ? name : "?";
        }
    }
    ysfx_free(fx);
    char m[200];
    snprintf(m, sizeof m, "a change of \"%s\" costs %.2f ms in the audio thread (one 256-sample block at 48 kHz has %.2f ms)",
             worstName.c_str(), worst, budget);
    if (worst > budget) errors.push_back(m);
    else if (worst > budget / 2) notes.push_back(m);
}

int main(int argc, char **argv)
{
    bool numerics = true, memory = true, cost = true;
    int failed = 0, count = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-numerics")) { numerics = false; continue; }
        if (!strcmp(argv[i], "--no-memory")) { memory = false; continue; }
        if (!strcmp(argv[i], "--no-cost")) { cost = false; continue; }
        const char *path = argv[i];
        if (is_midi_effect(path))
            continue;
        ++count;
        std::vector<std::string> errors, notes;
        if (numerics) check_numerics(path, errors);
        if (memory) check_memory(path, errors);
        if (cost) check_cost(path, errors, notes);
        std::string name = path;
        name = name.substr(name.find_last_of('/') + 1);
        printf("%s %s\n", !errors.empty() ? "FAIL" : notes.empty() ? "ok  " : "warn", name.c_str());
        for (auto &e : errors) printf("    error: %s\n", e.c_str());
        for (auto &n : notes) printf("    note:  %s\n", n.c_str());
        fflush(stdout);
        failed += !errors.empty();
    }
    printf("\n%d of %d plugins failed\n", failed, count);
    return failed;
}
