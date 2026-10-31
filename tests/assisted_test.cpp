// NULL JSFX - checks that the Learn rules of the Assisted plugins do what they promise.
// SPDX-License-Identifier: MIT
//
// usage: assisted_test <dir with assisted_eq.jsfx, assisted_comp.jsfx and gfx/>
//
// Each case renders a known signal through a plugin (and its GFX version) with Learn On, then
// reads the hidden Learned sliders back:
//   EQ  - a spectrum on the -4.5 dB/oct reference with a +6 dB bump at 250-400 Hz reads as mud
//         (the mud region high, the others near 0), the bump's band becomes the mud frequency,
//         Learn returns to Off, and the EQ then cuts there;
//       - silence never finishes learning;
//       - Learn turned off after 1 s finishes with what it heard.
//   Comp - a signal stepping between -30 and -6 dBFS: the 90th percentile reads near -6 dBFS,
//         the median near the quiet level, and the crest factor of a sine near 3 dB;
//       - a click train every 250 ms gives an onset interval near 250 ms;
//       - with Target Podcast, the output after Learn reads near -16 LUFS.
//   Reverb - clicks every 250 ms read as onsets 250 ms apart and keep the Warm decay under six
//         of them (the tail after Learn is shorter); white noise reads bright, a twice
//         low-passed one dark.
//   Delay - at 100 bpm the Natural echo comes a quarter note (600 ms) after the dry signal;
//         after Learn on clicks faster than the Warm echoes, the repeats halve: each echo of
//         a burst is about 15 dB under the one before.
//   Native - the VST3/CLAP DSP (native/plugins/assisted_*/Assisted*DSP.hpp) learns the same
//         values as the JSFX from the same 16 s and then sounds the same (1e-5); the comparison
//         test (native/test/compare) covers the rest but never plays 15 s with Learn On.
// Exit code 0 when every check passes.
//
// build: g++ -O2 -std=c++17 -I <ysfx>/include -I native/common -I native/plugins/assisted_eq
//            -I native/plugins/assisted_comp -I native/plugins/assisted_reverb
//            -I native/plugins/assisted_delay tests/assisted_test.cpp libysfx.a -lpthread -ldl

#include <ysfx.h>

#include "AssistedCompDSP.hpp"
#include "AssistedDelayDSP.hpp"
#include "AssistedEqDSP.hpp"
#include "AssistedReverbDSP.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

static int g_fail = 0;

static void check(bool ok, const std::string& what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok)
        ++g_fail;
}

static ysfx_t* load(const std::string& path, double sr)
{
    ysfx_config_t* cfg = ysfx_config_new();
    ysfx_guess_file_roots(cfg, path.c_str());
    ysfx_t* fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, path.c_str(), 0) || !ysfx_compile(fx, ysfx_compile_no_gfx))
    {
        std::printf("FAIL %s does not compile\n", path.c_str());
        ++g_fail;
        ysfx_free(fx);
        return nullptr;
    }
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, 512);
    ysfx_init(fx);
    return fx;
}

// renders `seconds` of gen(t) -> (l, r); returns the output
using Gen = std::function<void(size_t, double&, double&)>;
static std::vector<double> render(ysfx_t* fx, double sr, double seconds, const Gen& gen,
                                  double setAt = -1, uint32_t setSlider = 0, double setValue = 0)
{
    const size_t n = size_t(seconds * sr);
    std::vector<double> out;
    out.reserve(2 * n);
    std::vector<double> l(512), r(512), ol(512), orr(512);
    bool set = false;
    for (size_t pos = 0; pos < n; pos += 512)
    {
        const uint32_t m = uint32_t(std::min<size_t>(512, n - pos));
        if (!set && setAt >= 0 && double(pos) >= setAt * sr)
        {
            ysfx_slider_set_value(fx, setSlider, setValue);
            set = true;
        }
        for (uint32_t i = 0; i < m; ++i)
            gen(pos + i, l[i], r[i]);
        const double* ins[2] = { l.data(), r.data() };
        double* outs[2] = { ol.data(), orr.data() };
        ysfx_process_double(fx, ins, outs, 2, 2, m);
        for (uint32_t i = 0; i < m; ++i)
        {
            out.push_back(ol[i]);
            out.push_back(orr[i]);
        }
    }
    return out;
}

static double slider(ysfx_t* fx, uint32_t i) { return ysfx_slider_get_value(fx, i); }

