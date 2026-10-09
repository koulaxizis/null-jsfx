/*
 * NULL JSFX - Pressure (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PressureDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginPressure : public NullOneSliderPlugin
{
public:
    PluginPressure()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        setup(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Pressure"; }
    const char* getDescription() const override
    {
        return "Bus glue: a stereo-linked compressor whose threshold drops (0 to -30 dB) and ratio rises (1:1 to 6:1) with Amount, with makeup gain.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'r', 's'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];
        float in = 0.0f, out = 0.0f;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            in = fIn.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
            fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
            out = fOut.process(std::fmax(std::fabs(outL[i]), std::fabs(outR[i])));
        }
        if (frames > 0)
        {
            taps.inLevel.store(in, std::memory_order_relaxed);
            taps.outLevel.store(out, std::memory_order_relaxed);
        }
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fDsp.gain(), 0.0000001))), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.detectorDb()), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    nullpressure::PressureProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPressure)
};

Plugin* createPlugin()
{
    return new PluginPressure();
}

END_NAMESPACE_DISTRHO
