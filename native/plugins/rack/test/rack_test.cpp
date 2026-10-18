/*
 * NULL JSFX - Rack engine tests (RackEngine directly, no plugin framework)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 *   presets     every factory preset loads (modules, slider names, ranges, sources, globals) and
 *               renders 4 s at 44.1 and 96 kHz: finite, peak < 4, not silent. Presets whose
 *               modules this build lacks are skipped (and listed), except in a full build.
 *   exact       one Full/Stereo slot at Mix 100% equals the module alone, bit for bit (tilt, delay)
 *   bands       Low/Mid/High with an identity module (with and without latency) sum flat (0.01 dB)
 *   channels    Left/Right leave the other side bit-exact; Mid/Side leave the other component
 *               (within float rounding of the M/S matrix)
 *   modulation  an LFO route moves its target; a macro at 0, or a route of depth 0, changes nothing
 *   threads     swapping modules and routes while audio runs
 *   state       routes and slots strings round-trip
 *
 * Added with the lanes, the feedback loop, the step sequencer and the keyed modules:
 *   identity    every factory preset that uses none of them renders bit for bit like the 2.1
 *               engine (test/RackEngineV1.hpp, a frozen copy), at 44.1 and 96 kHz, host blocks of
 *               512 (transport playing, Macro 1 sweeping) and 300 (stopped), and again with the new
 *               parameters moved to values that must not change anything (Lane Mix, Feedback
 *               From/To, the sequencer, lane B on empty slots)
 *   lanes       two lanes with different latencies line up (impulse), Lane Mix blends them
 *   feedback    the loop repeats every kChunk samples and decays; 95 % on noise stays bounded
 *   sequencer   the steps, Steps, sync to the host beat and Glide
 *   keyed       Sidechain Ducker keys on the rack input delayed by the latency before it
 */

#include "NullModules.hpp"
#include "RackEngine.hpp"
#include "RackEngineV1.hpp"

#include <atomic>
#include <cmath>
#include <complex>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using rack::RackEngine;

static int gFailures = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            std::printf("  FAIL: " __VA_ARGS__);            \
            std::printf("\n");                              \
            ++gFailures;                                    \
        }                                                   \
    } while (0)

// ---- an identity module (optionally a pure delay that reports it as latency) -----------------

namespace identity {

static const DISTRHO::SliderInfo kSliders[] = {
    { "Latency", "latency", 0.0f, 4000.0f, 0.0f, "smp", 1.0f },
    { "Mute", "mute", 0.0f, 1.0f, 0.0f, "", 1.0f },
};

class IdentityModule final : public nulljsfx::Module {
public:
    void setSampleRate(double) override { std::fill(fL, fL + kLen, 0.0f); std::fill(fR, fR + kLen, 0.0f); }
    void setValue(uint32_t i, float v) override
    {
        if (i == 0)
            fLat = uint32_t(v);
        if (i == 1)
            fGain = v >= 0.5f ? 0.0f : 1.0f;
    }
    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t n,
                 const nulljsfx::ModuleTime&) override
    {
        for (uint32_t i = 0; i < n; ++i)
        {
            fL[fPos] = inL[i];
            fR[fPos] = inR[i];
            const uint32_t k = (fPos + kLen - fLat) % kLen;
            outL[i] = fL[k] * fGain;
            outR[i] = fR[k] * fGain;
            fPos = (fPos + 1) % kLen;
        }
    }
    uint32_t latency() const override { return fLat; }

private:
    static constexpr uint32_t kLen = 8192;
    float fL[kLen] = {}, fR[kLen] = {};
    uint32_t fPos = 0, fLat = 0;
    float fGain = 1.0f;
};

inline nulljsfx::Module* create() { return new IdentityModule(); }
static const nulljsfx::ModuleDesc kDesc = { "identity", "Identity", "Test", kSliders, 2, create };

} // namespace identity

static std::vector<const nulljsfx::ModuleDesc*> allModules(bool withIdentity)
{
    std::vector<const nulljsfx::ModuleDesc*> v(nullrack::kModules, nullrack::kModules + nullrack::kNumModules);
    if (withIdentity)
        v.push_back(&identity::kDesc);
    return v;
}

// ---- signals ---------------------------------------------------------------------------------

struct Stereo {
    std::vector<float> l, r;
};

// music-like test signal: tones, filtered noise and decaying bursts on every beat, about -12 dBFS
static Stereo testSignal(double sr, double seconds)
{
    const size_t n = size_t(sr * seconds);
    Stereo s;
    s.l.resize(n);
    s.r.resize(n);
    uint32_t seed = 12345;
    double lp = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = double(i) / sr;
        seed = seed * 1664525u + 1013904223u;
        const double noise = (double(seed >> 8) / double(1u << 24)) * 2.0 - 1.0;
        lp += 0.2 * (noise - lp);
        const double beat = std::fmod(t, 0.5);
        const double burst = std::exp(-beat * 30.0) * std::sin(2.0 * M_PI * 60.0 * beat);
        const double tone = 0.15 * std::sin(2.0 * M_PI * 220.0 * t) + 0.08 * std::sin(2.0 * M_PI * 1760.0 * t);
        s.l[i] = float(0.25 * burst + tone + 0.08 * lp);
        s.r[i] = float(0.25 * burst + 0.9 * tone + 0.08 * noise * 0.5 + 0.04 * lp);
    }
    return s;
}

