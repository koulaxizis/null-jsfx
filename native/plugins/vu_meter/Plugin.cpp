/*
 * NULL JSFX - VU Meter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "VuMeterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginVuMeter : public VuMeterBase
{
public:
    PluginVuMeter()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "VUMeter"; }
    const char* getDescription() const override
    {
        return "Analog-style level meter per channel: VU, EBU PPM, K-14 or K-20, with peak hold and OVER.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'V', 'U', 'm'); }

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
        if (view.resetRequest.exchange(false, std::memory_order_relaxed))
            fDsp.reset();
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        fDsp.publish(view);
    }

private:
    void update() { fDsp.setParams(value(0), value(1), value(2), value(3)); }

    nullvu::VuMeter fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginVuMeter)
};

Plugin* createPlugin()
{
    return new PluginVuMeter();
}

END_NAMESPACE_DISTRHO