// the host tempo the plugin sees from now on
static void set_tempo(ysfx_t* fx, double bpm)
{
    ysfx_time_info_t ti {};
    ti.tempo = bpm;
    ti.playback_state = ysfx_playback_paused;
    ti.time_signature[0] = 4;
    ti.time_signature[1] = 4;
    ysfx_set_time_info(fx, &ti);
}

// the -60 dB time of a tail from its 50 ms RMS (twice the -5 to -35 dB time), in seconds
static double t60(const std::vector<double>& out, double sr, double from)
{
    const size_t w = size_t(0.05 * sr);
    std::vector<double> db;
    for (size_t i = size_t(from * sr); i + w <= out.size() / 2; i += w)
    {
        double s = 0;
        for (size_t j = i; j < i + w; ++j)
            s += out[2 * j] * out[2 * j];
        db.push_back(10 * std::log10(s / double(w) + 1e-30));
    }
    double t5 = -1, t35 = -1;
    for (size_t i = 0; i < db.size(); ++i)
    {
        if (t5 < 0 && db[i] < db[0] - 5)
            t5 = double(i) * 0.05;
        if (t35 < 0 && db[i] < db[0] - 35)
            t35 = double(i) * 0.05;
    }
    return t5 >= 0 && t35 >= 0 ? 2 * (t35 - t5) : -1;
}

// a sum of sines, 24 per octave from 30 Hz to 16 kHz, with random phases: every third-octave
// band holds the same number of them, so the band levels follow amp_db(f)
static Gen spectrum(double sr, const std::function<double(double)>& amp_db, double level)
{
    auto fs = std::make_shared<std::vector<double>>();
    auto as = std::make_shared<std::vector<double>>();
    auto ps = std::make_shared<std::vector<double>>();
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> u(0, 2 * M_PI);
    double sum = 0;
    for (double f = 20; f < 20000; f *= std::pow(2.0, 1.0 / 24))
    {
        fs->push_back(f);
        as->push_back(std::pow(10.0, amp_db(f) / 20));
        ps->push_back(u(rng));
        sum += as->back() * as->back() / 2;
    }
    const double norm = std::pow(10.0, level / 20) / std::sqrt(sum);
    return [=](size_t n, double& l, double& r) {
        double v = 0;
        const double t = double(n) / sr;
        for (size_t k = 0; k < fs->size(); ++k)
            v += (*as)[k] * std::sin(2 * M_PI * (*fs)[k] * t + (*ps)[k]);
        l = r = v * norm;
    };
}

// output RMS of a band around f (a narrow RBJ band-pass), in dB, after the first `skip` seconds
static double band_db(const std::vector<double>& out, double sr, double f, double skip)
{
    const double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * 8), a0 = 1 + al;
    const double b0 = al / a0, a1 = -2 * std::cos(w) / a0, a2 = (1 - al) / a0;
    double z1 = 0, z2 = 0, s = 0;
    size_t cnt = 0;
    for (size_t i = 0; i < out.size() / 2; ++i)
    {
        const double x = out[2 * i];
        const double y = b0 * x + z1;
        z1 = -a1 * y + z2;
        z2 = -b0 * x - a2 * y;
        if (double(i) >= skip * sr)
        {
            s += y * y;
            ++cnt;
        }
    }
    return 10 * std::log10(s / double(cnt) + 1e-30);
}