static void render(RackEngine& e, const Stereo& in, Stereo& out, uint32_t block, RackEngine::Time t = {},
                   void (*perBlock)(RackEngine&, size_t, size_t) = nullptr)
{
    const size_t n = in.l.size();
    out.l.assign(n, 0.0f);
    out.r.assign(n, 0.0f);
    for (size_t pos = 0; pos < n; pos += block)
    {
        const uint32_t k = uint32_t(std::min<size_t>(block, n - pos));
        if (perBlock)
            perBlock(e, pos, n);
        e.process(in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, k, t);
        if (t.playing)
            t.beat += double(k) / e.sampleRate() * t.tempo / 60.0;
    }
}

// ---- presets ---------------------------------------------------------------------------------

static bool strieq(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
        if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
            return false;
    return *a == *b;
}

static void testPresets()
{
    std::printf("presets (%u)\n", rack::kNumPresets);
    auto mods = allModules(false);
    std::vector<std::string> skipped;
    for (uint32_t p = 0; p < rack::kNumPresets; ++p)
    {
        const rack::Preset& pr = rack::kPresets[p];
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        std::string missing;
        for (uint32_t s = 0; s < pr.numSlots; ++s)
            if (!e->find(pr.slots[s].id))
                missing += std::string(missing.empty() ? "" : ", ") + pr.slots[s].id;
        if (!missing.empty())
        {
#ifdef RACK_FULL_BUILD
            CHECK(false, "%s: modules missing in a full build: %s", pr.name, missing.c_str());
#else
            skipped.push_back(std::string(pr.name) + " (" + missing + ")");
#endif
            continue;
        }
        // the generated slider names must be the modules' slider names
        for (uint32_t s = 0; s < pr.numSlots; ++s)
        {
            const nulljsfx::ModuleDesc* d = e->find(pr.slots[s].id);
            for (uint32_t k = 0; k < pr.slots[s].numValues; ++k)
            {
                const rack::PresetValue& v = pr.slots[s].values[k];
                CHECK(v.index < d->numSliders && strieq(d->sliders[v.index].name, v.name), "%s: %s slider %u is %s, not %s",
                      pr.name, d->id, unsigned(v.index) + 1, v.index < d->numSliders ? d->sliders[v.index].name : "-", v.name);
            }
        }
        for (uint32_t r = 0; r < pr.numRoutes; ++r)
        {
            const rack::PresetRoute& rt = pr.routes[r];
            const nulljsfx::ModuleDesc* d = rt.slot < pr.numSlots ? e->find(pr.slots[rt.slot].id) : nullptr;
            CHECK(d && rt.value < d->numSliders && strieq(d->sliders[rt.value].name, rt.name), "%s: route %u target %s",
                  pr.name, r + 1, rt.name);
        }
        double peak[2] = {}, rms[2] = {};
        for (int k = 0; k < 2; ++k)
        {
            const double sr = k ? 96000.0 : 44100.0;
            e->setSampleRate(sr);
            std::string err;
            CHECK(e->loadPreset(pr, &err), "%s: %s", pr.name, err.c_str());
            const Stereo in = testSignal(sr, 4.0);
            Stereo out;
            RackEngine::Time t;
            t.tempo = 120.0;
            t.playing = true;
            t.valid = true;
            // sweep Macro 1 from 0 to 100% over the render (the macro presets' main control)
            render(*e, in, out, 512, t, [](RackEngine& en, size_t pos, size_t n) {
                en.setParam(rack::kMacro1, 100.0f * float(pos) / float(n));
            });
            bool finite = true;
            double sum = 0.0;
            for (size_t i = 0; i < out.l.size(); ++i)
            {
                finite = finite && std::isfinite(out.l[i]) && std::isfinite(out.r[i]);
                peak[k] = std::max(peak[k], double(std::max(std::fabs(out.l[i]), std::fabs(out.r[i]))));
                if (i >= out.l.size() / 4)
                    sum += double(out.l[i]) * out.l[i] + double(out.r[i]) * out.r[i];
            }
            rms[k] = std::sqrt(sum / (out.l.size() * 1.5));
            CHECK(finite, "%s at %.0f Hz: not finite", pr.name, sr);
            CHECK(peak[k] < 4.0, "%s at %.0f Hz: peak %.2f", pr.name, sr, peak[k]);
            CHECK(rms[k] > 1e-3, "%s at %.0f Hz: silent (rms %.2g)", pr.name, sr, rms[k]);
        }
        std::printf("  %-22s %-8s %u slots %u routes  peak %.2f / %.2f  rms %.3f / %.3f  latency %u\n", pr.name,
                    pr.category, pr.numSlots, pr.numRoutes, peak[0], peak[1], rms[0], rms[1], e->latency());
    }
    for (const std::string& s : skipped)
        std::printf("  skipped (modules not in this build): %s\n", s.c_str());
}

// ---- one slot == the module alone ------------------------------------------------------------

