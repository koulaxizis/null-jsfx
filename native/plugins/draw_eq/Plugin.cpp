/*
 * NULL JSFX - Draw EQ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * For the display: the mono input and output go into the analyzer ring (EqAnalyzer.hpp).
 */

#include "DrawEqPlugin.hpp"
#include "DrawEqDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginDrawEq : public DrawEqBase
{
public:
    PluginDrawEq()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "DrawEQ"; }
    const char* getDescription() const override
    {
        return "An EQ you draw: ten points at octave-spaced frequencies joined by a smooth curve, with smoothness, output and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'e', 'q'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const float pre = (inputs[0][i] + inputs[1][i]) * 0.5f;
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            analyzer.push(pre, float((l + r) * 0.5));
        }
    }

private:
    void update()
    {
        nulldraweq::Params p;
        for (int i = 0; i < nulldraweq::kPoints; ++i)
            p.gain[i] = nulldraweq::sliderValue(uint32_t(i), value(uint32_t(i)));
        p.smooth = nulldraweq::sliderValue(nulldraweq::kSmoothness, value(nulldraweq::kSmoothness));
        p.output = nulldraweq::sliderValue(nulldraweq::kOutput, value(nulldraweq::kOutput));
        p.mix = nulldraweq::sliderValue(nulldraweq::kMix, value(nulldraweq::kMix));
        fDsp.setParams(p);
        analyzer.sampleRate.store(float(getSampleRate()), std::memory_order_relaxed);
    }

    nulldraweq::DrawEqProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDrawEq)
};

Plugin* createPlugin()
{
    return new PluginDrawEq();
}

END_NAMESPACE_DISTRHO
