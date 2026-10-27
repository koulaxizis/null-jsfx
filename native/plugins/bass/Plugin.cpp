/*
 * NULL JSFX - Bass (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. For the display: aux[0..7] = the note playing (16 notes
 * per value, as bits), aux[8..15] = the keys held, extra = phrase * 100 + the phrase's step + 1
 * (0 when it waits). A phrase chosen in the interface arrives as request = phrase + 1.
 */

#include "NullPlugin.hpp"
#include "BassDSP.hpp"
#include "BassSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginBass : public NullMultiPlugin
{
public:
    PluginBass()
        : NullMultiPlugin(nullbass::kSliders, nullbass::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Bass"; }
    const char* getDescription() const override
    {
        return "Monophonic synth bass and 808 with glide, drive, a resonant 24 dB low-pass and a phrase player: the keys below C2 choose a phrase, the keys above play it.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'a', 's'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.setParams(fLast);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void process(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        // the JSFX sends every message it receives on (midisend in @block); SysEx, which the
        // JSFX never sees, passes through there as well
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]);

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
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullbass::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > 3)
                continue;
            fEvents[n++] = { ev.frame, double(ev.data[0]), ev.size > 1 ? double(ev.data[1]) : 0.0,
                             ev.size > 2 ? double(ev.data[2]) : 0.0 };
        }
        fDsp.beginBlock(fEvents, n, tp.playing, tempo, beatPos, taps.request.exchange(0));
        fDsp.process(outputs[0], outputs[1], frames);

        // the display: the note playing, the keys held, the phrase and its step
        uint32_t bits[16] = {};
        if (fDsp.vStage == 1 || fDsp.vStage == 2)
        {
            const int t = int(std::lround(fDsp.vTgt));
            if (t >= 0 && t < 128)
                bits[t / 16] |= 1u << (t % 16);
        }
        for (int i = 0; i < fDsp.hn; ++i)
        {
            const int k = int(fDsp.HS[i]);
            if (k >= 0 && k < 128)
                bits[8 + k / 16] |= 1u << (k % 16);
        }
        for (int i = 0; i < 16; ++i)
            taps.aux[i].store(float(bits[i]), std::memory_order_relaxed);
        const bool waits = fDsp.phSel <= 0 || fDsp.hn == 0 || fDsp.phS <= -1000;
        const double step = waits ? 0.0 : fDsp.phS - std::floor(fDsp.phS / 16) * 16 + 1;
        taps.extra.store(float(fDsp.phSel * 100 + step), std::memory_order_relaxed);
        float pk = 0.0f;
        for (uint32_t i = 0; i < frames; ++i)
            pk = std::max(pk, std::fabs(outputs[0][i]));
        taps.outLevel.store(pk, std::memory_order_relaxed);
    }

private:
    void update()
    {
        for (uint32_t i = 0; i < nullbass::kNumParams; ++i)
        {
            const SliderInfo& in = nullbass::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            fLast[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                      : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                fLast[i] = std::round(fLast[i] * 10.0) / 10.0;
        }
        fDsp.setParams(fLast);
    }

    nullbass::Bass fDsp;
    nullbass::MidiEv fEvents[nullbass::kMaxEvents];
    double fLast[nullbass::kNumParams] = {};

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBass)
};

Plugin* createPlugin()
{
    return new PluginBass();
}

END_NAMESPACE_DISTRHO
