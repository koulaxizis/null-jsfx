/*
 * NULL JSFX - Multiband Comp (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "MultibandCompressorDSP.hpp"
#include "MultibandCompressorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginMultibandCompressor : public NullMultiPlugin
{
public:
    PluginMultibandCompressor()
        : NullMultiPlugin(nullmultiband::kSliders, nullmultiband::kNumSliders)
    {
        update();
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "MultibandComp"; }
    const char* getDescription() const override
    {
        return "Four-band compressor: three crossovers, threshold, ratio, makeup and mode per band, shared attack, release and knee.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '3', 'm', 'b'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // display: gain reduction per band in dB, in scopeL[0..3] (low to high)
        for (int b = 0; b < 4; ++b)
            taps.scopeL[b] = float(-20.0 * std::log10(std::fmax(fDsp.env(b), 0.0000001)));
    }

private:
    void update()
    {
        nullmultiband::Params p;
        for (int k = 0; k < 3; ++k)
            p.xover[k] = std::round(value(uint32_t(k)));
        for (int b = 0; b < 4; ++b)
        {
            const uint32_t o = 3 + 4 * uint32_t(b);
            p.band[b].thr = std::round(value(o));
            p.band[b].ratio = nullmultiband::tenth(value(o + 1));
            p.band[b].makeupDb = std::round(value(o + 2));
            p.band[b].mode = int(std::lround(value(o + 3)));
        }
        p.attackMs = std::round(value(19));
        p.releaseMs = std::round(value(20));
        p.kneePct = std::round(value(21));
        p.masterDb = std::round(value(22));
        fDsp.setParams(p);
    }

    nullmultiband::MultibandProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMultibandCompressor)
};

Plugin* createPlugin()
{
    return new PluginMultibandCompressor();
}

END_NAMESPACE_DISTRHO
