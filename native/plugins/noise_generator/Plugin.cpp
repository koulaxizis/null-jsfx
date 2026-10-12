/*
 * NULL JSFX - Noise Generator (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "NoiseGeneratorDSP.hpp"
#include "NoiseGeneratorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginNoiseGenerator : public NullMultiPlugin
{
public:
    PluginNoiseGenerator()
        : NullMultiPlugin(nullnoise::kSliders, nullnoise::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "NoiseGenerator"; }
    const char* getDescription() const override
    {
        return "White, pink or brown noise at a set level, from mono to fully decorrelated stereo.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '5', 'N', 'g'); }

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

    void run(const float**, float** outputs, uint32_t frames) override
    {
        fDsp.process(outputs[0], outputs[1], frames);
    }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(value(0))), value(1), value(2));
    }

    nullnoise::NoiseProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginNoiseGenerator)
};

Plugin* createPlugin()
{
    return new PluginNoiseGenerator();
}

END_NAMESPACE_DISTRHO