static void test_eq(const std::string& path)
{
    const std::string name = path.substr(path.find_last_of('/') + 1);
    const double sr = 48000;
    auto ref = [](double f) { return -4.5 * std::log2(f / 1000); };
    auto bump = [ref](double f) { return ref(f) + (f >= 250 && f <= 400 ? 6.0 : 0.0); };

    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 1, 4);   // Source Mix
        ysfx_slider_set_value(fx, 0, 1);   // Learn On
        const Gen g = spectrum(sr, bump, -20);
        render(fx, sr, 16, g);
        check(slider(fx, 0) == 0, name + ": Learn returns to Off after 15 s of signal");
        check(slider(fx, 8) == 1, name + ": Learned is Yes");
        check(slider(fx, 10) > 1.5, name + ": a 250-400 Hz bump reads as mud (" + std::to_string(slider(fx, 10)) + " dB)");
        check(slider(fx, 11) >= 250 && slider(fx, 11) <= 400,
              name + ": the mud frequency is in the bump (" + std::to_string(slider(fx, 11)) + " Hz)");
        bool flat = true;
        for (uint32_t i : { 9u, 12u, 13u, 14u })
            flat = flat && std::fabs(slider(fx, i)) < 1.5;
        check(flat, name + ": low, honk, presence and air stay within 1.5 dB of the reference (" + std::to_string(slider(fx, 9)) + ", " + std::to_string(slider(fx, 12)) + ", " + std::to_string(slider(fx, 13)) + ", " + std::to_string(slider(fx, 14)) + ")");
        // the EQ now cuts the bump: compare the band at 315 Hz with 1 kHz, before and after
        const std::vector<double> out = render(fx, sr, 2, g);
        const double before = bump(315) - bump(1000);
        const double after = band_db(out, sr, 315, 0.5) - band_db(out, sr, 1000, 0.5);
        check(after < before - 1, name + ": after Learn the EQ cuts 315 Hz against 1 kHz (" +
                                      std::to_string(before) + " -> " + std::to_string(after) + " dB)");
        ysfx_free(fx);
    }

    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, [](size_t, double& l, double& r) { l = r = 0; });
        check(slider(fx, 0) == 1 && slider(fx, 8) == 0, name + ": silence never finishes learning");
        ysfx_free(fx);
    }

    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 1.5, spectrum(sr, ref, -20), 1.0, 0, 0);
        check(slider(fx, 8) == 1, name + ": Learn turned off after 1 s finishes with what it heard");
        ysfx_free(fx);
    }
}


// integrated loudness (BS.1770, as the plugin measures it but with exact gating) of an output
static double lufs(const std::vector<double>& out, double sr, double skip)
{
    const double pi = M_PI;
    double k = std::tan(pi * 1681.974450955533 / sr);
    const double vh = std::pow(10.0, 3.999843853973347 / 20), vb = std::pow(vh, 0.4996667741545416);
    double q = 0.7071752369554196, a0 = 1 + k / q + k * k;
    const double b0 = (vh + vb * k / q + k * k) / a0, b1 = 2 * (k * k - vh) / a0, b2 = (vh - vb * k / q + k * k) / a0;
    const double a1 = 2 * (k * k - 1) / a0, a2 = (1 - k / q + k * k) / a0;
    k = std::tan(pi * 38.13547087602444 / sr);
    q = 0.5003270373238773;
    const double c0 = 1 + k / q + k * k, c1 = 2 * (k * k - 1) / c0, c2 = (1 - k / q + k * k) / c0;
    double z[2][4] = {};
    const size_t sub = size_t(0.1 * sr);
    std::vector<double> subs;
    double acc = 0;
    size_t cnt = 0;
    for (size_t i = size_t(skip * sr); i < out.size() / 2; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const double x = out[2 * i + ch];
            const double y = b0 * x + z[ch][0];
            z[ch][0] = b1 * x - a1 * y + z[ch][1];
            z[ch][1] = b2 * x - a2 * y;
            const double kk = y + z[ch][2];
            z[ch][2] = -2 * y - c1 * kk + z[ch][3];
            z[ch][3] = y - c2 * kk;
            acc += kk * kk;
        }
        if (++cnt == sub)
        {
            subs.push_back(acc);
            acc = 0;
            cnt = 0;
        }
    }
    std::vector<double> blocks;
    for (size_t i = 3; i < subs.size(); ++i)
        blocks.push_back((subs[i] + subs[i - 1] + subs[i - 2] + subs[i - 3]) / (4.0 * double(sub)));
    auto mean = [&](double gate) {
        double p = 0, n = 0;
        for (double b : blocks)
            if (-0.691 + 10 * std::log10(b) > gate) { p += b; n += 1; }
        return n > 0 ? -0.691 + 10 * std::log10(p / n) : -200.0;
    };
    return mean(mean(-70) - 10);
}

