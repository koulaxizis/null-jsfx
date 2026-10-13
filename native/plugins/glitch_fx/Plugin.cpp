/*
 * NULL JSFX - Glitch FX (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "GlitchFxDSP.hpp"
#include "GlitchFxSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginGlitchFx : public NullMultiPlugin
{
public:
    PluginGlitchFx()
        : NullMultiPlugin(nullglitch::kSliders, nullglitch::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "GlitchFX"; }
    const char* getDescription() const override
    {
        return "Tempo-synced stutter with gate, pitch shift, random reverse, tape stop and granular variations.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '7', 'G', 'l'); }

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
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo);
        taps.extra.store(float(tempo), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullglitch::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullglitch::GlitchProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGlitchFx)
};

Plugin* createPlugin()
{
    return new PluginGlitchFx();
}

END_NAMESPACE_DISTRHO
