/*
 * NULL JSFX - Space (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SpaceDSP.hpp"
#include "SpaceSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSpace : public NullMultiPlugin
{
public:
    PluginSpace()
        : NullMultiPlugin(nullspace::kSliders, nullspace::kNumSliders)
    {
        update();
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Space"; }
    const char* getDescription() const override
    {
        return "Algorithmic reverb on an 8-line feedback delay network: Room, Chamber, Hall and Galaxy, with size, "
               "decay, damping, detune, pre-delay and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'p', 'c'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void update()
    {
        fDsp.setParams(nullspace::paramsFrom([this](uint32_t i) { return nullspace::sliderValue(i, value(i)); }));
    }

    nullspace::SpaceProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpace)
};

Plugin* createPlugin()
{
    return new PluginSpace();
}

END_NAMESPACE_DISTRHO
