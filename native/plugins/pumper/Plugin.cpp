/*
 * NULL JSFX - Pumper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "PumperDSP.hpp"

START_NAMESPACE_DISTRHO

class PluginPumper : public NullOneSliderPlugin
{
public:
    PluginPumper()
        : NullOneSliderPlugin({ "Amount", "amount", 0.0f, 100.0f, 50.0f, "" })
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        fDsp.setAmount(value());
    }

protected:
    const char* getLabel() const override { return "Pumper"; }
    const char* getDescription() const override
    {
        return "Tempo-synced sidechain-style pumping: the level ducks on every beat (up to -20 dB) and swells back.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'm', 'p'); }

    void valueChanged(float v) override { fDsp.setAmount(v); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        double beatPos = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beatPos = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                          tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beatPos = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tp.playing, beatPos, tempo);
        taps.extra.store(float(fDsp.phase()), std::memory_order_relaxed);
    }

private:
    nullpumper::PumperProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPumper)
};

Plugin* createPlugin()
{
    return new PluginPumper();
}

END_NAMESPACE_DISTRHO
