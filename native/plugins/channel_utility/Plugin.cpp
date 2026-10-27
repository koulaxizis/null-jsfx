/*
 * NULL JSFX - Channel Utility (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ChannelUtilityDSP.hpp"
#include "ChannelUtilitySliders.hpp"

START_NAMESPACE_DISTRHO

class PluginChannelUtility : public NullMultiPlugin
{
public:
    PluginChannelUtility()
        : NullMultiPlugin(nullchannelutility::kSliders, nullchannelutility::kNumSliders)
    {
        setup(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "ChannelUtility"; }
    const char* getDescription() const override
    {
        return "Channel strip tools: pad and gain, polarity, DC block, balance, solo, mono check, output gain and hard limit.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '3', 'c', 'u'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { setup(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        // display: peak followers of the input (read before the outputs are written) and output,
        // in scopeL[0..3] (IN L, IN R, OUT L, OUT R) as in channel_utility_gfx.jsfx
        for (uint32_t i = 0; i < frames; ++i)
        {
            fPeak[0].process(inputs[0][i]);
            fPeak[1].process(inputs[1][i]);
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        for (uint32_t i = 0; i < frames; ++i)
        {
            fPeak[2].process(outputs[0][i]);
            fPeak[3].process(outputs[1][i]);
        }
        for (int k = 0; k < 4; ++k)
            taps.scopeL[k] = fPeak[k].value;
    }

private:
    void setup(double sr)
    {
        for (auto& p : fPeak)
            p.setup(sr);
        update();
        fDsp.setSampleRate(sr);
    }

    void update()
    {
        using nullchannelutility::tenth;
        nullchannelutility::Params p;
        p.pad = int(std::lround(value(0)));
        p.inGainDb = tenth(value(1));
        p.invL = value(2) >= 0.5f;
        p.invR = value(3) >= 0.5f;
        p.dcFreq = std::round(value(4));
        p.dcMixPct = std::round(value(5));
        p.balLDb = tenth(value(6));
        p.balRDb = tenth(value(7));
        p.soloL = value(8) >= 0.5f;
        p.soloR = value(9) >= 0.5f;
        p.mono = value(10) >= 0.5f;
        p.outGainDb = tenth(value(11));
        p.limit = value(12) >= 0.5f;
        fDsp.setParams(p);
    }

    nullchannelutility::ChannelUtilityProcessor fDsp;
    nulljsfx::PeakFollower fPeak[4];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChannelUtility)
};

Plugin* createPlugin()
{
    return new PluginChannelUtility();
}

END_NAMESPACE_DISTRHO
