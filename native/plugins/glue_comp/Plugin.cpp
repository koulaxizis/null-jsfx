/*
 * NULL JSFX - Glue Comp (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in glue_comp_gfx.jsfx): inLevel = the detector's peak level (after the
 * sidechain high-pass), outLevel = the output peak, gr = gain reduction in dB.
 */

#include "NullPlugin.hpp"
#include "GlueCompDSP.hpp"
#include "GlueCompSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginGlueComp : public NullMultiPlugin
{
public:
    PluginGlueComp()
        : NullMultiPlugin(nullgluecomp::kSliders, nullgluecomp::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "GlueComp"; }
    const char* getDescription() const override
    {
        return "Stereo bus compressor: linked peak detector, soft knee, stepped attack and release with an auto "
               "release, makeup, parallel mix and a sidechain high-pass.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'G', 'l', 'u'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            fIn.process(float(fDsp.keyAbs()));
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.gr.store(float(fDsp.gr()), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nullgluecomp::sliderValue(i, value(i)); }

    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    void update()
    {
        nullgluecomp::Params p;
        p.threshold = v(0);
        p.ratio = int(v(1));
        p.attack = int(v(2));
        p.release = int(v(3));
        p.makeup = v(4);
        p.mix = v(5);
        p.hpf = v(6);
        fDsp.setParams(p);
    }

    nullgluecomp::GlueCompProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGlueComp)
};

Plugin* createPlugin()
{
    return new PluginGlueComp();
}

END_NAMESPACE_DISTRHO
