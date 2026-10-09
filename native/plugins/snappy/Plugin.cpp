/*
 * NULL JSFX - Snappy (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 2 s history of the gain and the output level, as in snappy_gfx.jsfx:
 * one entry per 7 ms chunk, pushed into the Taps ring (scopeL = output peak of the chunk,
 * scopeR = the chunk's gain furthest from 0 dB, linear).
 */

#include "NullPlugin.hpp"
#include "SnappyDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginSnappy : public NullOneSliderPlugin
{
public:
    PluginSnappy()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        setup(getSampleRate());
        fDsp.setAmount(value());
        fDsp.reset();
        for (uint32_t i = 0; i < nulljsfx::Taps::kScopeLen; ++i)
            taps.scopeR[i] = 1.0f;
    }

protected:
    const char* getLabel() const override { return "Snappy"; }
    const char* getDescription() const override
    {
        return "Transient punch: hits are boosted (up to +9 dB) and the sustain trimmed (up to -3 dB), stereo-linked.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'n', 'p'); }

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
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inL[i], r = inR[i];
            const double g = fDsp.processSample(l, r);
            outL[i] = float(l);
            outR[i] = float(r);

            fPk = std::fmax(fPk, std::fmax(std::fabs(l), std::fabs(r)));
            fGh = std::fmax(fGh, g);
            fGl = std::fmin(fGl, g);
            if (++fN >= fChunk)
            {
                taps.push(float(fPk), float(fGh * fGl > 1.0 ? fGh : fGl));
                fN = 0;
                fPk = 0.0;
                fGh = 0.0;
                fGl = 1000.0;
            }
        }
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fPk = 0.0;
        fGh = 0.0;
        fGl = 1000.0;
    }

    nullsnappy::SnappyProcessor fDsp;
    uint32_t fChunk = 336, fN = 0;
    double fPk = 0.0, fGh = 0.0, fGl = 1000.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSnappy)
};

Plugin* createPlugin()
{
    return new PluginSnappy();
}

END_NAMESPACE_DISTRHO