static void test_comp(const std::string& path)
{
    const std::string name = path.substr(path.find_last_of('/') + 1);
    const double sr = 48000;
    // a 1 kHz sine whose level steps every 0.5 s: three quiet steps at -30 dBFS, then one at -6
    auto steps = [sr](size_t n, double& l, double& r) {
        const double t = double(n) / sr;
        const double db = (size_t(t / 0.5) % 4) == 3 ? -6.0 : -30.0;
        l = r = std::pow(10.0, db / 20) * std::sqrt(2.0) * std::sin(2 * M_PI * 1000 * t);
    };
    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, steps);
        check(slider(fx, 0) == 0 && slider(fx, 8) == 1, name + ": Learn finishes and returns to Off");
        check(std::fabs(slider(fx, 9) + 6) < 1, name + ": the loud parts read near -6 dBFS (" + std::to_string(slider(fx, 9)) + ")");
        check(std::fabs(slider(fx, 10) + 30) < 1, name + ": the median reads near -30 dBFS (" + std::to_string(slider(fx, 10)) + ")");
        check(std::fabs(slider(fx, 11) - 3) < 1, name + ": a sine's crest factor reads near 3 dB (" + std::to_string(slider(fx, 11)) + ")");
        ysfx_free(fx);
    }
    // clicks every 250 ms (a 2 ms burst at -6 dBFS over a -40 dBFS hum)
    auto clicks = [sr](size_t n, double& l, double& r) {
        const double t = double(n) / sr;
        const double ph = std::fmod(t, 0.25);
        const double a = ph < 0.002 ? 0.5 : 0.01;
        l = r = a * std::sin(2 * M_PI * 220 * t);
    };
    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, clicks);
        check(std::fabs(slider(fx, 12) - 250) < 15, name + ": clicks every 250 ms read as onsets 250 ms apart (" + std::to_string(slider(fx, 12)) + " ms)");
        ysfx_free(fx);
    }
    // Podcast: after Learn the output sits near -16 LUFS
    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 2, 5);
        ysfx_slider_set_value(fx, 0, 1);
        auto ref = [](double f) { return -4.5 * std::log2(f / 1000); };
        const Gen g = spectrum(sr, ref, -30);
        render(fx, sr, 16, g);
        const std::vector<double> out = render(fx, sr, 6, g);
        const double l = lufs(out, sr, 1);
        check(std::fabs(l + 16) < 1.5, name + ": Target Podcast brings the output near -16 LUFS (" + std::to_string(l) + " LUFS)");
        ysfx_free(fx);
    }
}

// the native DSP against the JSFX: same input (rounded to float, as a host gives it), same
// 512-sample blocks, Learn On from the start; the learned sliders must match exactly and the
// output, during and after Learn, within 1e-5
template <class Dsp>
static void test_native(const std::string& path, Dsp& dsp, int numValues, const std::vector<std::pair<int, double>>& set,
                        const Gen& gen, double tempo = 0)
{
    const std::string name = path.substr(path.find_last_of('/') + 1) + " vs native";
    const double sr = 48000;
    ysfx_t* fx = load(path, sr);
    if (!fx)
        return;
    if (tempo > 0)
        set_tempo(fx, tempo);
    for (int i = 0; i < numValues; ++i)
        dsp.s[i] = ysfx_slider_get_value(fx, uint32_t(i));
    for (auto [i, v] : set)
    {
        ysfx_slider_set_value(fx, uint32_t(i), v);
        dsp.s[i] = v;
    }
    dsp.setSampleRate(sr);
    dsp.update();
    std::vector<double> l(512), r(512), ol(512), orr(512);
    std::vector<float> fl(512), fr(512), nl(512), nr(512);
    double worst = 0;
    const size_t n = size_t(18 * sr);
    for (size_t pos = 0; pos < n; pos += 512)
    {
        for (uint32_t i = 0; i < 512; ++i)
        {
            double a, b;
            gen(pos + i, a, b);
            fl[i] = float(a); fr[i] = float(b);
            l[i] = fl[i]; r[i] = fr[i];
        }
        const double* ins[2] = { l.data(), r.data() };
        double* outs[2] = { ol.data(), orr.data() };
        ysfx_process_double(fx, ins, outs, 2, 2, 512);
        if constexpr (std::is_same_v<Dsp, nullassisteddelay::AssistedDelayProcessor>)
            dsp.block(tempo > 0 ? tempo : 120.0);
        else
            dsp.block();
        dsp.process(fl.data(), fr.data(), nl.data(), nr.data(), 512);
        for (uint32_t i = 0; i < 512; ++i)
            worst = std::max({ worst, std::fabs(ol[i] - nl[i]), std::fabs(orr[i] - nr[i]) });
    }
    bool same = true;
    std::string vals;
    for (int i = 0; i < numValues; ++i)
    {
        const double a = ysfx_slider_get_value(fx, uint32_t(i));
        if (a != dsp.s[i])
        {
            same = false;
            vals += " slider" + std::to_string(i + 1) + " " + std::to_string(a) + "/" + std::to_string(dsp.s[i]);
        }
    }
    check(ysfx_slider_get_value(fx, 8) == 1, name + ": Learn finished");
    check(same, name + ": the learned sliders match" + vals);
    char buf[64];
    std::snprintf(buf, sizeof(buf), " (max |diff| %.3g)", worst);
    check(worst <= 1e-5, name + ": the output matches" + buf);
    ysfx_free(fx);
}

