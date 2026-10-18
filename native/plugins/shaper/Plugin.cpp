/*
 * NULL JSFX - Shaper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the oversampling filters' delay as latency (the JSFX's pdc_delay). For the display:
 * inLevel = peak of the curve's input (after Drive and Bias), outLevel = output peak.
 */

#include "NullPlugin.hpp"
#include "ShaperDSP.hpp"
#include "ShaperSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginShaper : public NullMultiPlugin
{
public:
    PluginShaper()
        : NullMultiPlugin(nullshaper::kSliders, nullshaper::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Shaper"; }
    const char* getDescription() const override
    {
        return "Waveshaping distortion with a drawable transfer curve, preset shapes, bias for even harmonics, 2x/4x oversampling, tone, mix and output.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'h', 'p'); }

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
        const double gin = fDsp.inputGain(), bias = fDsp.bias();
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            const double x = std::fmax(std::fabs(l * gin + bias), std::fabs(r * gin + bias));
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            fIn.process(float(x));
            fOut.process(float(std::fmax(std::fabs(l), std::fabs(r))));
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nullshaper::sliderValue(i, value(i)); }

    void setup(double sr)
    {
        fDsp.init(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    void update()
    {
        nullshaper::Params p;
        p.shape = int(std::lround(v(0)));
        p.drive = v(1);
        p.bias = v(2);
        p.symmetry = int(std::lround(v(3)));
        p.oversampling = int(std::lround(v(4)));
        p.tone = v(5);
        p.mix = v(6);
        p.output = v(7);
        for (uint32_t i = 0; i < 16; ++i)
            p.points[i] = v(nullshaper::kFirstPoint + i);
        fDsp.setParams(p);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay
    }

    nullshaper::ShaperProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginShaper)
};

Plugin* createPlugin()
{
    return new PluginShaper();
}

END_NAMESPACE_DISTRHO
