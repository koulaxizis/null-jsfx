/*
 * NULL JSFX - Spread (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SpreadDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginSpread : public NullOneSliderPlugin
{
public:
    PluginSpread()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
        fMid.setup(getSampleRate());
        fSide.setup(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Spread"; }
    const char* getDescription() const override
    {
        return "Mono-compatible stereo widener: a delayed, high-passed copy of the mid is added to the left and subtracted from the right.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'p', 'r'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        fMid.setup(getSampleRate());
        fSide.setup(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        fMid.setup(newSampleRate);
        fSide.setup(newSampleRate);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // display: vectorscope taps and mid/side peak followers of the output (as in spread_gfx.jsfx)
        for (uint32_t i = 0; i < frames; ++i)
        {
            const float l = outputs[0][i], r = outputs[1][i];
            taps.push(l, r);
            fMid.process((l + r) * 0.5f);
            fSide.process((l - r) * 0.5f);
        }
        taps.inLevel.store(fMid.value, std::memory_order_relaxed);
        taps.outLevel.store(fSide.value, std::memory_order_relaxed);
    }

private:
    nullspread::SpreadProcessor fDsp;
    nulljsfx::PeakFollower fMid, fSide;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpread)
};

Plugin* createPlugin()
{
    return new PluginSpread();
}

END_NAMESPACE_DISTRHO
