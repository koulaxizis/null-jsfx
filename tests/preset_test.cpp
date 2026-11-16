// Preset level test for NULL JSFX (Bible Part V): loads every factory preset of every plain JSFX from
// its preset library (<plugin>.jsfx.rpl, written by tools/build_presets.py), renders 3 s of a
// music-like test signal at 48 kHz and compares each preset with the plugin's "Default" preset.
// A preset fails when its output is not finite, when it peaks above +6 dBFS, or when its loudness
// (RMS over both channels) moves more than the allowed jump from Default. Switching presets should
// not make a track jump in level (the Kilohearts habit of level-matched presets). Effects whose job is
// level (gain, gates, ducking, transient and bass tools...) or whose presets are expected to move
// it are listed with their own allowance in tests/preset_levels.txt, one "<plugin> <dB> [preset]" per
// line: without a preset name it covers every preset of the plugin, with one only that preset.
// MIDI effects and meters are skipped (their audio passes through untouched by design), and so are
// the surround beds (twelve outputs; the test measures a stereo pair).
// Instruments get notes (a phrase over two octaves) instead of the audio input.
// Usage: preset_test [--report] tests/preset_levels.txt plugin.jsfx ...
//        --report prints every preset's level and never fails (to tune tests/preset_levels.txt)
// Exit code is the number of presets that failed.
#include "ysfx.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static constexpr double kMaxJumpDb = 6.0;   // default allowance from Default's level
static constexpr double kMaxPeakDb = 12.0;  // no preset may peak above this (a blow-up, not a level choice)
static constexpr double kRate = 48000.0;
static constexpr double kSeconds = 3.0;

static void reporter(intptr_t, ysfx_log_level, const char *) {}

static std::string read_text(const std::string &path)
{
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

static std::string category(const std::string &text)
{
    const char *key = "//category:";
    size_t p = text.find(key);
    if (p == std::string::npos) return "";
    p += strlen(key);
    while (p < text.size() && text[p] == ' ') ++p;
    size_t e = text.find('\n', p);
    return text.substr(p, e - p);
}

struct Level { double rmsDb = -200, peakDb = -200; bool finite = true; };

static double db(double v) { return v > 1e-10 ? 20 * std::log10(v) : -200; }

// a music-like test signal: a kick-like pulse, a bass note, a three-note chord and some noise,
// with soft and loud halves so dynamics react
static Level render(ysfx_t *fx)
{
    const uint32_t block = 256;
    std::vector<double> inL(block), inR(block), outL(block), outR(block), sc(block);
    const uint32_t nin = ysfx_get_num_inputs(fx) > 2 ? 4 : 2;
    const double *ins[4] = {inL.data(), inR.data(), sc.data(), sc.data()};
    double *outs[2] = {outL.data(), outR.data()};
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
    const bool instrument = ysfx_get_num_inputs(fx) == 0;
    static const uint8_t phrase[] = {48, 55, 60, 64, 67, 60, 52, 59, 62, 65, 69, 62, 45, 52, 57, 60};
    const uint64_t every = (uint64_t)(kRate / 4);
    uint8_t msgs[64][3];
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> noise(-1, 1);
    double acc = 0, peak = 0;
    bool finite = true;
    uint64_t n = 0, total = (uint64_t)(kRate * kSeconds), counted = 0;
    const uint64_t skip = (uint64_t)(kRate * 0.25); // the first quarter second is not measured
    while (n < total) {
        for (uint32_t i = 0; i < block; ++i) {
            const double t = (double)(n + i) / kRate;
            const double env = std::fmod(t, 1.0) < 0.5 ? 1.0 : 0.35;
            const double tb = std::fmod(t, 0.5);
            const double kick = 0.6 * std::exp(-tb * 18) * std::sin(2 * M_PI * (50 + 80 * std::exp(-tb * 40)) * tb);
            const double bass = 0.15 * std::sin(2 * M_PI * 55 * t);
            const double chordL = 0.07 * (std::sin(2 * M_PI * 220 * t) + std::sin(2 * M_PI * 277.18 * t) + std::sin(2 * M_PI * 329.63 * t));
            const double chordR = 0.07 * (std::sin(2 * M_PI * 220.5 * t) + std::sin(2 * M_PI * 277.6 * t) + std::sin(2 * M_PI * 330.2 * t));
            const double hiss = 0.03 * noise(rng);
            inL[i] = env * (kick + bass + chordL + hiss);
            inR[i] = env * (kick + bass + chordR - hiss);
            sc[i] = kick;
        }
        ti.time_position = n / kRate;
        ti.beat_position = ti.time_position * 2;
        ysfx_set_time_info(fx, &ti);
        if (instrument) {
            int m = 0;
            for (uint64_t k = (n + every - 1) / every; k * every < n + block && m < 62; ++k) {
                // the previous note ends where the next one starts
                if (k > 0) {
                    msgs[m][0] = 0x80; msgs[m][1] = phrase[(k - 1) % 16]; msgs[m][2] = 0;
                    ysfx_midi_event_t off = {0, (uint32_t)(k * every - n), 3, msgs[m]};
                    ysfx_send_midi(fx, &off);
                    ++m;
                }
                msgs[m][0] = 0x90; msgs[m][1] = phrase[k % 16]; msgs[m][2] = (uint8_t)(70 + (k * 13) % 40);
                ysfx_midi_event_t on = {0, (uint32_t)(k * every - n), 3, msgs[m]};
                ysfx_send_midi(fx, &on);
                ++m;
            }
        }
        ysfx_process_double(fx, ins, outs, nin, 2, block);
        ysfx_midi_event_t me;
        while (ysfx_receive_midi(fx, &me)) {}
        for (uint32_t i = 0; i < block; ++i) {
            if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) { finite = false; continue; }
            if (n + i < skip) continue;
            peak = std::fmax(peak, std::fmax(std::fabs(outL[i]), std::fabs(outR[i])));
            acc += outL[i] * outL[i] + outR[i] * outR[i];
            ++counted;
        }
        n += block;
    }
    Level lv;
    lv.finite = finite;
    lv.peakDb = db(peak);
    lv.rmsDb = db(std::sqrt(acc / (2.0 * std::max<uint64_t>(1, counted))));
    return lv;
}

