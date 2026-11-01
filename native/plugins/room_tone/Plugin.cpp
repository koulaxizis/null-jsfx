/*
 * NULL JSFX - Room Tone (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "RoomToneSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginRoomTone : public RoomToneBase
{
public:
    PluginRoomTone()
    {
        fDsp.setView(&view);
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "RoomTone"; }
    const char* getDescription() const override
    {
        return "Learns the room tone and fills the gaps of the dialogue with new noise of the same spectrum.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 't', 'n'); }

    void valueChanged(uint32_t, float) override { update(); }

    // like the JSFX's @init: the learnt profile survives unless the sample rate changes
    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        update();
    }

    void update()
    {
        float v[nullrt::kNumSliders];
        for (uint32_t i = 0; i < nullrt::kNumSliders; ++i)
            v[i] = value(i);
        nullrt::apply(fDsp, v);
    }

    nullrt::RoomTone fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRoomTone)
};

Plugin* createPlugin()
{
    return new PluginRoomTone();
}

END_NAMESPACE_DISTRHO
