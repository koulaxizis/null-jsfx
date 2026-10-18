/*
 * NULL JSFX - Phase Align as a Rack module: drives nullphase::PhaseAlignProcessor as
 * plugins/phase_align/Plugin.cpp does. The Rack has no sidechain bus: with Source = Sidechain the
 * reference is the rack's input, the dry signal lined up with the module (RackKeyed.hpp), so the
 * module moves what the slots before it made against the dry signal (heard with Listen Mono Sum);
 * without a key from the Rack (the module test) the module's own input is the reference, as the
 * test feeds the JSFX. Source = Right to Left works as in the plugin. Latency as the plugin's.
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../RackKeyed.hpp"
#include "../../phase_align/PhaseAlignDSP.hpp"
#include "../../phase_align/PhaseAlignSliders.hpp"

namespace nullrack {
namespace phase_align {

class PhaseAlignModule final : public nulljsfx::Module, public rack::KeyedModule {
public:
    PhaseAlignModule()
    {
        for (uint32_t i = 0; i < nullphase::kNumSliders; ++i)
            fV[i] = nullphase::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullphase::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void setKey(const float* keyL, const float* keyR) override
    {
        fKeyL = keyL;
        fKeyR = keyR;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        const float* kl = fKeyL ? fKeyL : inL;
        const float* kr = fKeyR ? fKeyR : inR;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            fDsp.processSample(l, r, kl[i], kr[i]);
            outL[i] = float(l);
            outR[i] = float(r);
        }
        fKeyL = fKeyR = nullptr;
    }

    uint32_t latency() const override { return uint32_t(fDsp.latency()); } // pdc_delay

private:
    double v(uint32_t i) const { return nullphase::sliderValue(i, fV[i]); }
    void update()
    {
        nullphase::Params p;
        p.sidechain = v(0) >= 0.5;
        p.delayMs = v(1);
        p.fine = v(2);
        p.invL = v(3) >= 0.5;
        p.invR = v(4) >= 0.5;
        p.rotation = v(5);
        p.focus = v(6);
        p.sum = v(7) >= 0.5;
        fDsp.setParams(p);
    }

    nullphase::PhaseAlignProcessor fDsp;
    float fV[nullphase::kNumSliders];
    const float* fKeyL = nullptr;
    const float* fKeyR = nullptr;
};

inline nulljsfx::Module* create() { return new PhaseAlignModule(); }

static const nulljsfx::ModuleDesc kDesc = { "phase_align", "Phase Align", "Essentials", nullphase::kSliders,
                                            nullphase::kNumSliders, create };

} // namespace phase_align
} // namespace nullrack
