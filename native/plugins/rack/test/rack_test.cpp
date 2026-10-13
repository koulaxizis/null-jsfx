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
 */

#include "NullModules.hpp"
#include "RackEngine.hpp"

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
    rack::Route r[3];
    r[0].source = rack::kSrcLfo1; r[0].slot = 1; r[0].value = 0; r[0].depth = -0.25f;
    r[1].source = rack::kSrcEnv; r[1].slot = 7; r[1].value = 24; r[1].depth = 1.0f;
    r[2].source = rack::kSrcMacro1 + 7; r[2].slot = 0; r[2].value = 3; r[2].depth = 0.5f;
    const std::string s = rack::formatRoutes(r, 3);
    rack::Route back[rack::kMaxRoutes];
    const uint32_t n = rack::parseRoutes(s.c_str(), back);
    CHECK(n == 3 && rack::formatRoutes(back, n) == s, "routes round trip: %s", s.c_str());
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
        std::string swapped[rack::kSlots] = { "delay", "", "tilt" };
        e->setSlots(swapped);
        CHECK(e->param(rack::valueParam(2, 0)) == 0.8f && e->param(rack::valueParam(0, 1)) == 0.6f,
              "reorder lost the values");
        // a new module resets its values to the defaults
        e->setSlot(2, "delay");
        CHECK(std::fabs(e->plainValue(2, 0, e->param(rack::valueParam(2, 0))) - 250.0f) < 0.5f, "new module not at defaults");
        std::printf("  reorder keeps values, a new module starts at its defaults\n");
    }
}

int main()
{
    std::printf("Rack engine test, %u modules in this build\n", nullrack::kNumModules);
    testState();
    std::printf("exact\n");
    testExact("tilt", { { 0, 0.8f } });
    testExact("delay", { { 0, 0.2f }, { 1, 0.5f }, { 2, 1.0f }, { 5, 1.0f }, { 7, 0.6f }, { 8, 0.4f } });
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