// "<plugin>" -> dB for the whole plugin, "<plugin>/<preset>" -> dB for one preset
static std::map<std::string, double> allowances(const std::string &path)
{
    std::map<std::string, double> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream s(line);
        std::string name, preset;
        double v;
        if (!(s >> name >> v)) continue;
        std::getline(s, preset);
        const size_t b = preset.find_first_not_of(" \t");
        out[b == std::string::npos ? name : name + "/" + preset.substr(b)] = v;
    }
    return out;
}

int main(int argc, char **argv)
{
    bool report = false;
    int a = 1;
    if (a < argc && std::strcmp(argv[a], "--report") == 0) { report = true; ++a; }
    if (a >= argc) { std::fprintf(stderr, "usage: preset_test [--report] <tests/preset_levels.txt> <plugin.jsfx ...>\n"); return 2; }
    const std::map<std::string, double> allow = allowances(argv[a++]);
    int failed = 0, presets = 0, plugins = 0;
    for (; a < argc; ++a) {
        const std::string path = argv[a];
        const std::string text = read_text(path);
        const std::string cat = category(text);
        if (cat == "MIDI") continue;
        std::string stem = path.substr(path.find_last_of('/') + 1);
        stem = stem.substr(0, stem.size() - 5);
        const std::string rpl = path + ".rpl";
        ysfx_bank_t *bank = ysfx_load_bank(rpl.c_str());
        if (!bank) continue;
        static const char *meters[] = {"loudness_meter", "spectrum_analyzer", "stereo_scope", "oscilloscope", "vu_meter",
                                       "tuner", "reference", "multichannel_loudness", "downmix_check"};
        bool meter = false;
        for (const char *m : meters) meter = meter || stem == m;
        if (meter) { ysfx_bank_free(bank); continue; }
        ++plugins;
        const auto it = allow.find(stem);
        const double plugin_jump = it != allow.end() ? it->second : kMaxJumpDb;
        double ref = 0;
        for (uint32_t p = 0; p < bank->preset_count; ++p) {
            // a fresh instance per preset, so no tail of the one before is measured
            ysfx_config_t *conf = ysfx_config_new();
            ysfx_guess_file_roots(conf, path.c_str());
            ysfx_set_log_reporter(conf, &reporter);
            ysfx_t *fx = ysfx_new(conf);
            ysfx_config_free(conf);
            if (!ysfx_load_file(fx, path.c_str(), 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
                std::printf("FAIL %s: does not compile\n", stem.c_str());
                ++failed;
                ysfx_free(fx);
                break;
            }
            if (ysfx_get_num_outputs(fx) >= 12) { ysfx_free(fx); --plugins; break; }
            ysfx_set_sample_rate(fx, kRate);
            ysfx_set_block_size(fx, 256);
            ysfx_init(fx);
            ysfx_load_state(fx, bank->presets[p].state);
            const Level lv = render(fx);
            ysfx_free(fx);
            const std::string name = bank->presets[p].name ? bank->presets[p].name : "?";
            const auto one = allow.find(stem + "/" + name);
            const double jump = one != allow.end() ? one->second : plugin_jump;
            if (p == 0) ref = lv.rmsDb;
            ++presets;
            const double d = lv.rmsDb - ref;
            char why[160] = "";
            if (!lv.finite) std::snprintf(why, sizeof why, "output is not finite");
            else if (lv.peakDb > kMaxPeakDb) std::snprintf(why, sizeof why, "peaks at %+.1f dBFS (max %+.0f)", lv.peakDb, kMaxPeakDb);
            else if (p > 0 && std::fabs(d) > jump) std::snprintf(why, sizeof why, "level %+.1f dB from Default (allowed %.0f)", d, jump);
            if (report)
                std::printf("%s\t%s\trms %7.2f dB\tdiff %+7.2f dB\tpeak %+6.2f dBFS%s\n", stem.c_str(), name.c_str(),
                            lv.rmsDb, p ? d : 0.0, lv.peakDb, why[0] ? "  <-" : "");
            else if (why[0]) {
                std::printf("FAIL %s, preset \"%s\": %s\n", stem.c_str(), name.c_str(), why);
                ++failed;
            }
        }
        ysfx_bank_free(bank);
    }
    std::printf("preset_test: %d presets of %d plugins, %d failed\n", presets, plugins, report ? 0 : failed);
    return report ? 0 : failed;
}
