/*
 * NULL JSFX - NoiseReduction as a Rack module: drives nullnr::NoiseReducer as plugins/noise_reduction/Plugin.cpp does
 *
 * 4.2: no Noise File and no new value (the Rack's indices stay fixed): Use Noise File is always
 * Off here. The learnt profile is the module's data (saveData / loadData, the plugin's
 * "noiseprofile" string), saved in the Rack's "noiseprofiles" state; a loaded profile comes back
 * as the plugin's does with Use Noise File Off.
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../noise_reduction/NoiseReductionDSP.hpp"
#include "../../noise_reduction/NoiseReductionSliders.hpp"
#include "../../noise_reduction/NoiseFile.hpp"
#include "NullMailbox.hpp"

#include <cmath>

namespace nullrack {
namespace noise_reduction {

class NoiseReductionModule final : public nulljsfx::Module {
public:
    NoiseReductionModule()
    {
        for (uint32_t i = 0; i < nullnr::kNumModuleSliders; ++i)
            fV[i] = nullnr::kSliders[i].def;
        fDsp.setSnapshot(&fSnap);
    }

    std::string saveData() const override { return nullnr::encodeProfile(fSnap.read()); }

    void loadData(const char* data) override
    {
        nullnr::SavedProfile s;
        if (!nullnr::decodeProfile(data, s))
            return;
        s.gate = false; // no Use Noise File in the Rack: Off, which keeps the profile
        fMsg.saved = s;
        ++fMsg.savedSeq;
        fMail.post(new nullnr::Profiles(fMsg));
    }

    void setSampleRate(double sr) override
    {
        // like the JSFX's @init: the learnt profile survives unless the sample rate changes
        fDsp.setSampleRate(sr);
        update();
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullnr::kNumModuleSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        if (nullnr::Profiles* m = fMail.take())
        {
            fDsp.takeProfiles(*m);
            fMail.giveBack(m);
        }
        fDsp.process(inL, inR, outL, outR, frames);
    }

    // as Plugin.cpp's setLatency: pdc_delay = N - 1
    uint32_t latency() const override { return uint32_t(nullnr::NoiseReducer::latency()); }

private:
    void update()
    {
        // Threshold as the JSFX slider holds it (min + k * 0.5 in double)
        const double thr = -10.0 + std::round((double(fV[3]) + 10.0) / 0.5) * 0.5;
        fDsp.setParams(fV[0] >= 0.5f, fV[1] >= 0.5f, std::round(fV[2]), thr, std::round(fV[4]), std::round(fV[5]),
                       std::round(fV[6]), fV[7] >= 0.5f, std::round(fV[8]));
    }

    nullnr::NoiseReducer fDsp;
    nullnr::ProfileSnapshot fSnap;
    nulljsfx::Mailbox<nullnr::Profiles> fMail;
    nullnr::Profiles fMsg; // what was last posted (loadData, off the audio thread)
    float fV[nullnr::kNumModuleSliders];
};

inline nulljsfx::Module* create() { return new NoiseReductionModule(); }

static const nulljsfx::ModuleDesc kDesc = { "noise_reduction", "Noise Reduction", "Advanced", nullnr::kSliders,
                                            nullnr::kNumModuleSliders, create };

} // namespace noise_reduction
} // namespace nullrack
