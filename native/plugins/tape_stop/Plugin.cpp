/*
 * NULL JSFX - Tape Stop (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "TapeStopDSP.hpp"
#include "TapeStopSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTapeStop : public NullMultiPlugin
{
public:
    PluginTapeStop()
        : NullMultiPlugin(nulltapestop::kSliders, nulltapestop::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "TapeStop"; }
    const char* getDescription() const override
    {
        return "Tape stop: playback slows to a halt like a tape motor, then spins back up to the live signal.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '6', 't', 's'); }

    void valueChanged(uint32_t index, float) override
    {
        if (index == 0)
        {
            fDsp.setTrigger(v(0));
            taps.gr.store(float(fDsp.trigger()), std::memory_order_relaxed);
        }
        update();
    }

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
        // for the interface: playback position (-1 idle), and the trigger latch the JSFX keeps in
        // slider1 (the interface sets the parameter back to Idle when the latch clears)
        taps.extra.store(float(fDsp.position()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.trigger()), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nulltapestop::sliderValue(i, value(i)); }

    void update() { fDsp.setParams(v(1), v(2)); }

    nulltapestop::TapeStopProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTapeStop)
};

Plugin* createPlugin()
{
    return new PluginTapeStop();
}

END_NAMESPACE_DISTRHO
