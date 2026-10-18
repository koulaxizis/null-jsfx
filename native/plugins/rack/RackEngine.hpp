/*
 * NULL JSFX - Rack: the engine (framework-free, used by Plugin.cpp and test/rack_test.cpp)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Up to 8 modules in series. Per slot: On, Mix (dry delayed by the module's latency), Band (the
 * slot's own Linkwitz-Riley 4th-order crossovers at the two global frequencies; the module runs
 * on one band and the other bands, delayed by its latency, are added back: LP^2 + HP^2 is an
 * allpass, so what the module leaves alone stays flat) and Channel (Mid, Side, Left or Right:
 * the module gets that component on both inputs, its output is averaged, the other component
 * passes, delayed by its latency).
 *
 * Modulation, every kChunk samples: Macro 1-8, two LFOs (free or tempo-synced; sine, triangle,
 * saw up/down, square, random S&H by an exact Park-Miller step), an envelope follower of the
 * rack input and (added after 2.1) a 16-step sequencer (Steps 1..16, free in Hz or synced, Glide: a linear
 * ramp from the previous step over that share of each step), routed to slot values with depth
 * -1..1 of the value's range (log sliders in log space). A module's setValue is called only when
 * its effective (snapped) value changes.
 *
 * Added after 2.1, each off at its default so 2.1 sessions render bit for bit as before
 * (rack_test "identity" checks it against a copy of the 2.1 engine):
 * - Lanes: each slot is on lane A or B. With a slot on B, lane A and lane B each run the rack
 *   input through their own slots in order; the shorter lane is delayed to line up with the
 *   longer one (the rack reports the longer lane's latency) and Lane Mix blends them (0 = A,
 *   100 = B). With no slot on B, the rack is the single serial chain of 2.1, whatever Lane Mix.
 * - Feedback: the output of slot To is fed back into the input of slot From (From > To are
 *   swapped) with a delay of kChunk samples (the processing block, whatever the host's block
 *   size), through a DC blocker, a 40 Hz low cut, a 12 kHz high cut and a tanh soft clip.
 *   At 0 the loop is not computed.
 * - Keyed modules (RackKeyed.hpp, Sidechain Ducker) get the rack input as their key, delayed by
 *   the latency of the slots before them in their lane.
 *
 * Threads: parameters are atomics (any thread). Module changes, routes and the sample rate are
 * set off the audio thread: the new module is created and prepared there, then swapped in under
 * a try-lock that process() takes per block; when it cannot, process() outputs the input delayed
 * by the last reported latency. Old modules are deleted outside the lock, never in process().
 * process() does not allocate.
 */

#pragma once

#include "NullModule.hpp"
#include "RackKeyed.hpp"
#include "RackParams.hpp"
#include "RackPresets.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace rack {

class SpinLock {
public:
    bool tryLock() noexcept { return !fFlag.test_and_set(std::memory_order_acquire); }
    void lock() noexcept
    {
        while (fFlag.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }
    void unlock() noexcept { fFlag.clear(std::memory_order_release); }

private:
    std::atomic_flag fFlag = ATOMIC_FLAG_INIT;
};

inline bool isLogSlider(const char* id, uint32_t index)
{
    for (uint32_t i = 0; i < kNumLogSliders; ++i)
        if (kLogSliders[i].index == index && std::strcmp(kLogSliders[i].id, id) == 0)
            return true;
    return false;
}

// ---- small DSP pieces ------------------------------------------------------------------------

// stereo delay line with a fixed power-of-two capacity, allocated once
struct DelayLine {
    std::vector<float> l, r;
    uint32_t mask = 0, pos = 0;

    void allocate(uint32_t frames) // frames: a power of two
    {
        l.assign(frames, 0.0f);
        r.assign(frames, 0.0f);
        mask = frames - 1;
        pos = 0;
    }
    void clear()
    {
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
    }
    uint32_t maxDelay() const { return mask; }
    // clears the last `frames` samples written (what delays up to frames - 1 read next)
    void clearRecent(uint32_t frames)
    {
        frames = std::min(frames, mask + 1);
        for (uint32_t i = 1; i <= frames; ++i)
            l[(pos - i) & mask] = r[(pos - i) & mask] = 0.0f;
    }
    // writes x, returns the sample written d samples ago (d = 0: x itself)
    inline void process(float xl, float xr, uint32_t d, float& yl, float& yr)
    {
        l[pos] = xl;
        r[pos] = xr;
        const uint32_t k = (pos - d) & mask;
        yl = l[k];
        yr = r[k];
        pos = (pos + 1) & mask;
    }
};

// stereo ring written a chunk at a time: read() taps it relative to the chunk being written
// (d = 0: the chunk's own samples, once written), advance() moves on to the next chunk
struct ChunkRing {
    std::vector<float> l, r;
    uint32_t mask = 0, pos = 0;

