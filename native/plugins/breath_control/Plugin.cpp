/*
 * NULL JSFX - Breath Control (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 1.8 s history as in breath_control_gfx.jsfx: one entry per 7 ms chunk, pushed into
 * the Taps ring (scopeL = the square root of the chunk's highest detector power, scopeR = its lowest gain).
 * inLevel = square root of the detector power, outLevel = output peak meter, gr = gain reduction in dB.
 */

#include "NullPlugin.hpp"
#include "BreathControlDSP.hpp"
#include "BreathControlSliders.hpp"

#include <algorithm>

START_NAMESPACE_DISTRHO

class PluginBreath : public NullMultiPlugin
{
public:
    PluginBreath()
        : NullMultiPlugin(nullbreath::kSliders, nullbreath::kNumSliders)
    {
        setup(getSampleRate());
        update();
        for (uint32_t i = 0; i < nulljsfx::Taps::kScopeLen; ++i)
            taps.scopeR[i] = 1.0f;
    }

protected:
    const char* getLabel() const override { return "Breath Control"; }
    const char* getDescription() const override
    {
        return "Breath reducer: below Threshold (breaths, room noise between phrases) the gain moves down to Range, with 3 dB hysteresis.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'b', 'r'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        setup(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        setup(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            const double det = fDsp.envelope(), g = fDsp.gain();
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
            fDmax = std::fmax(fDmax, det);
            fGmin = std::fmin(fGmin, g);
            if (++fN >= fChunk)
            {
                taps.push(float(std::sqrt(fDmax)), float(fGmin));
                fN = 0;
                fDmax = 0.0;
                fGmin = 1.0;
            }
        }
        taps.inLevel.store(float(std::sqrt(fDsp.envelope())), std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fDsp.gain(), 0.0000001))), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fOut.setup(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fDmax = 0.0;
        fGmin = 1.0;
    }

    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3));
    }

    nullbreath::BreathProcessor fDsp;
    nulljsfx::PeakFollower fOut;
    uint32_t fChunk = 1, fN = 0;
    double fDmax = 0.0, fGmin = 1.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBreath)
};

Plugin* createPlugin()
{
    return new PluginBreath();
}

END_NAMESPACE_DISTRHO
