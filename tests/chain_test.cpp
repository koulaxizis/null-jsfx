// Render test for the NULL JSFX FX chains (DATA/FXChains/*/*.RfxChain), built on ysfx.
// For every chain given on the command line: load each <JS ...> plugin from DATA/Effects with the
// slider values stored in the chain, run test audio through the plugins in series at 44.1 and
// 96 kHz, and report errors (missing plugin, compile failure, NaN/Inf, blow-ups, silence) plus the
// level change from input to output.
// Exit code is the number of chains that failed.
#include "ysfx.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Fx { std::string path; std::vector<std::string> values; };

static std::vector<Fx> parse_chain(const std::string &file, std::string &err)
{
    std::ifstream in(file);
    std::vector<Fx> fxs;
    std::string line;
    bool want_values = false;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tok;
        ls >> tok;
        if (tok == "<JS") {
            Fx fx;
            ls >> fx.path;
            fxs.push_back(fx);
            want_values = true;
        } else if (want_values) {
            std::istringstream vs(line);
            std::string v;
            while (vs >> v) fxs.back().values.push_back(v);
            want_values = false;
        } else if (tok.size() > 1 && tok[0] == '<') {
            err = "only JS plugins are expected, found " + tok;
        }
    }
    if (fxs.empty()) err = "no plugins in the chain";
    return fxs;
}

static void silent(intptr_t, ysfx_log_level, const char *) {}

static int test_chain(const std::string &file, const std::string &effects)
{
    std::string err;
    std::vector<Fx> chain = parse_chain(file, err);
    std::string name = file.substr(file.find_last_of('/') + 1);
    if (!err.empty()) { printf("FAIL %s\n    error: %s\n", name.c_str(), err.c_str()); return 1; }
    std::string report;
    bool fail = false;
    for (double sr : {44100.0, 96000.0}) {
        std::vector<ysfx_t *> fxs;
        for (const Fx &f : chain) {
            std::string path = effects + "/" + f.path;
            ysfx_config_t *cfg = ysfx_config_new();
            ysfx_guess_file_roots(cfg, path.c_str());
            ysfx_set_log_reporter(cfg, &silent);
            ysfx_t *fx = ysfx_new(cfg);
            ysfx_config_free(cfg);
            if (!ysfx_load_file(fx, path.c_str(), 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
                printf("FAIL %s\n    error: cannot load %s\n", name.c_str(), f.path.c_str());
                ysfx_free(fx);
                for (ysfx_t *x : fxs) ysfx_free(x);
                return 1;
            }
            ysfx_set_sample_rate(fx, sr);
            ysfx_set_block_size(fx, 256);
            ysfx_init(fx);
            for (uint32_t i = 0; i < f.values.size() && i < ysfx_max_sliders; ++i) {
                if (f.values[i] == "-") continue;
                if (!ysfx_slider_exists(fx, i)) { fail = true; report += "    error: " + f.path + " has no slider" + std::to_string(i + 1) + "\n"; continue; }
                ysfx_slider_set_value(fx, i, std::stod(f.values[i]));
            }
            fxs.push_back(fx);
        }
        // 4 s of music-like stereo: a chord with noise, gated on and off for the dynamics plugins
        const uint32_t block = 256;
        std::vector<double> l(block), r(block), sc(block, 0.0);
        ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
        std::mt19937 rng(7);
        std::uniform_real_distribution<double> noise(-1, 1);
        double in2 = 0, out2 = 0, peak = 0;
        bool nonfinite = false;
        uint64_t n = 0, total = (uint64_t)(sr * 4);
        while (n < total) {
            double inblk = 0;
            for (uint32_t i = 0; i < block; ++i) {
                double t = (n + i) / sr, env = std::fmod(t, 0.5) < 0.3 ? 1.0 : 0.15;
                l[i] = 0.2 * env * (std::sin(2 * M_PI * 110 * t) + 0.6 * std::sin(2 * M_PI * 277 * t) + 0.2 * noise(rng));
                r[i] = 0.2 * env * (std::sin(2 * M_PI * 165 * t) + 0.6 * std::sin(2 * M_PI * 330 * t) + 0.2 * noise(rng));
                inblk += l[i] * l[i] + r[i] * r[i];
            }
            if (n > sr) in2 += inblk;  // skip the first second (latency, settling)
            ti.time_position = n / sr;
            ti.beat_position = ti.time_position * 2;
            for (ysfx_t *fx : fxs) {
                std::vector<double> ol(block), orr(block);
                const double *ins[4] = {l.data(), r.data(), sc.data(), sc.data()};
                double *outs[2] = {ol.data(), orr.data()};
                ysfx_set_time_info(fx, &ti);
                ysfx_process_double(fx, ins, outs, ysfx_get_num_inputs(fx) > 2 ? 4 : 2, 2, block);
                l.swap(ol);
                r.swap(orr);
            }
            for (uint32_t i = 0; i < block; ++i) {
                if (!std::isfinite(l[i]) || !std::isfinite(r[i])) { nonfinite = true; continue; }
                peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
                if (n > sr) out2 += l[i] * l[i] + r[i] * r[i];
            }
            n += block;
        }
        for (ysfx_t *fx : fxs) ysfx_free(fx);
        double gain = 10 * std::log10((out2 + 1e-30) / in2);
        char b[160];
        snprintf(b, sizeof b, "    %.0f Hz: level %+.1f dB, peak %.2f\n", sr, gain, peak);
        report += b;
        if (nonfinite) { fail = true; report += "    error: NaN/Inf output\n"; }
        if (peak > 4) { fail = true; report += "    error: output blows up\n"; }
        if (gain < -40) { fail = true; report += "    error: (almost) silent output\n"; }
    }
    printf("%s %s (%zu plugins)\n%s", fail ? "FAIL" : "ok  ", name.c_str(), chain.size(), report.c_str());
    return fail;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: chain_test <Effects dir> chain.RfxChain ...\n"); return 2; }
    int failed = 0;
    for (int i = 2; i < argc; ++i) failed += test_chain(argv[i], argv[1]);
    printf("\n%d of %d chains failed\n", failed, argc - 2);
    return failed;
}