    void allocate(uint32_t frames) // frames: a power of two
    {
        l.assign(frames, 0.0f);
        r.assign(frames, 0.0f);
        mask = frames - 1;
        pos = 0;
    }
    void clear()
    {
        std::fill(l.begin(), l.end(), 0.0f);
        std::fill(r.begin(), r.end(), 0.0f);
    }
    uint32_t maxDelay(uint32_t chunk) const { return mask + 1 - chunk; }
    void write(const float* xl, const float* xr, uint32_t n)
    {
        for (uint32_t i = 0; i < n; ++i)
        {
            l[(pos + i) & mask] = xl[i];
            r[(pos + i) & mask] = xr[i];
        }
    }
    void read(uint32_t d, uint32_t n, float* ol, float* orr) const
    {
        for (uint32_t i = 0; i < n; ++i)
        {
            const uint32_t k = (pos + i - d) & mask;
            ol[i] = l[k];
            orr[i] = r[k];
        }
    }
    void advance(uint32_t n) { pos = (pos + n) & mask; }
};

// RBJ biquad, transposed direct form II in double
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    enum Type { kLow, kHigh, kAll };
    void set(Type t, double f, double sr, double q = 0.7071067811865476)
    {
        const double w = 2.0 * M_PI * f / sr, cw = std::cos(w), al = std::sin(w) / (2.0 * q), a0 = 1.0 + al;
        switch (t)
        {
        case kLow:  b0 = (1.0 - cw) * 0.5; b1 = 1.0 - cw; b2 = b0; break;
        case kHigh: b0 = (1.0 + cw) * 0.5; b1 = -(1.0 + cw); b2 = b0; break;
        case kAll:  b0 = 1.0 - al; b1 = -2.0 * cw; b2 = 1.0 + al; break;
        }
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - al) / a0;
        b0 /= a0; b1 /= a0; b2 /= a0;
    }
    void copyCoefs(const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
    void reset() { z1 = z2 = 0.0; }
    inline double process(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// three-band LR4 split of one channel: low (allpassed at f2 to match), mid and high
struct Split3 {
    Biquad lp1[2], hp1[2], lp2[2], hp2[2], ap2;

    void set(double f1, double f2, double sr)
    {
        Biquad l1, h1, l2, h2;
        l1.set(Biquad::kLow, f1, sr);
        h1.set(Biquad::kHigh, f1, sr);
        l2.set(Biquad::kLow, f2, sr);
        h2.set(Biquad::kHigh, f2, sr);
        for (int i = 0; i < 2; ++i)
        {
            lp1[i].copyCoefs(l1);
            hp1[i].copyCoefs(h1);
            lp2[i].copyCoefs(l2);
            hp2[i].copyCoefs(h2);
        }
        ap2.set(Biquad::kAll, f2, sr);
    }
    void reset()
    {
        for (int i = 0; i < 2; ++i)
        {
            lp1[i].reset(); hp1[i].reset(); lp2[i].reset(); hp2[i].reset();
        }
        ap2.reset();
    }
    inline void process(double x, double& lo, double& mid, double& hi)
    {
        lo = ap2.process(lp1[1].process(lp1[0].process(x)));
        const double h = hp1[1].process(hp1[0].process(x));
        mid = lp2[1].process(lp2[0].process(h));
        hi = hp2[1].process(hp2[0].process(h));
    }
};

// ---- the engine ------------------------------------------------------------------------------

class RackEngine {
public:
    static constexpr uint32_t kChunk = 128;          // modulation and processing block
    static constexpr uint32_t kSlotDelay = 1u << 16; // per-slot dry / pass-through capacity
    static constexpr uint32_t kRackDelay = 1u << 18; // the rack's own dry / fallback capacity

    struct Time {
        double tempo = 120.0;
        double beat = 0.0;      // beat position (quarter notes) at the start of the block
        bool playing = false;
        bool valid = false;     // the host reported a tempo/BBT
    };

    RackEngine(const nulljsfx::ModuleDesc* const* modules, uint32_t numModules)
        : fModules(modules), fNumModules(numModules)
    {
        for (uint32_t i = 0; i < kNumParams; ++i)
        {
            char n[32], s[32];
            fParams[i].store(paramInfo(i, n, sizeof(n), s, sizeof(s)).def, std::memory_order_relaxed);
        }
        for (Slot& s : fSlot)
        {
            s.dry.allocate(kSlotDelay);
            s.pass.allocate(kSlotDelay);
            for (auto& e : s.eff)
                e.store(0.0f, std::memory_order_relaxed);
        }
        fRackDry.allocate(kRackDelay);
        fAlignA.allocate(kRackDelay);
        fAlignB.allocate(kRackDelay);
        fKey.allocate(kRackDelay);
        fFbRing.allocate(2 * kChunk);
        for (auto& v : fSourceValue)
            v.store(0.0f, std::memory_order_relaxed);
        setSampleRate(44100.0);
    }

    ~RackEngine() = default;

    // ---- catalogue ---------------------------------------------------------------------------

    const nulljsfx::ModuleDesc* const* modules() const { return fModules; }
    uint32_t numModules() const { return fNumModules; }

    const nulljsfx::ModuleDesc* find(const char* id) const
    {
        if (!id || !*id)
            return nullptr;
        for (uint32_t i = 0; i < fNumModules; ++i)
            if (std::strcmp(fModules[i]->id, id) == 0)
                return fModules[i];
        return nullptr;
    }

    // ---- parameters (any thread) -------------------------------------------------------------

    float param(uint32_t i) const { return i < kNumParams ? fParams[i].load(std::memory_order_relaxed) : 0.0f; }
    void setParam(uint32_t i, float v)
    {
        if (i < kNumParams)
            fParams[i].store(v, std::memory_order_relaxed);
    }

    // ---- setup (never on the audio thread) ---------------------------------------------------

    void setSampleRate(double sr)
    {
        fLock.lock();
        fSampleRate = sr;
        for (Slot& s : fSlot)
        {
            if (s.mod)
                s.mod->setSampleRate(sr);
            s.split[0].reset();
            s.split[1].reset();
            s.dry.clear();
            s.pass.clear();
        }
        fRackDry.clear();
        fXLow = fXHigh = -1.0f;
        fEnv = 0.0;
        fLfoPhase[0] = fLfoPhase[1] = 0.0;
        fLfoCycle[0] = fLfoCycle[1] = -1;
        fSeqPos = 0.0;
        fAlignA.clear();
        fAlignB.clear();
        fKey.clear();
        fFbRing.clear();
        fLanesActive = fFbActive = false;
        // the feedback loop's filters: DC blocker (10 Hz), low cut (40 Hz), high cut (12 kHz)
        fFbDc = std::exp(-2.0 * M_PI * 10.0 / sr);
        fFbLc = std::exp(-2.0 * M_PI * 40.0 / sr);
        fFbHc = std::exp(-2.0 * M_PI * std::min(12000.0, 0.45 * sr) / sr);
        fLock.unlock();
    }
    double sampleRate() const { return fSampleRate; }

    const nulljsfx::ModuleDesc* slotModule(uint32_t slot) const { return slot < kSlots ? fSlot[slot].desc : nullptr; }
    const char* slotId(uint32_t slot) const
    {
        const nulljsfx::ModuleDesc* d = slotModule(slot);
        return d ? d->id : "";
    }

    // Sets all 8 slots (ids; unknown or empty = no module). A slot that keeps its module keeps
    // it; a module that moved to another slot moves with its instance and its slot parameters
    // (a reorder); every other slot gets a new module with its values reset to the defaults.
    // Returns false if an id is unknown (that slot is left empty).
    bool setSlots(const std::string ids[kSlots])
    {
        bool ok = true;
        const nulljsfx::ModuleDesc* want[kSlots];
        for (uint32_t i = 0; i < kSlots; ++i)
        {
            want[i] = find(ids[i].c_str());
            if (!want[i] && !ids[i].empty())
                ok = false;
        }
        // which old slot each new slot takes its instance from (-1 = a new one)
        int from[kSlots];
        bool used[kSlots] = {};
        for (uint32_t i = 0; i < kSlots; ++i)
        {
            from[i] = -1;
            if (want[i] && fSlot[i].desc == want[i])
            {
                from[i] = int(i);
                used[i] = true;
            }
        }
        for (uint32_t i = 0; i < kSlots; ++i)
            if (want[i] && from[i] < 0)
                for (uint32_t j = 0; j < kSlots; ++j)
                    if (!used[j] && fSlot[j].desc == want[i] && !(want[j] == fSlot[j].desc))
                    {
                        from[i] = int(j);
                        used[j] = true;
                        break;
                    }
        // create the new modules here, off the audio thread
        std::unique_ptr<nulljsfx::Module> fresh[kSlots];
        for (uint32_t i = 0; i < kSlots; ++i)
            if (want[i] && from[i] < 0)
            {
                fresh[i].reset(want[i]->create());
                fresh[i]->setSampleRate(fSampleRate);
                for (uint32_t v = 0; v < want[i]->numSliders && v < kValues; ++v)
                    fresh[i]->setValue(v, want[i]->sliders[v].def);
            }

        std::unique_ptr<nulljsfx::Module> old[kSlots];
        fLock.lock();
        Moved moved[kSlots];
        for (uint32_t j = 0; j < kSlots; ++j)
        {
            moved[j].mod = std::move(fSlot[j].mod);
            moved[j].desc = fSlot[j].desc;
            std::memcpy(moved[j].applied, fSlot[j].applied, sizeof(moved[j].applied));
            for (uint32_t p = 0; p < kSlotParams; ++p)
                moved[j].params[p] = param(slotParam(j, p));
            moved[j].lane = param(laneParam(j));
        }
        for (uint32_t i = 0; i < kSlots; ++i)
        {
            Slot& s = fSlot[i];
            if (from[i] >= 0)
            {
                Moved& m = moved[from[i]];
                s.mod = std::move(m.mod);
                s.desc = m.desc;
                std::memcpy(s.applied, m.applied, sizeof(s.applied));
                if (from[i] != int(i))
                {
                    for (uint32_t p = 0; p < kSlotParams; ++p)
                        setParam(slotParam(i, p), m.params[p]);
                    setParam(laneParam(i), m.lane);
                    resetSlotAudio(s);
                }
            }
            else
            {
                s.mod = std::move(fresh[i]);
                s.desc = want[i];
                resetSlotAudio(s);
                for (uint32_t v = 0; v < kValues; ++v)
                {
                    float def = 0.0f;
                    if (s.desc && v < s.desc->numSliders)
                    {
                        def = plainToValue(s.desc->sliders[v], isLogSlider(s.desc->id, v), s.desc->sliders[v].def);
                        s.applied[v] = s.desc->sliders[v].def;
                    }
                    setParam(valueParam(i, v), def);
                }
            }
            for (uint32_t v = 0; v < kValues; ++v)
                s.log[v] = s.desc && v < s.desc->numSliders && isLogSlider(s.desc->id, v);
            s.keyed = dynamic_cast<KeyedModule*>(s.mod.get());
        }
        for (uint32_t j = 0; j < kSlots; ++j)
            old[j] = std::move(moved[j].mod); // the instances nobody took
        fLock.unlock();
        // old[] is deleted here, outside the lock
        fVersion.fetch_add(1, std::memory_order_release);
        return ok;
    }

    bool setSlot(uint32_t slot, const char* id)
    {
        std::string ids[kSlots];
        for (uint32_t i = 0; i < kSlots; ++i)
            ids[i] = slotId(i);
        if (slot < kSlots)
            ids[slot] = id ? id : "";
        return setSlots(ids);
    }

    void setRoutes(const Route* r, uint32_t n)
    {
        fLock.lock();
        fNumRoutes = 0;
        for (uint32_t i = 0; i < n && fNumRoutes < kMaxRoutes; ++i)
            if (r[i].source >= 0 && r[i].source < kNumSources && r[i].slot >= 0 && r[i].slot < int(kSlots) &&
                r[i].value >= 0 && r[i].value < int(kValues))
                fRoutes[fNumRoutes++] = r[i];
        fLock.unlock();
        fVersion.fetch_add(1, std::memory_order_release);
    }

    uint32_t routes(Route* out) const
    {
        const_cast<SpinLock&>(fLock).lock();
        const uint32_t n = fNumRoutes;
        for (uint32_t i = 0; i < n; ++i)
            out[i] = fRoutes[i];
        const_cast<SpinLock&>(fLock).unlock();
        return n;
    }

    // ---- presets -----------------------------------------------------------------------------

    // What a preset sets: the slots' module ids, every parameter (the defaults, the modules'
    // default values, then the preset's own) and the routes
    struct PresetState {
        std::string ids[kSlots];
        float params[kNumParams];
        Route routes[kMaxRoutes];
        uint32_t numRoutes = 0;
    };

    // Resolves a preset against this build's modules. Returns false (and the reasons in err) if
    // it names a module, slider, source or global this build does not have; the rest still
    // resolves.
    bool resolvePreset(const Preset& p, PresetState& out, std::string* err = nullptr) const
    {
        bool ok = true;
        auto fail = [&](const std::string& why) {
            ok = false;
            if (err)
                *err += (err->empty() ? "" : "; ") + why;
        };
        for (uint32_t i = 0; i < kNumParams; ++i)
        {
            char n[32], s[32];
            out.params[i] = paramInfo(i, n, sizeof(n), s, sizeof(s)).def;
        }
        for (uint32_t i = 0; i < kSlots; ++i)
        {
            out.ids[i].clear();
            for (uint32_t v = 0; v < kValues; ++v)
                out.params[valueParam(i, v)] = 0.0f;
        }
        for (uint32_t i = 0; i < p.numSlots && i < kSlots; ++i)
        {
            const PresetSlot& ps = p.slots[i];
            const nulljsfx::ModuleDesc* d = find(ps.id);
            if (!d)
            {
                fail(std::string("no module ") + ps.id);
                continue;
            }
            out.ids[i] = ps.id;
            out.params[slotParam(i, kSlotOn)] = ps.on ? 1.0f : 0.0f;
            out.params[slotParam(i, kSlotMix)] = ps.mix;
            out.params[slotParam(i, kSlotBand)] = float(ps.band);
            out.params[slotParam(i, kSlotChannel)] = float(ps.channel);
            out.params[laneParam(i)] = ps.lane ? float(kLaneB) : float(kLaneA);
            for (uint32_t v = 0; v < d->numSliders && v < kValues; ++v)
                out.params[valueParam(i, v)] = plainToValue(d->sliders[v], isLogSlider(d->id, v), d->sliders[v].def);
            for (uint32_t k = 0; k < ps.numValues; ++k)
            {
                const PresetValue& pv = ps.values[k];
                if (pv.index >= d->numSliders || pv.index >= kValues)
                {
                    fail(std::string(d->id) + " has no slider " + std::to_string(pv.index + 1));
                    continue;
                }
                const DISTRHO::SliderInfo& in = d->sliders[pv.index];
                if (pv.value < in.min || pv.value > in.max)
                    fail(std::string(d->id) + " " + pv.name + " out of range");
                out.params[valueParam(i, pv.index)] = plainToValue(in, isLogSlider(d->id, pv.index), pv.value);
            }
        }
        for (uint32_t k = 0; k < p.numGlobals; ++k)
        {
            const PresetGlobal& g = p.globals[k];
            const int gi = globalFromSymbol(g.symbol);
            if (gi < 0)
            {
                fail(std::string("no parameter ") + g.symbol);
                continue;
            }
            float v = g.value;
            char gn[32], gs[32];
            const DISTRHO::SliderInfo in = paramInfo(uint32_t(gi), gn, sizeof(gn), gs, sizeof(gs));
            if (g.option)
            {
                int found = -1;
                const int n = in.options ? int(in.max - in.min + 0.5f) + 1 : 0;
                for (int o = 0; o < n; ++o)
                    if (std::strcmp(in.options[o], g.option) == 0)
                        found = o;
                if (found < 0)
                {
                    fail(std::string(g.symbol) + " has no option " + g.option);
                    continue;
                }
                v = in.min + float(found);
            }
            else if (v < in.min || v > in.max)
                fail(std::string(g.symbol) + " out of range");
            out.params[gi] = v;
        }
        out.numRoutes = 0;
        for (uint32_t k = 0; k < p.numRoutes && out.numRoutes < kMaxRoutes; ++k)
        {
            const PresetRoute& pr = p.routes[k];
            const int src = sourceFromSymbol(pr.source);
            if (src < 0 || pr.slot >= kSlots || pr.value >= kValues)
            {
                fail(std::string("bad route from ") + pr.source);
                continue;
            }
            Route& r = out.routes[out.numRoutes++];
            r.source = src;
            r.slot = pr.slot;
            r.value = pr.value;
            r.depth = pr.depth;
        }
        return ok;
    }

    // Loads a preset: fresh modules in every slot, every parameter and the routes.
    bool loadPreset(const Preset& p, std::string* err = nullptr)
    {
        std::unique_ptr<PresetState> st(new PresetState());
        const bool ok = resolvePreset(p, *st, err);
        std::string none[kSlots];
        setSlots(none);
        setSlots(st->ids);
        for (uint32_t i = 0; i < kNumParams; ++i)
            setParam(i, st->params[i]);
        setRoutes(st->routes, st->numRoutes);
        fPresetName = p.name;
        return ok;
    }

    std::string presetName() const { return fPresetName; }
    void setPresetName(const std::string& n) { fPresetName = n; }

    // ---- state for the interface (read from any thread; torn reads are harmless) -------------

    uint32_t version() const { return fVersion.load(std::memory_order_acquire); }
    uint32_t latency() const { return fLatency.load(std::memory_order_relaxed); }
    uint32_t slotLatency(uint32_t s) const { return s < kSlots ? fSlot[s].latency.load(std::memory_order_relaxed) : 0; }
    // the effective (modulated) normalized value of a slot value
    float effective(uint32_t slot, uint32_t v) const
    {
        return slot < kSlots && v < kValues ? fSlot[slot].eff[v].load(std::memory_order_relaxed) : 0.0f;
    }
    // the current value of a modulation source (macros, env 0..1; LFOs -1..1)
    float sourceValue(int src) const
    {
        return src >= 0 && src < kNumSources ? fSourceValue[src].load(std::memory_order_relaxed) : 0.0f;
    }
    float lfoPhase(int i) const { return fLfoPhaseOut[i & 1].load(std::memory_order_relaxed); }
    // the step the sequencer is on (0..15)
    int seqStep() const { return fSeqStepOut.load(std::memory_order_relaxed); }
    float inputPeak() const { return fInPeak.load(std::memory_order_relaxed); }
    float outputPeak() const { return fOutPeak.load(std::memory_order_relaxed); }

    // the plain value a slot value parameter (0..1) means for the slot's module
    float plainValue(uint32_t slot, uint32_t v, float norm) const
    {
        const nulljsfx::ModuleDesc* d = slotModule(slot);
        if (!d || v >= d->numSliders)
            return 0.0f;
        return valueToPlain(d->sliders[v], isLogSlider(d->id, v), norm);
    }

    // ---- audio -------------------------------------------------------------------------------

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames, const Time& time)
    {
        if (!fLock.tryLock())
        {
            // a module is being swapped: the input, delayed by the last latency
            const uint32_t d = std::min(fLatency.load(std::memory_order_relaxed), fRackDry.maxDelay());
            for (uint32_t i = 0; i < frames; ++i)
            {
                float l, r;
                fRackDry.process(inL[i], inR[i], d, l, r);
                outL[i] = l;
                outR[i] = r;
            }
            return;
        }
        Time t = time;
        for (uint32_t done = 0; done < frames;)
        {
            const uint32_t n = std::min(kChunk, frames - done);
            processChunk(inL + done, inR + done, outL + done, outR + done, n, t);
            done += n;
            const double tempo = t.tempo > 0.0 ? t.tempo : 120.0;
            if (t.playing)
                t.beat += double(n) / fSampleRate * tempo / 60.0;
        }
        fLock.unlock();
    }

private:
    struct Slot {
        std::unique_ptr<nulljsfx::Module> mod;
        const nulljsfx::ModuleDesc* desc = nullptr;
        float applied[kValues] = {};
        bool log[kValues] = {};
        DelayLine dry, pass;
        Split3 split[2];
        std::atomic<float> eff[kValues];
        std::atomic<uint32_t> latency { 0 };
        KeyedModule* keyed = nullptr; // mod, when it takes a key
    };

    struct Moved {
        std::unique_ptr<nulljsfx::Module> mod;
        const nulljsfx::ModuleDesc* desc = nullptr;
        float applied[kValues] = {};
        float params[kSlotParams] = {};
        float lane = 0.0f;
    };

    void resetSlotAudio(Slot& s)
    {
        s.dry.clear();
        s.pass.clear();
        s.split[0].reset();
        s.split[1].reset();
        s.latency.store(0, std::memory_order_relaxed);
    }

    // ---- modulation sources ------------------------------------------------------------------

    double lfoShape(int shape, double ph, int lfo) const
    {
        switch (shape)
        {
        case 0: return std::sin(2.0 * M_PI * ph);
        case 1: return ph < 0.25 ? 4.0 * ph : (ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0);
        case 2: return 2.0 * ph - 1.0;
        case 3: return 1.0 - 2.0 * ph;
        case 4: return ph < 0.5 ? 1.0 : -1.0;
        default: return fLfoHeld[lfo];
        }
    }

    // exact Park-Miller minimal standard step (16807 * seed mod 2^31 - 1), in doubles
    static double parkMiller(double& seed)
    {
        seed = std::fmod(16807.0 * seed, 2147483647.0);
        return seed / 2147483647.0;
    }

    void updateSources(const float* l, const float* r, uint32_t n, const Time& t)
    {
        for (int m = 0; m < 8; ++m)
            fSrc[kSrcMacro1 + m] = clamp01(param(kMacro1 + m) / 100.0f);

        const double tempo = t.tempo > 0.0 ? t.tempo : 120.0;
        for (int k = 0; k < 2; ++k)
        {
            const uint32_t base = k ? kLfo2Rate : kLfo1Rate;
            const int sync = int(std::lround(param(base + 1)));
            const int shape = int(std::lround(param(base + 2)));
            const double beats = sync > 0 && sync < 16 ? kSyncBeats[sync] : 0.0;
            double ph;
            long long cycle;
            if (beats > 0.0 && t.playing && t.valid)
            {
                const double c = t.beat / beats;
                cycle = (long long)std::floor(c);
                ph = c - std::floor(c);
                fLfoPhase[k] = ph;
            }
            else
            {
                ph = fLfoPhase[k];
                cycle = fLfoCycle[k] < 0 ? 0 : fLfoCycle[k];
            }
            if (cycle != fLfoCycle[k])
            {
                fLfoHeld[k] = parkMiller(fSeed[k]) * 2.0 - 1.0;
                fLfoCycle[k] = cycle;
            }
            fSrc[k ? kSrcLfo2 : kSrcLfo1] = float(lfoShape(shape, ph, k));
            fLfoPhaseOut[k].store(float(ph), std::memory_order_relaxed);
            if (!(beats > 0.0 && t.playing && t.valid))
            {
                // free running: Hz, or the synced note at the host tempo while stopped
                const double hz = beats > 0.0 ? tempo / 60.0 / beats : double(param(base));
                double next = fLfoPhase[k] + hz * double(n) / fSampleRate;
                if (next >= 1.0)
                {
                    next -= std::floor(next);
                    fLfoCycle[k] = fLfoCycle[k] + 1; // a new S&H value for the new cycle
                    fLfoHeld[k] = parkMiller(fSeed[k]) * 2.0 - 1.0;
                }
                fLfoPhase[k] = next;
            }
        }

        // envelope follower of the rack input (after the input gain)
        const double att = std::exp(-1.0 / (std::max(0.1f, param(kEnvAttack)) * 0.001 * fSampleRate));
        const double rel = std::exp(-1.0 / (std::max(1.0f, param(kEnvRelease)) * 0.001 * fSampleRate));
        double e = fEnv;
        for (uint32_t i = 0; i < n; ++i)
        {
            const double x = std::max(std::fabs(double(l[i])), std::fabs(double(r[i])));
            e = x > e ? att * e + (1.0 - att) * x : rel * e + (1.0 - rel) * x;
        }
        fEnv = e < 1e-30 ? 0.0 : e;
        const double g = std::pow(10.0, double(param(kEnvGain)) / 20.0);
        fSrc[kSrcEnv] = float(std::min(1.0, fEnv * g));

        // step sequencer: the position in steps, from the host's beat when synced and playing,
        // else running at Rate (or the synced note at the host tempo while stopped)
        {
            const int steps = std::max(1, std::min(int(kSeqSteps), int(std::lround(param(kSeqLength)))));
            const int sync = int(std::lround(param(kSeqSync)));
            const double beats = sync > 0 && sync < 16 ? kSyncBeats[sync] : 0.0; // per step
            const bool locked = beats > 0.0 && t.playing && t.valid;
            double pos = locked ? t.beat / beats : fSeqPos;
            pos = std::fmod(pos, double(steps));
            if (pos < 0.0)
                pos += double(steps);
            int k = int(std::floor(pos));
            if (k >= steps)
                k = steps - 1;
            const double frac = pos - double(k);
            const double cur = clamp01(param(kSeqStep1 + uint32_t(k)));
            const double prev = clamp01(param(kSeqStep1 + uint32_t((k + steps - 1) % steps)));
            const double glide = std::min(1.0, std::max(0.0, double(param(kSeqGlide)) / 100.0));
            fSrc[kSrcSeq] = float(glide > 0.0 && frac < glide ? prev + (cur - prev) * frac / glide : cur);
            fSeqStepOut.store(k, std::memory_order_relaxed);
            if (locked)
                fSeqPos = pos;
            else
            {
                const double rate = beats > 0.0 ? tempo / 60.0 / beats : double(param(kSeqRate)); // steps/s
                fSeqPos = std::fmod(pos + rate * double(n) / fSampleRate, double(steps));
            }
        }

        for (int s = 0; s < kNumSources; ++s)
            fSourceValue[s].store(fSrc[s], std::memory_order_relaxed);
    }

    // effective values -> Module::setValue where they changed
    void applyValues()
    {
        float mod[kSlots][kValues];
        bool any[kSlots] = {};
        std::memset(mod, 0, sizeof(mod));
        for (uint32_t i = 0; i < fNumRoutes; ++i)
        {
            const Route& r = fRoutes[i];
            mod[r.slot][r.value] += r.depth * fSrc[r.source];
            any[r.slot] = true;
        }
        for (uint32_t s = 0; s < kSlots; ++s)
        {
            Slot& sl = fSlot[s];
            if (!sl.desc)
                continue;
            const uint32_t nv = std::min<uint32_t>(sl.desc->numSliders, kValues);
            for (uint32_t v = 0; v < nv; ++v)
            {
                const float norm = clamp01(param(valueParam(s, v)) + (any[s] ? mod[s][v] : 0.0f));
                sl.eff[v].store(norm, std::memory_order_relaxed);
                const float plain = valueToPlain(sl.desc->sliders[v], sl.log[v], norm);
                if (plain != sl.applied[v])
                {
                    sl.applied[v] = plain;
                    sl.mod->setValue(v, plain);
                }
            }
        }
    }

    // ---- one chunk ---------------------------------------------------------------------------

    void processChunk(const float* inL, const float* inR, float* outL, float* outR, uint32_t n, const Time& t)
    {
        float* xl = fBufL;
        float* xr = fBufR;
        const float inGain = param(kInput) == 0.0f ? 1.0f : float(std::pow(10.0, double(param(kInput)) / 20.0));
        float pin = 0.0f;
        for (uint32_t i = 0; i < n; ++i)
        {
            xl[i] = inL[i] * inGain;
            xr[i] = inR[i] * inGain;
            pin = std::max(pin, std::max(std::fabs(xl[i]), std::fabs(xr[i])));
        }

        updateSources(xl, xr, n, t);
        applyValues();
        updateCrossovers();

        nulljsfx::ModuleTime mt;
        mt.tempo = t.tempo;
        mt.beat = t.beat;
        mt.playing = t.playing;
        mt.valid = t.valid;

        // the key of keyed modules: the rack input after the input gain
        fKey.write(xl, xr, n);

        bool laneB = false;
        for (uint32_t s = 0; s < kSlots; ++s)
            laneB = laneB || (fSlot[s].desc && param(laneParam(s)) >= 0.5f);
        const float fb = std::min(std::max(param(kFeedback), 0.0f), kMaxFeedback) / 100.0f;

        uint32_t total = 0;
        if (!laneB && !(fb > 0.0f))
        {
            // one serial chain, exactly as in 2.1
            fLanesActive = fFbActive = false;
            for (uint32_t s = 0; s < kSlots; ++s)
                if (fSlot[s].desc)
                {
                    const uint32_t lat = processSlot(s, xl, xr, n, mt, total);
                    total += lat;
                }
        }
        else
            total = processLanes(xl, xr, n, mt, laneB, fb);
        fKey.advance(n);

        // the rack's mix against its input delayed by the whole chain's latency, then the output gain
        const uint32_t d = std::min(total, fRackDry.maxDelay());
        const float mix = clamp01(param(kMix) / 100.0f);
        const float outGain = param(kOutput) == 0.0f ? 1.0f : float(std::pow(10.0, double(param(kOutput)) / 20.0));
        float pout = 0.0f;
        for (uint32_t i = 0; i < n; ++i)
        {
            float dl, dr;
            fRackDry.process(inL[i], inR[i], d, dl, dr);
            float l = xl[i], r = xr[i];
            if (mix < 1.0f)
            {
                l = l * mix + dl * inGain * (1.0f - mix);
                r = r * mix + dr * inGain * (1.0f - mix);
            }
            outL[i] = l * outGain;
            outR[i] = r * outGain;
            pout = std::max(pout, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
        }
        fLatency.store(total, std::memory_order_relaxed);
        fInPeak.store(std::max(pin, fInPeak.load(std::memory_order_relaxed) * fPeakFall), std::memory_order_relaxed);
        fOutPeak.store(std::max(pout, fOutPeak.load(std::memory_order_relaxed) * fPeakFall), std::memory_order_relaxed);
    }

    void updateCrossovers()
    {
        float f1 = param(kXoverLow), f2 = param(kXoverHigh);
        const float ny = float(fSampleRate) * 0.45f;
        f1 = std::min(std::max(f1, 20.0f), ny * 0.5f);
        f2 = std::min(std::max(f2, f1 * 1.25f), ny);
        if (f1 == fXLow && f2 == fXHigh)
            return;
        fXLow = f1;
        fXHigh = f2;
        for (Slot& s : fSlot)
            for (Split3& sp : s.split)
                sp.set(f1, f2, fSampleRate);
    }

    // two lanes and/or the feedback loop (added after 2.1); x holds the input and gets the output. Returns
    // the latency (the longer lane's).
    uint32_t processLanes(float* xl, float* xr, uint32_t n, const nulljsfx::ModuleTime& mt, bool laneB, float fb)
    {
        float* yl = fLaneL;
        float* yr = fLaneR;
        if (laneB)
        {
            std::memcpy(yl, xl, n * sizeof(float));
            std::memcpy(yr, xr, n * sizeof(float));
        }

        // feedback: what left slot `to` kChunk samples ago, through the loop's filters and soft clip
        const bool fbOn = fb > 0.0f;
        int from = int(std::lround(param(kFbFrom))) - 1, to = int(std::lround(param(kFbTo))) - 1;
        from = std::max(0, std::min(int(kSlots) - 1, from));
        to = std::max(0, std::min(int(kSlots) - 1, to));
        if (from > to)
            std::swap(from, to);
        if (fbOn)
        {
            if (!fFbActive)
            {
                fFbRing.clear();
                std::memset(fFbState, 0, sizeof(fFbState));
                fFbActive = true;
            }
            fFbRing.read(kChunk, n, fFbL, fFbR);
            float* ch[2] = { fFbL, fFbR };
            for (int c = 0; c < 2; ++c)
            {
                double* st = fFbState[c]; // dc x1, dc y1, low cut x1, low cut y1, high cut y1
                for (uint32_t i = 0; i < n; ++i)
                {
                    const double x = ch[c][i];
                    const double dc = x - st[0] + fFbDc * st[1];
                    st[0] = x;
                    st[1] = dc;
                    const double lc = fFbLc * (st[3] + dc - st[2]);
                    st[2] = dc;
                    st[3] = lc;
                    st[4] = (1.0 - fFbHc) * lc + fFbHc * st[4];
                    ch[c][i] = float(std::tanh(double(fb) * st[4]));
                }
                for (int k = 0; k < 5; ++k)
                    if (std::fabs(st[k]) < 1e-30)
                        st[k] = 0.0;
            }
        }
        else
            fFbActive = false;

        uint32_t latA = 0, latB = 0;
        for (uint32_t s = 0; s < kSlots; ++s)
        {
            const bool b = laneB && param(laneParam(s)) >= 0.5f;
            float* bl = b ? yl : xl;
            float* br = b ? yr : xr;
            uint32_t& lat = b ? latB : latA;
            if (fbOn && int(s) == from)
                for (uint32_t i = 0; i < n; ++i)
                {
                    bl[i] += fFbL[i];
                    br[i] += fFbR[i];
                }
            if (fSlot[s].desc)
            {
                const uint32_t l = processSlot(s, bl, br, n, mt, lat);
                lat += l;
            }
            if (fbOn && int(s) == to)
                fFbRing.write(bl, br, n);
        }
        if (fbOn)
            fFbRing.advance(n);
        if (!laneB)
        {
            fLanesActive = false;
            return latA;
        }

        // line the lanes up (delay the shorter one) and blend them
        const uint32_t total = std::max(latA, latB);
        const uint32_t dA = std::min(total - latA, fAlignA.maxDelay()), dB = std::min(total - latB, fAlignB.maxDelay());
        if (!fLanesActive)
        {
            fAlignA.clearRecent(dA + 1);
            fAlignB.clearRecent(dB + 1);
            fLanesActive = true;
        }
        const float m = clamp01(param(kLaneMix) / 100.0f);
        for (uint32_t i = 0; i < n; ++i)
        {
            float al, ar, bl, br;
            fAlignA.process(xl[i], xr[i], dA, al, ar);
            fAlignB.process(yl[i], yr[i], dB, bl, br);
            xl[i] = al * (1.0f - m) + bl * m;
            xr[i] = ar * (1.0f - m) + br * m;
        }
        return total;
    }

    // runs slot s on x in place; returns its latency. keyDelay: the latency of the slots before
    // it in its lane (what a keyed module's key is delayed by)
    uint32_t processSlot(uint32_t s, float* xl, float* xr, uint32_t n, const nulljsfx::ModuleTime& mt, uint32_t keyDelay)
    {
        Slot& sl = fSlot[s];
        const bool on = param(slotParam(s, kSlotOn)) >= 0.5f;
        const float mix = clamp01(param(slotParam(s, kSlotMix)) / 100.0f);
        const int band = int(std::lround(param(slotParam(s, kSlotBand))));
        const int chan = int(std::lround(param(slotParam(s, kSlotChannel))));
        const uint32_t lat = std::min(sl.mod->latency(), sl.dry.maxDelay());
        sl.latency.store(lat, std::memory_order_relaxed);

        // the module's input c (stereo, or one component on both sides) and what passes (p)
        float* cl = fCL;
        float* cr = fCR;
        float* pl = fPL;
        float* pr = fPR;
        const bool split = band >= kBandLow && band <= kBandHigh;
        const bool hasPass = split || chan != kChanStereo;
        for (uint32_t i = 0; i < n; ++i)
        {
            float bl = xl[i], br = xr[i], rl = 0.0f, rr = 0.0f;
            if (split)
            {
                double lo, mi, hi, lo2, mi2, hi2;
                sl.split[0].process(xl[i], lo, mi, hi);
                sl.split[1].process(xr[i], lo2, mi2, hi2);
                const double b0 = band == kBandLow ? lo : band == kBandMid ? mi : hi;
                const double b1 = band == kBandLow ? lo2 : band == kBandMid ? mi2 : hi2;
                bl = float(b0);
                br = float(b1);
                rl = float(lo + mi + hi - b0);
                rr = float(lo2 + mi2 + hi2 - b1);
            }
            switch (chan)
            {
            case kChanMid:
            {
                const float m = (bl + br) * 0.5f, sd = (bl - br) * 0.5f;
                cl[i] = cr[i] = m;
                pl[i] = sd + rl;
                pr[i] = rr - sd;
                break;
            }
            case kChanSide:
            {
                const float m = (bl + br) * 0.5f, sd = (bl - br) * 0.5f;
                cl[i] = cr[i] = sd;
                pl[i] = m + rl;
                pr[i] = m + rr;
                break;
            }
            case kChanLeft:
                cl[i] = cr[i] = bl;
                pl[i] = rl;
                pr[i] = br + rr;
                break;
            case kChanRight:
                cl[i] = cr[i] = br;
                pl[i] = bl + rl;
                pr[i] = rr;
                break;
            default:
                cl[i] = bl;
                cr[i] = br;
                pl[i] = rl;
                pr[i] = rr;
                break;
            }
        }

        float* ol = fOL;
        float* orr = fOR;
        if (on)
        {
            if (sl.keyed)
            {
                fKey.read(std::min(keyDelay, fKey.maxDelay(kChunk)), n, fKL, fKR);
                sl.keyed->setKey(fKL, fKR);
            }
            sl.mod->process(cl, cr, ol, orr, n, mt);
        }

        for (uint32_t i = 0; i < n; ++i)
        {
            float dl, dr, wl, wr;
            sl.dry.process(cl[i], cr[i], lat, dl, dr);
            if (!on)
            {
                wl = dl;
                wr = dr;
            }
            else if (mix >= 1.0f)
            {
                wl = ol[i];
                wr = orr[i];
            }
            else
            {
                wl = ol[i] * mix + dl * (1.0f - mix);
                wr = orr[i] * mix + dr * (1.0f - mix);
            }
            if (!hasPass)
            {
                xl[i] = wl;
                xr[i] = wr;
                continue;
            }
            float ql, qr;
            sl.pass.process(pl[i], pr[i], lat, ql, qr);
            const float w = chan == kChanStereo ? 0.0f : (wl + wr) * 0.5f;
            switch (chan)
            {
            case kChanMid:   xl[i] = w + ql; xr[i] = w + qr; break;
            case kChanSide:  xl[i] = ql + w; xr[i] = qr - w; break;
            case kChanLeft:  xl[i] = w + ql; xr[i] = qr; break;
            case kChanRight: xl[i] = ql; xr[i] = w + qr; break;
            default:         xl[i] = wl + ql; xr[i] = wr + qr; break;
            }
        }
        return lat;
    }

    const nulljsfx::ModuleDesc* const* fModules;
    const uint32_t fNumModules;

    SpinLock fLock;
    double fSampleRate = 44100.0;
    std::atomic<float> fParams[kNumParams];
    Slot fSlot[kSlots];
    Route fRoutes[kMaxRoutes];
    uint32_t fNumRoutes = 0;
    std::string fPresetName = "Init";

    DelayLine fRackDry;
    float fXLow = -1.0f, fXHigh = -1.0f;

    float fSrc[kNumSources] = {};
    double fEnv = 0.0;
    double fLfoPhase[2] = { 0.0, 0.0 };
    long long fLfoCycle[2] = { -1, -1 };
    double fLfoHeld[2] = { 0.0, 0.0 };
    double fSeed[2] = { 1.0, 7.0 };
    const float fPeakFall = 0.93f;

    std::atomic<float> fSourceValue[kNumSources];
    std::atomic<float> fLfoPhaseOut[2] { { 0.0f }, { 0.0f } };
    std::atomic<int> fSeqStepOut { 0 };
    double fSeqPos = 0.0;

    // added after 2.1: lanes, feedback, keys
    DelayLine fAlignA, fAlignB;
    ChunkRing fKey, fFbRing;
    bool fLanesActive = false, fFbActive = false;
    double fFbDc = 0.0, fFbLc = 0.0, fFbHc = 0.0;
    double fFbState[2][5] = {};
    float fLaneL[kChunk], fLaneR[kChunk], fFbL[kChunk], fFbR[kChunk], fKL[kChunk], fKR[kChunk];
    std::atomic<float> fInPeak { 0.0f }, fOutPeak { 0.0f };
    std::atomic<uint32_t> fLatency { 0 };
    std::atomic<uint32_t> fVersion { 0 };

    float fBufL[kChunk], fBufR[kChunk];
    float fCL[kChunk], fCR[kChunk], fPL[kChunk], fPR[kChunk], fOL[kChunk], fOR[kChunk];
};

} // namespace rack
