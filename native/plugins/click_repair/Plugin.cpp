/*
 * NULL JSFX - Click Repair (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ClickRepairDSP.hpp"
#include "ClickRepairSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginClickRepair : public NullMultiPlugin
{
public:
    PluginClickRepair()
        : NullMultiPlugin(nullclick::kSliders, nullclick::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "ClickRepair"; }
    const char* getDescription() const override
    {
        return "Finds clicks, pops and spikes and redraws them as a line between the good samples around them.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '5', 'C', 'r'); }

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
        // one sample at a time so the display gets the unrepaired signal next to the output
        for (uint32_t i = 0; i < frames; ++i)
        {
            fDsp.process(inputs[0] + i, inputs[1] + i, outputs[0] + i, outputs[1] + i, 1);
            taps.push(float(fDsp.lastInMid()), (outputs[0][i] + outputs[1][i]) * 0.5f);
        }
        taps.extra.store(float(fDsp.clicks()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(value(0), value(1));
        setLatency(uint32_t(fDsp.latency())); // the JSFX's pdc_delay
    }

    nullclick::ClickRepairProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginClickRepair)
};

Plugin* createPlugin()
{
    return new PluginClickRepair();
}

END_NAMESPACE_DISTRHO
