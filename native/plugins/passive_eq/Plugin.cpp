/*
 * NULL JSFX - Passive EQ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in passive_eq_gfx.jsfx): inLevel = the input peak, outLevel = the
 * output peak.
 */

#include "NullPlugin.hpp"
#include "PassiveEqDSP.hpp"
#include "PassiveEqSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPassiveEq : public NullMultiPlugin
{
public:
    PluginPassiveEq()
        : NullMultiPlugin(nullpassiveeq::kSliders, nullpassiveeq::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "PassiveEQ"; }
    const char* getDescription() const override
    {
        return "Program EQ in the passive style: low boost and attenuation at one stepped frequency, a high peak "
               "with bandwidth, a high shelf cut, a tube stage and output.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'E', 'q'); }

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
            const float il = inputs[0][i], ir = inputs[1][i];
            double l = il, r = ir;
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            fIn.process(std::fmax(std::fabs(il), std::fabs(ir)));
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    void update()
    {
        fDsp.setParams(nullpassiveeq::paramsFrom([this](uint32_t i) { return nullpassiveeq::sliderValue(i, value(i)); }));
    }

    nullpassiveeq::PassiveEqProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPassiveEq)
};

Plugin* createPlugin()
{
    return new PluginPassiveEq();
}

END_NAMESPACE_DISTRHO