static void testExact(const char* id, const std::vector<std::pair<uint32_t, float>>& norms)
{
    auto mods = allModules(false);
    std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
    const nulljsfx::ModuleDesc* d = e->find(id);
    if (!d)
    {
        std::printf("  %s: not in this build, skipped\n", id);
        return;
    }
    for (double sr : { 44100.0, 96000.0 })
    {
        e->setSampleRate(sr);
        e->setSlot(0, "");
        e->setSlot(0, id);
        std::unique_ptr<nulljsfx::Module> m(d->create());
        m->setSampleRate(sr);
        for (uint32_t v = 0; v < d->numSliders; ++v)
            m->setValue(v, d->sliders[v].def);
        for (auto& nv : norms)
        {
            e->setParam(rack::valueParam(0, nv.first), nv.second);
            m->setValue(nv.first, e->plainValue(0, nv.first, nv.second));
        }
        const Stereo in = testSignal(sr, 2.0);
        Stereo a, b;
        render(*e, in, a, 300); // host blocks of 300: the engine runs 128-sample chunks
        b.l.assign(in.l.size(), 0.0f);
        b.r.assign(in.l.size(), 0.0f);
        const nulljsfx::ModuleTime mt;
        size_t pos = 0;
        // the same chunks as the engine (128 within each 300-sample host block)
        while (pos < in.l.size())
        {
            const size_t blockEnd = std::min(in.l.size(), pos + 300);
            for (size_t p = pos; p < blockEnd; p += RackEngine::kChunk)
            {
                const uint32_t k = uint32_t(std::min<size_t>(RackEngine::kChunk, blockEnd - p));
                m->process(in.l.data() + p, in.r.data() + p, b.l.data() + p, b.r.data() + p, k, mt);
            }
            pos = blockEnd;
        }
        size_t diffs = 0;
        for (size_t i = 0; i < in.l.size(); ++i)
            diffs += (a.l[i] != b.l[i]) + (a.r[i] != b.r[i]);
        CHECK(diffs == 0, "%s at %.0f Hz: %zu samples differ from the module alone", id, sr, diffs);
        std::printf("  %-8s %6.0f Hz  bit-exact: %s\n", id, sr, diffs == 0 ? "yes" : "NO");
    }
}

// ---- bands -----------------------------------------------------------------------------------

static double magDb(const std::vector<float>& ir, double f, double sr)
{
    std::complex<double> h(0.0, 0.0);
    const double w = -2.0 * M_PI * f / sr;
    for (size_t i = 0; i < ir.size(); ++i)
        if (ir[i] != 0.0f)
            h += double(ir[i]) * std::polar(1.0, w * double(i));
    return 20.0 * std::log10(std::abs(h) + 1e-12);
}

static void testBands()
{
    auto mods = allModules(true);
    const double sr = 48000.0;
    for (float lat : { 0.0f, 333.0f })
        for (int band = rack::kBandLow; band <= rack::kBandHigh; ++band)
        {
            std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
            e->setSampleRate(sr);
            e->setSlot(0, "identity");
            e->setParam(rack::valueParam(0, 0), lat / 4000.0f);
            e->setParam(rack::slotParam(0, rack::kSlotBand), float(band));
            const size_t n = 16384;
            Stereo in, out;
            in.l.assign(n, 0.0f);
            in.r.assign(n, 0.0f);
            in.l[0] = in.r[0] = 1.0f;
            render(*e, in, out, 512);
            CHECK(e->latency() == uint32_t(lat), "latency %u, expected %.0f", e->latency(), lat);
            // magnitude response at 120 log-spaced frequencies, 20 Hz .. 20 kHz
            double worst = 0.0;
            for (int k = 0; k < 120; ++k)
                worst = std::max(worst, std::fabs(magDb(out.l, 20.0 * std::pow(1000.0, k / 119.0), sr)));
            CHECK(worst < 0.01, "band %s latency %.0f: %.4f dB off flat", rack::kBandOptions[band], lat, worst);
            // and the module really gets only its band: muted, that band is gone (crossovers 200 / 2500 Hz)
            e->setParam(rack::valueParam(0, 1), 1.0f);
            render(*e, in, out, 512);
            const double fIn = band == rack::kBandLow ? 50.0 : band == rack::kBandMid ? 700.0 : 10000.0;
            const double fOut = band == rack::kBandHigh ? 50.0 : 10000.0;
            const double gIn = magDb(out.l, fIn, sr), gOut = magDb(out.l, fOut, sr);
            CHECK(gIn < -20.0 && std::fabs(gOut) < 0.1, "band %s muted: %.1f dB in band, %.2f dB outside", rack::kBandOptions[band], gIn, gOut);
            std::printf("  band %-4s latency %3.0f: max deviation %.5f dB; muted %.1f dB at %.0f Hz, %.2f dB at %.0f Hz\n",
                        rack::kBandOptions[band], lat, worst, gIn, fIn, gOut, fOut);
        }
}

// ---- channels --------------------------------------------------------------------------------

