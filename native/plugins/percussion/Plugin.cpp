/*
 * NULL JSFX - Percussion (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. The MIDI of a block (note-ons and note-offs for the
 * groove keys, all notes off) goes to the DSP at its frames, like the JSFX's @block queue.
 */

#include "NullPlugin.hpp"
#include "PercussionDSP.hpp"
#include "PercussionSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginPercussion : public NullMultiPlugin
{
public:
    PluginPercussion()
        : NullMultiPlugin(nullperc::kSliders, nullperc::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Percussion"; }
    const char* getDescription() const override
    {
        return "Synthesized hand percussion (conga, bongos, shaker, tambourine, cowbell, claves, guiro, cabasa, "
               "triangle) with eight grooves that follow the host, played by MIDI too.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'r', 'c'); }

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

    void process(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
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
        // REAPER runs the JSFX's @init again when the transport starts
        if (tp.playing && !fWasPlaying)
        {
            fDsp.reset();
            update();
        }
        fWasPlaying = tp.playing;

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < nullperc::kMaxEvents; ++i)
        {
            const MidiEvent& ev = midi[i];
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 1 && ev.size <= 3 && d[0] >= 0x80 && d[0] < 0xF0)
                fEvents[n++] = { ev.frame, d[0], uint8_t(ev.size > 1 ? d[1] : 0), uint8_t(ev.size > 2 ? d[2] : 0) };
        }
        fDsp.beginBlock(fEvents, n, tp.playing, tempo, beatPos);
        if (const int req = taps.request.exchange(0))
            fDsp.trig(req - 1, 0, 1.0);
        fDsp.process(outputs, frames);

        taps.extra.store(float(fDsp.step), std::memory_order_relaxed);
        for (int t = 0; t < nullperc::kTracks; ++t)
            taps.aux[t].store(float(fDsp.hits[t]), std::memory_order_relaxed);
        taps.aux[nullperc::kTracks].store(float(fDsp.groove), std::memory_order_relaxed);
    }

private:
    void update()
    {
        double v[nullperc::kNumSliders];
        for (uint32_t i = 0; i < nullperc::kNumSliders; ++i)
        {
            const SliderInfo& in = nullperc::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            v[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                  : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                v[i] = std::round(v[i] * 10.0) / 10.0;
        }
        fDsp.setParams(v);
    }

    nullperc::Percussion fDsp;
    nullperc::MidiEv fEvents[nullperc::kMaxEvents];
    bool fWasPlaying = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPercussion)
};

Plugin* createPlugin()
{
    return new PluginPercussion();
}

END_NAMESPACE_DISTRHO
