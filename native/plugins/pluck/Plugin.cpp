/*
 * NULL JSFX - Pluck (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders; the transport drives the strummer like the JSFX's
 * play_state, tempo and beat_position. For the display: aux[0..7] = the strings sounding,
 * aux[8..15] = the keys held (a bit per note, 16 per word), extra = the strings sounding * 16
 * + the strum step playing + 1 (0 none).
 */

#include "NullPlugin.hpp"
#include "PluckDSP.hpp"
#include "PluckSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPluck : public NullMultiPlugin
{
public:
    PluginPluck()
        : NullMultiPlugin(nullpluck::kSliders, nullpluck::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Pluck"; }
    const char* getDescription() const override
    {
        return "Plucked strings by Karplus-Strong with body resonances (guitars, harp, koto, pipa, banjo, dulcimer), 12 voices, and a strummer that plays the held chord on the host's beat.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'l', 'k'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setParams(fLast);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
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

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullpluck::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > 3)
                continue;
            fEvents[n++] = { ev.frame, double(ev.data[0]), ev.size > 1 ? double(ev.data[1]) : 0.0,
                             ev.size > 2 ? double(ev.data[2]) : 0.0 };
        }
        fDsp.beginBlock(fEvents, n, tp.playing, tempo, beatPos);
        fDsp.process(outputs[0], outputs[1], frames);

        uint32_t bits[8];
        fDsp.soundingBits(bits);
        int strings = 0;
        for (int i = 0; i < 8; ++i)
            taps.aux[i].store(float(bits[i]), std::memory_order_relaxed);
        fDsp.heldBits(bits);
        for (int i = 0; i < 8; ++i)
            taps.aux[8 + i].store(float(bits[i]), std::memory_order_relaxed);
        strings = fDsp.voices();
        taps.extra.store(float(strings * 16 + fDsp.strumStep() + 1), std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullpluck::kNumParams; ++i)
        {
            const SliderInfo& in = nullpluck::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            fLast[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                      : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                fLast[i] = std::round(fLast[i] * 10.0) / 10.0;
        }
        fDsp.setParams(fLast);
    }

    nullpluck::Pluck fDsp;
    nullpluck::MidiEv fEvents[nullpluck::kMaxEvents];
    double fLast[nullpluck::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPluck)
};

Plugin* createPlugin()
{
    return new PluginPluck();
}

END_NAMESPACE_DISTRHO