static void testChannels()
{
    auto mods = allModules(true);
    const double sr = 44100.0;
    const Stereo in = testSignal(sr, 1.0);
    for (const char* id : { "identity", "tilt" })
        for (int ch = rack::kChanMid; ch <= rack::kChanRight; ++ch)
        {
            std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
            if (!e->find(id))
                continue;
            e->setSampleRate(sr);
            e->setSlot(0, id);
            if (std::string(id) == "tilt")
                e->setParam(rack::valueParam(0, 0), 0.9f); // Amount +80: a strong tilt
            e->setParam(rack::slotParam(0, rack::kSlotChannel), float(ch));
            Stereo out;
            render(*e, in, out, 512);
            double err = 0.0, moved = 0.0;
            for (size_t i = 0; i < in.l.size(); ++i)
            {
                double keep, keepIn, proc, procIn;
                switch (ch)
                {
                case rack::kChanMid:  keep = (out.l[i] - double(out.r[i])) * 0.5; keepIn = (in.l[i] - double(in.r[i])) * 0.5;
                                      proc = (out.l[i] + double(out.r[i])) * 0.5; procIn = (in.l[i] + double(in.r[i])) * 0.5; break;
                case rack::kChanSide: keep = (out.l[i] + double(out.r[i])) * 0.5; keepIn = (in.l[i] + double(in.r[i])) * 0.5;
                                      proc = (out.l[i] - double(out.r[i])) * 0.5; procIn = (in.l[i] - double(in.r[i])) * 0.5; break;
                case rack::kChanLeft: keep = out.r[i]; keepIn = in.r[i]; proc = out.l[i]; procIn = in.l[i]; break;
                default:              keep = out.l[i]; keepIn = in.l[i]; proc = out.r[i]; procIn = in.r[i]; break;
                }
                err = std::max(err, std::fabs(keep - keepIn));
                moved = std::max(moved, std::fabs(proc - procIn));
            }
            const bool lr = ch == rack::kChanLeft || ch == rack::kChanRight;
            const double tol = lr ? 0.0 : 1e-6; // M/S: the matrix rounds in float
            CHECK(err <= tol, "%s on %s: untouched component off by %.3g", id, rack::kChannelOptions[ch], err);
            if (std::string(id) == "tilt")
                CHECK(moved > 1e-3, "tilt on %s: processed component unchanged", rack::kChannelOptions[ch]);
            std::printf("  %-8s on %-5s untouched component max error %.3g%s, processed changed by %.3g\n", id,
                        rack::kChannelOptions[ch], err, lr ? " (bit-exact required)" : "", moved);
        }
}

// ---- modulation ------------------------------------------------------------------------------

static void testModulation()
{
    auto mods = allModules(false);
    const double sr = 44100.0;
    const Stereo in = testSignal(sr, 2.0);
    if (!RackEngine(mods.data(), uint32_t(mods.size())).find("tilt"))
    {
        std::printf("  tilt not in this build, skipped\n");
        return;
    }
    {
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        e->setSampleRate(sr);
        e->setSlot(0, "tilt");
        e->setParam(rack::kLfo1Rate, 2.0f);
        rack::Route r;
        r.source = rack::kSrcLfo1;
        r.slot = 0;
        r.value = 0;
        r.depth = 0.5f;
        e->setRoutes(&r, 1);
        float lo = 1.0f, hi = 0.0f;
        Stereo out;
        out.l.resize(512);
        out.r.resize(512);
        for (size_t pos = 0; pos + 512 <= in.l.size(); pos += 512)
        {
            e->process(in.l.data() + pos, in.r.data() + pos, out.l.data(), out.r.data(), 512, {});
            lo = std::min(lo, e->effective(0, 0));
            hi = std::max(hi, e->effective(0, 0));
        }
        CHECK(hi - lo > 0.9f, "LFO route moved the value only %.3f..%.3f", lo, hi);
        std::printf("  LFO 1 -> tilt amount, depth 0.5: effective value %.3f .. %.3f\n", lo, hi);
    }
    for (int k = 0; k < 2; ++k)
    {
        // a: no routes; b: macro 1 at 0 with depth 0.7 (k = 0) or macro at 80% with depth 0 (k = 1)
        Stereo a, b;
        for (int pass = 0; pass < 2; ++pass)
        {
            std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
            e->setSampleRate(sr);
            e->setSlot(0, "tilt");
            e->setParam(rack::valueParam(0, 0), 0.3f);
            if (pass)
            {
                rack::Route r;
                r.source = rack::kSrcMacro1;
                r.depth = k ? 0.0f : 0.7f;
                e->setParam(rack::kMacro1, k ? 80.0f : 0.0f);
                e->setRoutes(&r, 1);
            }
            render(*e, in, pass ? b : a, 512);
        }
        size_t diffs = 0;
        for (size_t i = 0; i < a.l.size(); ++i)
            diffs += (a.l[i] != b.l[i]) + (a.r[i] != b.r[i]);
        CHECK(diffs == 0, "%s changed %zu samples", k ? "macro 80% depth 0" : "macro 0 depth 0.7", diffs);
        std::printf("  %s: %s\n", k ? "macro at 80%, depth 0  " : "macro at 0, depth 0.7   ", diffs ? "CHANGED" : "no change");
    }
}

// ---- threads ---------------------------------------------------------------------------------

static void testThreads()
{
    auto mods = allModules(false);
    std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
    e->setSampleRate(48000.0);
    std::atomic<bool> stop { false };
    std::thread audio([&] {
        std::vector<float> l(256, 0.1f), r(256, -0.1f), ol(256), orr(256);
        while (!stop.load())
            e->process(l.data(), r.data(), ol.data(), orr.data(), 256, {});
    });
    const char* ids[] = { "tilt", "delay", "" };
    for (int i = 0; i < 300; ++i)
    {
        e->setSlot(uint32_t(i % 8), ids[i % 3]);
        rack::Route r;
        r.source = i % rack::kNumSources;
        r.slot = i % 8;
        r.value = 0;
        r.depth = 0.25f;
        e->setRoutes(&r, 1);
    }
    stop = true;
    audio.join();
    std::printf("  300 module swaps while processing: ok\n");
}

// ---- state strings ---------------------------------------------------------------------------

