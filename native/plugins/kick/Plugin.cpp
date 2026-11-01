/*
 * NULL JSFX - Kick (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in kick_gfx.jsfx): outLevel = the output peak of the block.
 */

#include "NullPlugin.hpp"
#include "KickDSP.hpp"
#include "KickSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginKick : public NullMultiPlugin
{
public:
    PluginKick()
        : NullMultiPlugin(nullkick::kSliders, nullkick::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "Kick"; }
    const char* getDescription() const override
    {
        return "Kick drum synthesizer played from MIDI: a sine body with a pitch sweep, a click, drive and decay, tuned to Pitch or to the notes.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'K', 'i', 'k'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        // the JSFX sends every message it receives on (midisend in @block); SysEx, which the
        // JSFX never sees, passes through there as well
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]);

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullkick::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > 3)
                continue;
            fEvents[n++] = { ev.frame, double(ev.data[0]), ev.size > 1 ? double(ev.data[1]) : 0.0,
                             ev.size > 2 ? double(ev.data[2]) : 0.0 };
        }
        fDsp.beginBlock(fEvents, n);
        fDsp.process(outputs[0], outputs[1], frames);

        float pk = 0.0f;
        for (uint32_t i = 0; i < frames; ++i)
            pk = std::max(pk, std::fabs(outputs[0][i]));
        taps.outLevel.store(pk, std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullkick::paramsFrom([this](uint32_t i) { return nullkick::sliderValue(i, value(i)); }));
    }

    nullkick::KickProcessor fDsp;
    nullkick::MidiEv fEvents[nullkick::kMaxEvents];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginKick)
};

Plugin* createPlugin()
{
    return new PluginKick();
}

END_NAMESPACE_DISTRHO
