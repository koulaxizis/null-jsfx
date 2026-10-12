/*
 * NULL JSFX - Harmonizer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "HarmonizerDSP.hpp"
#include "HarmonizerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginHarmonizer : public NullMultiPlugin
{
public:
    PluginHarmonizer()
        : NullMultiPlugin(nullharmonizer::kSliders, nullharmonizer::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Harmonizer"; }
    const char* getDescription() const override
    {
        return "Two pitch-shifted harmony voices at fixed intervals, detuned, delayed and spread, with an equal-power mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '8', 'H', 'm'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    // @init then @slider
    void reset(double sr)
    {
        fDsp.setSampleRate(sr);
        update();
    }

    // the parameter as the JSFX holds it: a fractional step (0.1, 0.5) is a double in the JSFX,
    // so the float parameter is snapped back onto min + k * step in double (as REAPER does)
    static double sliderValue(const SliderInfo& in, float v)
    {
        if (in.step <= 0.0f || in.step >= 1.0f || in.options)
            return v;
        const double per = std::round(1.0 / double(in.step)); // steps per unit, e.g. 10
        const double st = 1.0 / per, lo = std::round(double(in.min) * per) / per;
        return std::fmin(double(in.max), lo + std::round((double(v) - lo) / st) * st);
    }

    void update()
    {
        double s[nullharmonizer::kNumSliders];
        for (uint32_t i = 0; i < nullharmonizer::kNumSliders; ++i)
            s[i] = sliderValue(nullharmonizer::kSliders[i], value(i));
        fDsp.setParams(s);
    }

    nullharmonizer::HarmonizerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHarmonizer)
};

Plugin* createPlugin()
{
    return new PluginHarmonizer();
}

END_NAMESPACE_DISTRHO