static void testState()
{
    rack::Route r[4];
    r[0].source = rack::kSrcLfo1; r[0].slot = 1; r[0].value = 0; r[0].depth = -0.25f;
    r[1].source = rack::kSrcEnv; r[1].slot = 7; r[1].value = 24; r[1].depth = 1.0f;
    r[2].source = rack::kSrcMacro1 + 7; r[2].slot = 0; r[2].value = 3; r[2].depth = 0.5f;
    r[3].source = rack::kSrcSeq; r[3].slot = 2; r[3].value = 0; r[3].depth = 0.75f;
    const std::string s = rack::formatRoutes(r, 4);
    rack::Route back[rack::kMaxRoutes];
    const uint32_t n = rack::parseRoutes(s.c_str(), back);
    CHECK(n == 4 && rack::formatRoutes(back, n) == s, "routes round trip: %s", s.c_str());
    // a 2.1 routes string means the same
    CHECK(rack::parseRoutes("lfo1>2.1:-0.25;macro8>1.4:0.5", back) == 2 && back[0].source == rack::kSrcLfo1 &&
          back[1].source == rack::kSrcMacro1 + 7 && back[1].value == 3, "2.1 routes string");
    // the 2.1 parameters keep their indices and symbols; the new ones come after them
    {
        char nm[32], sy[32];
        CHECK(rack::kExtFirst == 254 && std::string(rack::paramInfo(253, nm, 32, sy, 32).symbol) == "s8_v25" &&
              std::string(rack::paramInfo(rack::kSeqStep1 + 15, nm, 32, sy, 32).symbol) == "seq_step16" &&
              std::string(rack::paramInfo(rack::laneParam(7), nm, 32, sy, 32).symbol) == "s8_lane" &&
              rack::kNumParams == 286, "parameter layout");
        CHECK(rack::globalFromSymbol("feedback") == int(rack::kFeedback) && rack::globalFromSymbol("seq_step3") == int(rack::kSeqStep1 + 2) &&
              rack::globalFromSymbol("env_gain") == int(rack::kEnvGain) && rack::globalFromSymbol("s1_lane") < 0, "global symbols");
    }
    CHECK(rack::parseRoutes("bogus;lfo1>9.1:0.5;env>1.26:0.5;lfo2>1.1:x", back) == 0, "bad routes accepted");
    std::string ids[rack::kSlots] = { "tilt", "", "delay" };
    std::string ids2[rack::kSlots];
    rack::parseSlots(rack::formatSlots(ids).c_str(), ids2);
    bool same = true;
    for (uint32_t i = 0; i < rack::kSlots; ++i)
        same = same && ids[i] == ids2[i];
    CHECK(same, "slots round trip");
    std::printf("  routes \"%s\" and slots round-trip\n", s.c_str());

    // a reorder moves the module instances with their slot parameters
    auto mods = allModules(false);
    std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
    if (e->find("tilt") && e->find("delay"))
    {
        e->setSlots(ids);
        e->setParam(rack::valueParam(0, 0), 0.8f);
        e->setParam(rack::valueParam(2, 1), 0.6f);
        e->setParam(rack::laneParam(0), 1.0f);
        std::string swapped[rack::kSlots] = { "delay", "", "tilt" };
        e->setSlots(swapped);
        CHECK(e->param(rack::valueParam(2, 0)) == 0.8f && e->param(rack::valueParam(0, 1)) == 0.6f,
              "reorder lost the values");
        CHECK(e->param(rack::laneParam(2)) == 1.0f && e->param(rack::laneParam(0)) == 0.0f, "reorder lost the lane");
        // a new module resets its values to the defaults
        e->setSlot(2, "delay");
        CHECK(std::fabs(e->plainValue(2, 0, e->param(rack::valueParam(2, 0))) - 250.0f) < 0.5f, "new module not at defaults");
        std::printf("  reorder keeps values, a new module starts at its defaults\n");
    }
}

// ---- identity: with the new features off, bit for bit the 2.1 engine -------------------------

// renders with any engine (RackEngine or rack_v1::RackEngine); neutral() changes the new
// parameters in ways that must not change the sound (RackEngine only)
template <class Engine>
static void renderAny(Engine& e, const Stereo& in, Stereo& out, uint32_t block, bool playing)
{
    typename Engine::Time t;
    t.tempo = 120.0;
    t.playing = playing;
    t.valid = playing;
    const size_t n = in.l.size();
    out.l.assign(n, 0.0f);
    out.r.assign(n, 0.0f);
    for (size_t pos = 0; pos < n; pos += block)
    {
        const uint32_t k = uint32_t(std::min<size_t>(block, n - pos));
        e.setParam(rack::kMacro1, 100.0f * float(pos) / float(n));
        e.process(in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, k, t);
        if (t.playing)
            t.beat += double(k) / e.sampleRate() * t.tempo / 60.0;
    }
}

static void neutral(RackEngine& e, const RackEngine::PresetState& st)
{
    e.setParam(rack::kLaneMix, 100.0f);    // no slot on lane B: Lane Mix does nothing
    e.setParam(rack::kFbFrom, 6.0f);       // feedback 0: the loop does nothing
    e.setParam(rack::kFbTo, 2.0f);
    e.setParam(rack::kSeqLength, 5.0f);    // no seq route: the sequencer moves nothing
    e.setParam(rack::kSeqSync, 0.0f);
    e.setParam(rack::kSeqRate, 13.0f);
    e.setParam(rack::kSeqGlide, 70.0f);
    e.setParam(rack::kSeqStep1 + 3, 0.9f);
    for (uint32_t s = 0; s < rack::kSlots; ++s)
        if (st.ids[s].empty())
            e.setParam(rack::laneParam(s), 1.0f); // lane B on an empty slot: no lane B
}

