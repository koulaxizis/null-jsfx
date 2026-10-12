/*
 * NULL JSFX - Volume Shaper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "VolumeShaperDSP.hpp"
#include "VolumeShaperSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginVolumeShaper : public NullMultiPlugin
{
public:
    PluginVolumeShaper()
        : NullMultiPlugin(nullvolshaper::kSliders, nullvolshaper::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        update();
    }

protected:
    const char* getLabel() const override { return "VolumeShaper"; }
    const char* getDescription() const override
    {
        return "Tempo-synced volume envelope: pump, saw, gate, trance, sine and stutter shapes with rate, depth, "
               "smoothing, phase and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'V', 'S', 'h'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
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
        // the cycle position (playhead) and the smoothed gain (before Mix), for the display
        taps.extra.store(float(fDsp.position()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.gain()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullvolshaper::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullvolshaper::VolumeShaperProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginVolumeShaper)
};

Plugin* createPlugin()
{
    return new PluginVolumeShaper();
}

END_NAMESPACE_DISTRHO
