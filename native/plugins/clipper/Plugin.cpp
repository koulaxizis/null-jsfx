/*
 * NULL JSFX - Clipper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the oversampling filters' delay as latency (the JSFX's pdc_delay). For the display:
 * inLevel = peak of the input after Input, outLevel = output peak, gr = the clip's gain
 * reduction in dB, held like the GFX JSFX's cm_g.
 */

#include "NullPlugin.hpp"
#include "ClipperDSP.hpp"
#include "ClipperSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginClipper : public NullMultiPlugin
{
public:
    PluginClipper()
        : NullMultiPlugin(nullclipper::kSliders, nullclipper::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Clipper"; }
    const char* getDescription() const override
    {
        return "Clips peaks at a ceiling, hard or with a soft knee, with 2x to 8x oversampling against aliasing, mix and output.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'C', 'l', 'p'); }

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
        const double gin = fDsp.inputGain();
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            const double x = std::fmax(std::fabs(l * gin), std::fabs(r * gin));
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            fIn.process(float(x));
            fOut.process(float(std::fmax(std::fabs(l), std::fabs(r))));
            const double cg = fDsp.clipGain();
            fG = cg < fG ? cg : 1.0 - (1.0 - fG) * fRel;
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fG, 0.0000001))), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nullclipper::sliderValue(i, value(i)); }

    void setup(double sr)
    {
        fDsp.init(sr);
        fIn.setup(sr);
        fOut.setup(sr);
        fRel = std::exp(-1.0 / (0.3 * sr));
        fG = 1.0;
    }

    void update()
    {
        nullclipper::Params p;
        p.input = v(0);
        p.ceiling = v(1);
        p.knee = v(2);
        p.oversampling = int(std::lround(v(3)));
        p.mix = v(4);
        p.output = v(5);
        p.link = int(std::lround(v(6)));
        fDsp.setParams(p);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay
    }

    nullclipper::ClipperProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;
    double fG = 1.0, fRel = 0.9999;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginClipper)
};

Plugin* createPlugin()
{
    return new PluginClipper();
}

END_NAMESPACE_DISTRHO
