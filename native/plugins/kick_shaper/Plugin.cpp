/*
 * NULL JSFX - Kick Shaper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "KickShaperDSP.hpp"
#include "KickShaperSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginKickShaper : public NullMultiPlugin
{
public:
    PluginKickShaper()
        : NullMultiPlugin(nullkick::kSliders, nullkick::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "KickShaper"; }
    const char* getDescription() const override
    {
        return "Kick drum shaper: attack, body, thump and click bands, and a decay that tightens the tail.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '7', 'K', 's'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void update()
    {
        fDsp.setParams(nullkick::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullkick::KickShaperProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginKickShaper)
};

Plugin* createPlugin()
{
    return new PluginKickShaper();
}

END_NAMESPACE_DISTRHO