static bool usesNewFeatures(const RackEngine::PresetState& st)
{
    if (st.params[rack::kFeedback] > 0.0f)
        return true;
    for (uint32_t s = 0; s < rack::kSlots; ++s)
        if (!st.ids[s].empty() && st.params[rack::laneParam(s)] >= 0.5f)
            return true;
    for (uint32_t r = 0; r < st.numRoutes; ++r)
        if (st.routes[r].source >= rack_v1::kNumSources)
            return true;
    return false;
}

static void testIdentity()
{
    auto mods = allModules(false);
    uint32_t compared = 0, skipped = 0;
    size_t samples = 0;
    for (uint32_t p = 0; p < rack::kNumPresets; ++p)
    {
        const rack::Preset& pr = rack::kPresets[p];
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        std::unique_ptr<RackEngine::PresetState> st(new RackEngine::PresetState());
        bool have = true;
        for (uint32_t s = 0; s < pr.numSlots; ++s)
            have = have && e->find(pr.slots[s].id);
        e->resolvePreset(pr, *st);
        if (!have || usesNewFeatures(*st))
        {
            ++skipped;
            continue;
        }
        size_t diffs = 0;
        // pass: 0 = 44.1 kHz, blocks of 512, playing; 1 = 96 kHz, 512, playing; 2 = 44.1 kHz,
        // 300, stopped; 3 = as 0 with the neutral changes
        for (int pass = 0; pass < 4; ++pass)
        {
            const double sr = pass == 1 ? 96000.0 : 44100.0;
            const uint32_t block = pass == 2 ? 300 : 512;
            std::unique_ptr<rack_v1::RackEngine> v1(new rack_v1::RackEngine(mods.data(), uint32_t(mods.size())));
            e.reset(new RackEngine(mods.data(), uint32_t(mods.size())));
            v1->setSampleRate(sr);
            e->setSampleRate(sr);
            v1->loadPreset(pr);
            e->loadPreset(pr);
            if (pass == 3)
                neutral(*e, *st);
            const Stereo in = testSignal(sr, 3.0);
            Stereo a, b;
            renderAny(*v1, in, a, block, pass != 2);
            renderAny(*e, in, b, block, pass != 2);
            for (size_t i = 0; i < in.l.size(); ++i)
                diffs += (a.l[i] != b.l[i]) + (a.r[i] != b.r[i]);
            samples += 2 * in.l.size();
            CHECK(e->latency() == v1->latency(), "%s: latency %u, 2.1 %u", pr.name, e->latency(), v1->latency());
        }
        CHECK(diffs == 0, "%s: %zu samples differ from the 2.1 engine", pr.name, diffs);
        ++compared;
    }
    std::printf("  %u presets bit-identical to the 2.1 engine (%zu samples, 4 passes each); %u skipped "
                "(they use the new features or modules not in this build)\n", compared, samples, skipped);
#ifdef RACK_FULL_BUILD
    CHECK(compared >= 52, "only %u presets compared", compared);
#endif
}

// ---- lanes -----------------------------------------------------------------------------------

static void testLanes()
{
    auto mods = allModules(true);
    const double sr = 48000.0;
    const size_t n = 4096;
    Stereo in, out;
    in.l.assign(n, 0.0f);
    in.r.assign(n, 0.0f);
    in.l[0] = 1.0f;
    in.r[0] = 0.5f;
    for (float mixB : { 0.0f, 50.0f, 100.0f })
    {
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        e->setSampleRate(sr);
        std::string ids[rack::kSlots] = { "identity", "identity", "identity" };
        e->setSlots(ids);
        e->setParam(rack::valueParam(0, 0), 100.0f / 4000.0f); // lane A: 100 + 0 samples
        e->setParam(rack::valueParam(1, 0), 333.0f / 4000.0f); // lane B: 333 + 50 samples
        e->setParam(rack::valueParam(2, 0), 50.0f / 4000.0f);
        e->setParam(rack::laneParam(1), 1.0f);
        e->setParam(rack::laneParam(2), 1.0f);
        e->setParam(rack::kLaneMix, mixB);
        render(*e, in, out, 512);
        // both lanes line up at the longer lane's 383 samples: A's part (1 - m) plus B's m
        size_t bad = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const float wantL = i == 383 ? 1.0f : 0.0f, wantR = i == 383 ? 0.5f : 0.0f;
            bad += std::fabs(out.l[i] - wantL) > 1e-6f || std::fabs(out.r[i] - wantR) > 1e-6f;
        }
        CHECK(e->latency() == 383, "lanes: latency %u, expected 383", e->latency());
        CHECK(bad == 0, "lanes, mix %.0f: %zu samples off the aligned impulse", mixB, bad);
        // lane B muted: only A's share is left
        e->setParam(rack::valueParam(1, 1), 1.0f);
        render(*e, in, out, 512);
        CHECK(std::fabs(out.l[383] - (1.0f - mixB / 100.0f)) < 1e-6f, "lanes, mix %.0f, B muted: %.4f", mixB, out.l[383]);
        std::printf("  lane A 100 smp, lane B 383 smp, Lane Mix %3.0f: aligned at 383, latency %u; B muted leaves %.2f\n",
                    mixB, e->latency(), out.l[383]);
    }
}

