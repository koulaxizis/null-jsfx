/*
 * NULL JSFX - Parametric EQ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ParametricEqDSP.hpp"
#include "ParametricEqSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginParametricEq : public NullMultiPlugin
{
public:
    PluginParametricEq()
        : NullMultiPlugin(nullpeq::kSliders, nullpeq::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "ParametricEQ"; }
    const char* getDescription() const override
    {
        return "Five fully parametric bands in series, each one of eight filter shapes, with bypass and solo per band.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '2', 'P', 'E'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void update()
    {
        nullpeq::BandParams p[nullpeq::kBands];
        nullpeq::bandParams([this](uint32_t i) { return double(value(i)); }, p);
        fDsp.setParams(p);
    }

    nullpeq::ParametricEqProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginParametricEq)
};

Plugin* createPlugin()
{
    return new PluginParametricEq();
}

END_NAMESPACE_DISTRHO
