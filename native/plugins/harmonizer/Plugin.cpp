/*
 * NULL JSFX - Harmonizer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * MIDI notes reach the DSP at their frames (Mode MIDI), and every MIDI message passes through
 * as the JSFX's midisend does. For the display: aux[0..1] = the voices' intervals now, aux[2..3]
 * their levels (Mode MIDI fades), aux[4] = 1 + the note the scale is on (0 = none yet),
 * aux[5] = 1 while the input is pitched, aux[6..7] = 1 + the held notes voice 1 and 2 sing,
 * aux[8] = 1 when the pitch-tracking path runs.
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
        return "Two pitch-shifted harmony voices at fixed intervals, scale degrees of a key or held MIDI notes, detuned, delayed and spread, with optional formant preservation and an equal-power mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '8', 'H', 'm'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { reset(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { reset(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]); // every message passes through
        uint32_t mi = 0;
        for (uint32_t i = 0; i < frames; ++i)
        {
            for (; mi < midiCount && midi[mi].frame <= i; ++mi)
            {
                const uint8_t* d = midi[mi].size > MidiEvent::kDataSize ? midi[mi].dataExt : midi[mi].data;
                if (midi[mi].size >= 3)
                    fDsp.midi(d[0], d[1], d[2]);
            }
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
        }
        taps.aux[0].store(float(fDsp.interval1()), std::memory_order_relaxed);
        taps.aux[1].store(float(fDsp.interval2()), std::memory_order_relaxed);
        taps.aux[2].store(float(fDsp.gate1()), std::memory_order_relaxed);
        taps.aux[3].store(float(fDsp.gate2()), std::memory_order_relaxed);
        taps.aux[4].store(float(fDsp.scaleNote() + 1.0), std::memory_order_relaxed);
        taps.aux[5].store(fDsp.pitched() ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.aux[6].store(float(fDsp.note1() + 1), std::memory_order_relaxed);
        taps.aux[7].store(float(fDsp.note2() + 1), std::memory_order_relaxed);
        taps.aux[8].store(fDsp.advanced() ? 1.0f : 0.0f, std::memory_order_relaxed);
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