// 250 ms clicks: a 2 ms burst at -6 dBFS over a -40 dBFS hum
static void clicks(double sr, size_t n, double& l, double& r)
{
    const double t = double(n) / sr;
    const double a = std::fmod(t, 0.25) < 0.002 ? 0.5 : 0.01;
    l = r = a * std::sin(2 * M_PI * 220 * t);
}

static void test_reverb(const std::string& path)
{
    const std::string name = path.substr(path.find_last_of('/') + 1);
    const double sr = 48000;
    const Gen click = [sr](size_t n, double& l, double& r) { clicks(sr, n, l, r); };
    std::mt19937 rng(3);
    std::normal_distribution<double> nd(0, 0.1);
    auto noise = [&](size_t, double& l, double& r) { l = r = nd(rng); };
    // a 3 s noise burst then 4 s of silence: the tail's -60 dB time
    auto tail = [&](ysfx_t* fx) {
        std::mt19937 g2(5);
        const std::vector<double> out = render(fx, sr, 7, [&](size_t n, double& l, double& r) {
            l = r = double(n) < 3 * sr ? nd(g2) : 0.0;
        });
        return t60(out, sr, 3.5); // after the ducking has let go
    };
    double before = -1;
    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 1, 3); // Source Instrument, Target Warm: 1.98 s
        ysfx_slider_set_value(fx, 2, 1);
        before = tail(fx);
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, click);
        check(slider(fx, 0) == 0 && slider(fx, 8) == 1, name + ": Learn finishes and returns to Off");
        check(std::fabs(slider(fx, 11) - 250) < 15, name + ": clicks every 250 ms read as onsets 250 ms apart (" + std::to_string(slider(fx, 11)) + " ms)");
        const double after = tail(fx);
        check(before > 0 && after > 0 && after < before * 0.85,
              name + ": the decay is kept under six onset intervals (" + std::to_string(before) + " -> " + std::to_string(after) + " s)");
        ysfx_free(fx);
    }
    if (ysfx_t* fx = load(path, sr))
    {
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, noise);
        check(slider(fx, 12) > -3, name + ": white noise reads bright (" + std::to_string(slider(fx, 12)) + " dB)");
        ysfx_free(fx);
    }
    if (ysfx_t* fx = load(path, sr))
    {
        double z1 = 0, z2 = 0;
        const double a = 1 - std::exp(-2 * M_PI * 300 / sr);
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, [&](size_t, double& l, double& r) {
            z1 += a * (nd(rng) - z1);
            z2 += a * (z1 - z2);
            l = r = 4 * z2;
        });
        check(slider(fx, 12) < -20, name + ": noise low-passed twice at 300 Hz reads dark (" + std::to_string(slider(fx, 12)) + " dB)");
        ysfx_free(fx);
    }
}

// the peak of |left| within [t0, t1) seconds and where it is (in seconds)
static std::pair<double, double> peak(const std::vector<double>& out, double sr, double t0, double t1)
{
    double p = 0, at = -1;
    for (size_t i = size_t(t0 * sr); i < size_t(t1 * sr) && 2 * i < out.size(); ++i)
        if (std::fabs(out[2 * i]) > p)
        {
            p = std::fabs(out[2 * i]);
            at = double(i) / sr;
        }
    return { p, at };
}