// ---- feedback --------------------------------------------------------------------------------

static void testFeedback()
{
    auto mods = allModules(true);
    const double sr = 48000.0;
    {
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        e->setSampleRate(sr);
        e->setSlot(1, "identity");
        e->setParam(rack::valueParam(1, 0), 200.0f / 4000.0f); // a 200-sample "delay"
        e->setParam(rack::kFeedback, 50.0f);
        e->setParam(rack::kFbFrom, 2.0f);
        e->setParam(rack::kFbTo, 2.0f);
        const size_t n = 8192;
        Stereo in, out;
        in.l.assign(n, 0.0f);
        in.r.assign(n, 0.0f);
        for (size_t i = 0; i < 64; ++i) // a short 1 kHz burst (the loop cuts DC and lows)
            in.l[i] = in.r[i] = 0.5f * float(std::sin(2.0 * M_PI * 1000.0 * double(i) / sr));
        render(*e, in, out, 512);
        // repeats every 200 + kChunk samples, each quieter
        double prev = 1e9;
        bool decays = true;
        for (int k = 0; k < 4; ++k)
        {
            double pk = 0.0;
            const size_t at = 200 + size_t(k) * (200 + RackEngine::kChunk);
            for (size_t i = at; i < at + 64; ++i)
                pk = std::max(pk, double(std::fabs(out.l[i])));
            CHECK(pk > 1e-3, "feedback: repeat %d missing (peak %.4f)", k, pk);
            decays = decays && pk < prev;
            prev = pk;
            std::printf("  repeat %d at %4zu samples: peak %.4f\n", k, at, pk);
        }
        CHECK(decays, "feedback: the repeats do not decay");
        // nothing between the repeats
        double gap = 0.0;
        for (size_t i = 300; i < 328; ++i)
            gap = std::max(gap, double(std::fabs(out.l[i])));
        CHECK(gap < 1e-6, "feedback: signal between the repeats (%.3g)", gap);
    }
    {
        // 95 % around a chain with gain stays bounded
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        if (e->find("saturation") && e->find("delay"))
        {
            e->setSampleRate(sr);
            std::string ids[rack::kSlots] = { "delay", "saturation" };
            e->setSlots(ids);
            e->setParam(rack::valueParam(0, 2), 1.0f); // delay Mix 100
            e->setParam(rack::valueParam(1, 0), 1.0f); // Drive 100
            e->setParam(rack::kFeedback, 95.0f);
            e->setParam(rack::kFbFrom, 1.0f);
            e->setParam(rack::kFbTo, 2.0f);
            const Stereo in = testSignal(sr, 4.0);
            Stereo out;
            render(*e, in, out, 333);
            double pk = 0.0;
            bool finite = true;
            for (size_t i = 0; i < out.l.size(); ++i)
            {
                finite = finite && std::isfinite(out.l[i]) && std::isfinite(out.r[i]);
                pk = std::max(pk, double(std::max(std::fabs(out.l[i]), std::fabs(out.r[i]))));
            }
            CHECK(finite && pk < 4.0, "feedback 95%%: peak %.2f%s", pk, finite ? "" : ", not finite");
            std::printf("  feedback 95%% around delay + saturation (drive 100): peak %.2f\n", pk);
        }
    }
}

// ---- step sequencer --------------------------------------------------------------------------

