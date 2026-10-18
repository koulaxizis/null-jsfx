/*
 * NULL JSFX - Step FX (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Takes the tempo and beat position from the host like the JSFX (120 bpm and free-running
 * when the host reports none), MIDI notes C3..G3 for MIDI Trigger, and passes every MIDI
 * message through as the JSFX's midisend does. For the display: extra = the playing step
 * (-1 under MIDI Trigger), aux[0] = the effect the held notes pick, gr = the tempo.
 */

#include "NullPlugin.hpp"
#include "StepFxDSP.hpp"
#include "StepFxSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginStepFx : public NullMultiPlugin
{
public:
    PluginStepFx()
        : NullMultiPlugin(nullstepfx::kSliders, nullstepfx::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "StepFx"; }
    const char* getDescription() const override
    {
        return "16-step sequencer that plays a different effect on every step (repeat, reverse, filter, gate, tape stop, "
               "pitch, crush), synced to the host tempo or triggered by MIDI notes.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 't', 'F'); }

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

    void run(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
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
        for (uint32_t i = 0; i < midiCount; ++i)
        {
            const MidiEvent& ev = midi[i];
            writeMidiEvent(ev); // every message passes through
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 3 && n < kMaxMidi)
                fMidi[n++] = { ev.frame, d[0], d[1], d[2] };
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo, tp.playing, beatPos, fMidi, n);

        taps.extra.store(float(fDsp.currentStep()), std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.midiEffect()), std::memory_order_relaxed);
        taps.gr.store(float(tempo), std::memory_order_relaxed);
    }

private:
    static constexpr uint32_t kMaxMidi = 256;

    void update()
    {
        fDsp.setParams(nullstepfx::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullstepfx::StepFxProcessor fDsp;
    nullstepfx::MidiMsg fMidi[kMaxMidi];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginStepFx)
};

Plugin* createPlugin()
{
    return new PluginStepFx();
}

END_NAMESPACE_DISTRHO