static void test_delay(const std::string& path)
{
    const std::string name = path.substr(path.find_last_of('/') + 1);
    const double sr = 48000;
    // a 20 ms 1 kHz burst at the start, then silence
    auto burst = [sr](size_t n, double& l, double& r) {
        l = r = double(n) < 0.02 * sr ? 0.5 * std::sin(2 * M_PI * 1000 * double(n) / sr) : 0.0;
    };
    if (ysfx_t* fx = load(path, sr))
    {
        set_tempo(fx, 100);
        ysfx_slider_set_value(fx, 1, 3); // Instrument
        const std::vector<double> out = render(fx, sr, 1.5, burst);
        const auto e = peak(out, sr, 0.1, 1.2);
        check(std::fabs(e.second - 0.6) < 0.025, name + ": at 100 bpm the Natural echo comes after a quarter note (" + std::to_string(e.second) + " s)");
        ysfx_free(fx);
    }
    if (ysfx_t* fx = load(path, sr))
    {
        set_tempo(fx, 100);
        ysfx_slider_set_value(fx, 1, 3); // Instrument
        ysfx_slider_set_value(fx, 2, 1); // Warm: a dotted quarter (900 ms), 4 repeats
        ysfx_slider_set_value(fx, 0, 1);
        render(fx, sr, 16, [sr](size_t n, double& l, double& r) { clicks(sr, n, l, r); });
        render(fx, sr, 4, [](size_t, double& l, double& r) { l = r = 0; });
        check(slider(fx, 8) == 1 && std::fabs(slider(fx, 11) - 250) < 15, name + ": Learn reads the clicks' onsets (" + std::to_string(slider(fx, 11)) + " ms)");
        const std::vector<double> out = render(fx, sr, 2.5, burst);
        const double e1 = peak(out, sr, 0.85, 0.95).first, e2 = peak(out, sr, 1.75, 1.85).first;
        const double d = 20 * std::log10(e2 / e1);
        check(std::fabs(d + 15) < 1.5, name + ": with onsets faster than the echoes the repeats halve (" + std::to_string(d) + " dB per echo)");
        ysfx_free(fx);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: assisted_test <dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    for (const char* f : { "/assisted_eq.jsfx", "/gfx/assisted_eq_gfx.jsfx" })
    {
        const std::string p = dir + f;
        if (FILE* fp = std::fopen(p.c_str(), "r"))
        {
            std::fclose(fp);
            test_eq(p);
        }
    }
    for (const char* f : { "/assisted_comp.jsfx", "/gfx/assisted_comp_gfx.jsfx" })
    {
        const std::string p = dir + f;
        if (FILE* fp = std::fopen(p.c_str(), "r"))
        {
            std::fclose(fp);
            test_comp(p);
        }
    }
    {
        auto ref = [](double f) { return -4.5 * std::log2(f / 1000); };
        auto bump = [ref](double f) { return ref(f) + (f >= 250 && f <= 400 ? 6.0 : 0.0); };
        auto eq = std::make_unique<nullassistedeq::AssistedEqProcessor>();
        test_native(dir + "/assisted_eq.jsfx", *eq, nullassistedeq::kNumValues, { { 1, 4 }, { 0, 1 } },
                    spectrum(48000, bump, -20));
        auto comp = std::make_unique<nullassistedcomp::AssistedCompProcessor>();
        test_native(dir + "/assisted_comp.jsfx", *comp, nullassistedcomp::kNumValues, { { 2, 5 }, { 0, 1 } },
                    spectrum(48000, ref, -30));
        // clicks over a spectrum, so onsets, levels and brightness all count
        const Gen sp = spectrum(48000, ref, -30);
        const Gen mixed = [sp](size_t n, double& l, double& r) {
            double a, b, c, d;
            sp(n, a, b);
            clicks(48000, n, c, d);
            l = a + c;
            r = b + d;
        };
        auto rv = std::make_unique<nullassistedreverb::AssistedReverbProcessor>();
        test_native(dir + "/assisted_reverb.jsfx", *rv, nullassistedreverb::kNumValues, { { 2, 1 }, { 0, 1 } }, mixed);
        auto dl = std::make_unique<nullassisteddelay::AssistedDelayProcessor>();
        test_native(dir + "/assisted_delay.jsfx", *dl, nullassisteddelay::kNumValues, { { 2, 2 }, { 0, 1 } }, mixed, 100);
    }
    for (const char* f : { "/assisted_reverb.jsfx", "/gfx/assisted_reverb_gfx.jsfx" })
        if (FILE* fp = std::fopen((dir + f).c_str(), "r"))
        {
            std::fclose(fp);
            test_reverb(dir + f);
        }
    for (const char* f : { "/assisted_delay.jsfx", "/gfx/assisted_delay_gfx.jsfx" })
        if (FILE* fp = std::fopen((dir + f).c_str(), "r"))
        {
            std::fclose(fp);
            test_delay(dir + f);
        }
    std::printf("\n%d check(s) failed\n", g_fail);
    return g_fail ? 1 : 0;
}