static void testSequencer()
{
    auto mods = allModules(true);
    const double sr = 48000.0;
    std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
    e->setSampleRate(sr);
    e->setSlot(0, "identity");
    e->setParam(rack::valueParam(0, 0), 0.0f);
    rack::Route r;
    r.source = rack::kSrcSeq;
    r.slot = 0;
    r.value = 0;
    r.depth = 1.0f;
    e->setRoutes(&r, 1);
    const float pattern[4] = { 0.0f, 1.0f, 0.25f, 0.75f };
    for (int k = 0; k < 4; ++k)
        e->setParam(rack::kSeqStep1 + uint32_t(k), pattern[k]);
    e->setParam(rack::kSeqLength, 4.0f);
    std::vector<float> l(RackEngine::kChunk, 0.0f), r2(RackEngine::kChunk, 0.0f), ol(RackEngine::kChunk), orr(RackEngine::kChunk);

    // free at 37.5 steps/s = one step per 10 chunks of 128 samples at 48 kHz: the values in order
    e->setParam(rack::kSeqSync, 0.0f);
    e->setParam(rack::kSeqRate, 37.5f);
    int k0 = -1, changes = 0;
    bool valuesOk = true;
    for (int c = 0; c < 2000; ++c)
    {
        e->process(l.data(), r2.data(), ol.data(), orr.data(), RackEngine::kChunk, {});
        const int k = e->seqStep();
        valuesOk = valuesOk && k < 4 && std::fabs(e->sourceValue(rack::kSrcSeq) - pattern[k]) < 1e-6f &&
                   std::fabs(e->effective(0, 0) - pattern[k]) < 1e-6f;
        if (k != k0)
        {
            CHECK(k0 < 0 || k == (k0 + 1) % 4, "sequencer: step %d after %d", k, k0);
            k0 = k;
            ++changes;
        }
    }
    // 2000 chunks = 5.33 s at 37.5 steps/s: 200 steps
    CHECK(valuesOk, "sequencer: the source is not the current step's value");
    CHECK(changes >= 195 && changes <= 205, "sequencer: %d steps in 5.33 s at 37.5 steps/s", changes);
    std::printf("  free 37.5 steps/s, 4 steps: %d steps in 5.33 s, values follow the pattern\n", changes);

    // synced 1/16 at 120 bpm, playing: the step is the host's beat position
    e->setParam(rack::kSeqSync, 7.0f); // 1/16 = 0.25 beat a step
    bool syncOk = true;
    for (double beat : { 0.0, 0.26, 0.5, 0.8, 1.0, 3.9, 17.3 })
    {
        RackEngine::Time t;
        t.tempo = 120.0;
        t.playing = t.valid = true;
        t.beat = beat;
        e->process(l.data(), r2.data(), ol.data(), orr.data(), RackEngine::kChunk, t);
        const int want = int(std::floor(beat / 0.25)) % 4;
        syncOk = syncOk && e->seqStep() == want;
    }
    CHECK(syncOk, "sequencer: synced steps do not follow the beat");
    std::printf("  synced 1/16: the step follows the host beat: %s\n", syncOk ? "yes" : "NO");

    // glide 100 %: values between the steps appear
    e->setParam(rack::kSeqSync, 0.0f);
    e->setParam(rack::kSeqRate, 4.0f);
    e->setParam(rack::kSeqGlide, 100.0f);
    int between = 0;
    for (int c = 0; c < 400; ++c)
    {
        e->process(l.data(), r2.data(), ol.data(), orr.data(), RackEngine::kChunk, {});
        const float v = e->sourceValue(rack::kSrcSeq);
        bool onStep = false;
        for (float p : pattern)
            onStep = onStep || std::fabs(v - p) < 1e-4f;
        between += !onStep;
    }
    CHECK(between > 200, "sequencer: glide 100%% gave only %d in-between values of 400", between);
    std::printf("  glide 100%%: %d of 400 chunks between steps\n", between);
}

// ---- keyed modules ---------------------------------------------------------------------------

static void testKeyed()
{
    auto mods = allModules(true);
    std::unique_ptr<RackEngine> probe(new RackEngine(mods.data(), uint32_t(mods.size())));
    if (!probe->find("sidechain_ducker"))
    {
        std::printf("  sidechain_ducker not in this build, skipped\n");
        return;
    }
    // after a 333-sample delay, Key = SIDE (the rack input, delayed by the 333 samples before
    // it) must duck exactly like Key = MAIN (the module's own input)
    const double sr = 44100.0;
    const Stereo in = testSignal(sr, 2.0);
    Stereo out[2];
    for (int k = 0; k < 2; ++k)
    {
        std::unique_ptr<RackEngine> e(new RackEngine(mods.data(), uint32_t(mods.size())));
        e->setSampleRate(sr);
        std::string ids[rack::kSlots] = { "identity", "sidechain_ducker" };
        e->setSlots(ids);
        e->setParam(rack::valueParam(0, 0), 333.0f / 4000.0f);
        e->setParam(rack::valueParam(1, 5), k ? 1.0f : 0.0f); // Key: SIDE, MAIN
        e->setParam(rack::valueParam(1, 0), 0.6f);            // Threshold -24 dB
        render(*e, in, out[k], 512);
    }
    size_t diffs = 0;
    double moved = 0.0;
    for (size_t i = 0; i < in.l.size(); ++i)
    {
        diffs += (out[0].l[i] != out[1].l[i]) + (out[0].r[i] != out[1].r[i]);
        if (i >= 333)
            moved = std::max(moved, double(std::fabs(out[0].l[i] - in.l[i - 333])));
    }
    CHECK(diffs == 0, "keyed: Key SIDE after 333 samples of latency differs from Key MAIN in %zu samples", diffs);
    CHECK(moved > 1e-3, "keyed: the ducker did not duck");
    std::printf("  sidechain_ducker after 333 samples: key = rack input, aligned (%s), ducked by up to %.3f\n",
                diffs ? "NO" : "bit-exact", moved);
}

int main()
{
    std::printf("Rack engine test, %u modules in this build\n", nullrack::kNumModules);
    testState();
    std::printf("exact\n");
    testExact("tilt", { { 0, 0.8f } });
    testExact("delay", { { 0, 0.2f }, { 1, 0.5f }, { 2, 1.0f }, { 5, 1.0f }, { 7, 0.6f }, { 8, 0.4f } });
    testExact("sidechain_ducker", { { 0, 0.7f }, { 1, 0.2f }, { 6, 0.3f } });
    testExact("convolution", { { 0, 0.25f }, { 1, 0.5f }, { 2, 0.1f } });
    std::printf("identity\n");
    testIdentity();
    std::printf("lanes\n");
    testLanes();
    std::printf("feedback\n");
    testFeedback();
    std::printf("sequencer\n");
    testSequencer();
    std::printf("keyed\n");
    testKeyed();
    std::printf("bands\n");
    testBands();
    std::printf("channels\n");
    testChannels();
    std::printf("modulation\n");
    testModulation();
    std::printf("threads\n");
    testThreads();
    testPresets();
    std::printf("%s (%d failure%s)\n", gFailures ? "FAIL" : "PASS", gFailures, gFailures == 1 ? "" : "s");
    return gFailures ? 1 : 0;
}
