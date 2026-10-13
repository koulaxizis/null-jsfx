/*
 * NULL JSFX - Loudness Meter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "LoudnessMeterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginLoudnessMeter : public LoudnessMeterBase
{
public:
    PluginLoudnessMeter()
    {
        fDsp.setView(&view);
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "LoudnessMeter"; }
    const char* getDescription() const override
    {
        return "EBU R128 / ITU-R BS.1770-4 loudness meter: momentary, short-term, integrated, LRA and true peak.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '0', 'L', 'm'); }

    // Reset: any change of the Reset parameter clears the measurement (in run, on the audio thread)
    void valueChanged(uint32_t index, float v) override
    {
        if (index == 4 && v != fResetLast)
        {
            fResetLast = v;
            fResetPending.store(true, std::memory_order_relaxed);
        }
    }

    void activate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        if (fResetPending.exchange(false, std::memory_order_relaxed))
            fDsp.reset();
        const bool playing = getTimePosition().playing;
        if (playing && !fWasPlaying && value(3) >= 0.5f)
            fDsp.reset();
        fWasPlaying = playing;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, value(5) >= 0.5f);
    }

private:
    nullloud::LoudnessMeter fDsp;
    float fResetLast = 0.0f;
    std::atomic<bool> fResetPending { false };
    bool fWasPlaying = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLoudnessMeter)
};

Plugin* createPlugin()
{
    return new PluginLoudnessMeter();
}

END_NAMESPACE_DISTRHO
