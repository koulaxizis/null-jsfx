/*
 * NULL JSFX - Granular Delay (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "GranularDelayDSP.hpp"
#include "GranularDelaySliders.hpp"

START_NAMESPACE_DISTRHO

class PluginGranularDelay : public NullMultiPlugin
{
public:
    PluginGranularDelay()
        : NullMultiPlugin(nullgranular::kSliders, nullgranular::kNumSliders)
    {
        reset(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "GranularDelay"; }
    const char* getDescription() const override
    {
        return "Grain-cloud delay: time, grain size, density, pitch, spray, reverse, feedback, spread and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '8', 'G', 'd'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
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
        double s[nullgranular::kNumSliders];
        for (uint32_t i = 0; i < nullgranular::kNumSliders; ++i)
            s[i] = sliderValue(nullgranular::kSliders[i], value(i));
        fDsp.setParams(s);
    }

    nullgranular::GranularDelayProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGranularDelay)
};

Plugin* createPlugin()
{
    return new PluginGranularDelay();
}

END_NAMESPACE_DISTRHO
