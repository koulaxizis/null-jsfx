/*
 * NULL JSFX - ChannelUtility as a Rack module: drives nullchannelutility::ChannelUtilityProcessor as plugins/channel_utility/Plugin.cpp does
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullModule.hpp"
#include "../../channel_utility/ChannelUtilityDSP.hpp"
#include "../../channel_utility/ChannelUtilitySliders.hpp"

#include <cmath>

namespace nullrack {
namespace channel_utility {

class ChannelUtilityModule final : public nulljsfx::Module {
public:
    ChannelUtilityModule()
    {
        for (uint32_t i = 0; i < nullchannelutility::kNumSliders; ++i)
            fV[i] = nullchannelutility::kSliders[i].def;
    }

    void setSampleRate(double sr) override
    {
        // as Plugin.cpp's setup(): @slider, then @init
        update();
        fDsp.setSampleRate(sr);
    }

    void setValue(uint32_t index, float v) override
    {
        if (index >= nullchannelutility::kNumSliders)
            return;
        fV[index] = v;
        update();
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames,
                 const nulljsfx::ModuleTime&) override
    {
        fDsp.process(inL, inR, outL, outR, frames);
    }

private:
    void update()
    {
        using nullchannelutility::tenth;
        nullchannelutility::Params p;
        p.pad = int(std::lround(fV[0]));
        p.inGainDb = tenth(fV[1]);
        p.invL = fV[2] >= 0.5f;
        p.invR = fV[3] >= 0.5f;
        p.dcFreq = std::round(fV[4]);
        p.dcMixPct = std::round(fV[5]);
        p.balLDb = tenth(fV[6]);
        p.balRDb = tenth(fV[7]);
        p.soloL = fV[8] >= 0.5f;
        p.soloR = fV[9] >= 0.5f;
        p.mono = fV[10] >= 0.5f;
        p.outGainDb = tenth(fV[11]);
        p.limit = fV[12] >= 0.5f;
        fDsp.setParams(p);
    }

    nullchannelutility::ChannelUtilityProcessor fDsp;
    float fV[nullchannelutility::kNumSliders];
};

inline nulljsfx::Module* create() { return new ChannelUtilityModule(); }

static const nulljsfx::ModuleDesc kDesc = { "channel_utility", "Channel Utility", "Essentials", nullchannelutility::kSliders,
                                            nullchannelutility::kNumSliders, create };

} // namespace channel_utility
} // namespace nullrack
