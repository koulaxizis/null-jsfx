/*
 * NULL JSFX - FM (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders. MIDI (notes, sustain, mod wheel, pitch bend, all notes
 * off) is played at its frame like the JSFX's @block queue. The interface gets each voice's
 * loudest carrier envelope (aux[0..7]).
 */

#include "NullPlugin.hpp"
#include "FmDSP.hpp"
#include "FmSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginFm : public NullMultiPlugin
{
public:
    PluginFm()
        : NullMultiPlugin(nullfm::kSliders, nullfm::kNumParams)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "FM"; }
    const char* getDescription() const override
    {
        return "Four-operator FM synthesizer with eight algorithms, per-operator envelopes and velocity, feedback, chorus and reverb.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'F', 'M', 's'); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input)
            return NullMultiPlugin::initAudioPort(input, index, port);
        port.groupId = kPortGroupStereo;
        port.name = index == 0 ? "Left" : "Right";
        port.symbol = index == 0 ? "out_left" : "out_right";
    }

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
        // the JSFX sends every message it receives on (midisend in @block); SysEx, which the
        // JSFX never sees, passes through there as well
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]);

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullfm::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            if (ev.size < 1 || ev.size > MidiEvent::kDataSize)
                continue;
            const uint8_t* d = ev.data;
            if (d[0] < 0x80 || d[0] >= 0xF0)
                continue;
            fEvents[n++] = { ev.frame, d[0], uint8_t(ev.size > 1 ? d[1] : 0), uint8_t(ev.size > 2 ? d[2] : 0) };
        }
        fDsp.beginBlock(fEvents, n);
        fDsp.process(outputs[0], outputs[1], frames);

        for (int v = 0; v < nullfm::kVoices; ++v)
        {
            const nullfm::Fm::Voice& vo = fDsp.voice(v);
            float e = 0.0f;
            if (vo.st > 0)
                for (int k = 0; k < 4; ++k)
                    e = std::max(e, float(vo.m[4 + k]));
            taps.aux[v].store(e, std::memory_order_relaxed);
        }
    }

private:
    void update()
    {
        double v[nullfm::kNumSliders];
        for (uint32_t i = 0; i < nullfm::kNumSliders; ++i)
        {
            const SliderInfo& in = nullfm::kSliders[i];
            // back on the JSFX's grid in double (a float 0.01 step is not 0.01)
            double x = double(value(i));
            if (in.step > 0.0f)
            {
                x = double(in.min) + std::round((x - in.min) / double(in.step)) * double(in.step);
                if (in.step < 0.05f)
                    x = std::round(x * 100.0) / 100.0;
                else if (in.step < 1.0f)
                    x = std::round(x * 10.0) / 10.0;
            }
            v[i] = x;
        }
        fDsp.setParams(v);
    }

    nullfm::Fm fDsp;
    nullfm::MidiEv fEvents[nullfm::kMaxEvents];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginFm)
};

Plugin* createPlugin()
{
    return new PluginFm();
}

END_NAMESPACE_DISTRHO
