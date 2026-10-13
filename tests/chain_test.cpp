// Render test for the NULL JSFX FX chains (DATA/FXChains/*/*.RfxChain), built on ysfx.
// For every chain given on the command line: load each <JS ...> plugin from DATA/Effects with the
// slider values stored in the chain, run test audio through the plugins in series at 44.1 and
// 96 kHz, and report errors (missing plugin, compile failure, NaN/Inf, blow-ups, silence) plus the
// level change from input to output. Routing works as in REAPER: REQUIRED_CHANNELS sets the track
// channels and a plugin's JS_PINMAP the channels each pin reads or writes (a channel no output pin
// writes passes through). Parameter modulation (PROGRAMENV) stays at its baseline, the slider value.
// Exit code is the number of chains that failed.
#include "ysfx.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Fx {
    std::string path;
    std::vector<std::string> values;
    std::vector<uint64_t> in_masks, out_masks;  // per pin, the track channels (bit 0 = channel 1)
};

static std::string base64_decode(const std::string &s)
{
    static const std::string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : s) {
        size_t v = abc.find(c);
        if (v == std::string::npos) continue;  // padding, spaces
        acc = (acc << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) { bits -= 8; out += char((acc >> bits) & 0xff); }
    }
    return out;
}

// JS_PINMAP as REAPER writes it: 1000, then for the inputs and the outputs: byte size, 1000,
// track channels, pin count and a 64-bit channel mask per pin
static bool parse_pinmap(const std::string &data, Fx &fx)
{
    auto u32 = [&](size_t at) { uint32_t v = 0; for (int i = 3; i >= 0; --i) v = (v << 8) | uint8_t(data[at + i]); return v; };
    if (data.size() < 4 || u32(0) != 1000) return false;
    size_t at = 4;
    for (std::vector<uint64_t> *masks : {&fx.in_masks, &fx.out_masks}) {
        if (at + 16 > data.size()) return false;
        uint32_t pins = u32(at + 12);
        at += 16;
        if (at + 8 * size_t(pins) > data.size()) return false;
        for (uint32_t p = 0; p < pins; ++p, at += 8) masks->push_back(uint64_t(u32(at)) | (uint64_t(u32(at + 4)) << 32));
    }
    return true;
}

static std::vector<Fx> parse_chain(const std::string &file, uint32_t &channels, std::string &err)
{
    std::ifstream in(file);
    std::vector<Fx> fxs;
    std::string line, pinmap;
    bool want_values = false, in_pinmap = false;
    int skip = 0;  // depth inside a block that holds no plugin (PROGRAMENV)
    channels = 2;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tok;
        ls >> tok;
        if (in_pinmap) {
            if (tok == ">") {
                in_pinmap = false;
                if (fxs.empty() || !parse_pinmap(base64_decode(pinmap), fxs.back())) err = "bad JS_PINMAP";
            } else {
                pinmap += tok;
            }
        } else if (skip) {
            if (tok == ">") --skip;
            else if (tok.size() > 1 && tok[0] == '<') ++skip;
        } else if (tok == "REQUIRED_CHANNELS") {
            ls >> channels;
        } else if (tok == "<JS_PINMAP") {
            in_pinmap = true;
            pinmap.clear();
        } else if (tok == "<PROGRAMENV") {
            skip = 1;
        } else if (tok == "<JS") {
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
    uint32_t nch = 2;
    std::vector<Fx> chain = parse_chain(file, nch, err);
    if (nch < 2 || nch > 64) err = "REQUIRED_CHANNELS out of range";
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
        std::vector<std::vector<double>> ch(nch, std::vector<double>(block));
        std::vector<double> &l = ch[0], &r = ch[1];
        ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
        std::mt19937 rng(7);
        std::uniform_real_distribution<double> noise(-1, 1);
        double in2 = 0, out2 = 0, peak = 0;
        bool nonfinite = false;
        uint64_t n = 0, total = (uint64_t)(sr * 4);
        while (n < total) {
            double inblk = 0;
            for (uint32_t c = 2; c < nch; ++c) std::fill(ch[c].begin(), ch[c].end(), 0.0);
            for (uint32_t i = 0; i < block; ++i) {
                double t = (n + i) / sr, env = std::fmod(t, 0.5) < 0.3 ? 1.0 : 0.15;
                l[i] = 0.2 * env * (std::sin(2 * M_PI * 110 * t) + 0.6 * std::sin(2 * M_PI * 277 * t) + 0.2 * noise(rng));
                r[i] = 0.2 * env * (std::sin(2 * M_PI * 165 * t) + 0.6 * std::sin(2 * M_PI * 330 * t) + 0.2 * noise(rng));
                inblk += l[i] * l[i] + r[i] * r[i];
            }
            if (n > sr) in2 += inblk;  // skip the first second (latency, settling)
            ti.time_position = n / sr;
            ti.beat_position = ti.time_position * 2;
            for (size_t k = 0; k < fxs.size(); ++k) {
                ysfx_t *fx = fxs[k];
                const Fx &f = chain[k];
                uint32_t nin = ysfx_get_num_inputs(fx), nout = ysfx_get_num_outputs(fx);
                // pins without a pin map read and write the track channel of the same number
                auto in_mask = [&](uint32_t p) { return p < f.in_masks.size() ? f.in_masks[p] : (p < nch ? 1ull << p : 0); };
                auto out_mask = [&](uint32_t p) { return p < f.out_masks.size() ? f.out_masks[p] : (p < nch ? 1ull << p : 0); };
                std::vector<std::vector<double>> pin_in(nin, std::vector<double>(block, 0.0)), pin_out(nout, std::vector<double>(block));
                std::vector<const double *> ins(nin);
                std::vector<double *> outs(nout);
                for (uint32_t p = 0; p < nin; ++p) {
                    for (uint32_t c = 0; c < nch; ++c)
                        if (in_mask(p) >> c & 1)
                            for (uint32_t i = 0; i < block; ++i) pin_in[p][i] += ch[c][i];
                    ins[p] = pin_in[p].data();
                }
                for (uint32_t p = 0; p < nout; ++p) outs[p] = pin_out[p].data();
                ysfx_set_time_info(fx, &ti);
                ysfx_process_double(fx, ins.data(), outs.data(), nin, nout, block);
                for (uint32_t c = 0; c < nch; ++c) {
                    bool written = false;
                    for (uint32_t p = 0; p < nout; ++p) {
                        if (!(out_mask(p) >> c & 1)) continue;
                        if (!written) std::fill(ch[c].begin(), ch[c].end(), 0.0);
                        written = true;
                        for (uint32_t i = 0; i < block; ++i) ch[c][i] += pin_out[p][i];
                    }
                }
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
