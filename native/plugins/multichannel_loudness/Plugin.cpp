/*
 * NULL JSFX - Multichannel Loudness (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "MultichannelLoudnessSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginMultichannelLoudness : public MultichannelLoudnessBase
{
public:
    PluginMultichannelLoudness()
    {
        fDsp.setView(&view);
        fDsp.setSampleRate(getSampleRate());
        fDsp.setBed(int(std::lround(value(0))));
    }

protected:
    const char* getLabel() const override { return "MultichannelLoudness"; }
    const char* getDescription() const override
    {
        return "EBU R128 / ITU-R BS.1770-4 loudness of a 5.1, 7.1 or 7.1.4 bed: momentary, short-term, integrated, LRA, "
               "true peak and a meter per channel.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'L', 'u'); }

    // the twelve pins, named as the JSFX's
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        static const char* const names[12] = { "L", "R", "C", "LFE", "Ls", "Rs", "Lrs", "Rrs", "Ltf", "Rtf", "Ltr", "Rtr" };
        port.name = names[index];
        port.symbol = String(input ? "in_" : "out_") + String(names[index]).toLower();
    }

    // Bed sets the weights; any change of Reset clears the measurement (in run, on the audio thread)
    void valueChanged(uint32_t index, float v) override
    {
        if (index == 0)
            fDsp.setBed(int(std::lround(v)));
        if (index == 4 && v != fResetLast)
        {
            fResetLast = v;
            fResetPending.store(true, std::memory_order_relaxed);
        }
    }

    void onActivate() override { fDsp.setSampleRate(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        if (fResetPending.exchange(false, std::memory_order_relaxed))
            fDsp.reset();
        const bool playing = getTimePosition().playing;
        if (playing && !fWasPlaying && value(3) >= 0.5f)
            fDsp.reset();
        fWasPlaying = playing;
        fDsp.process(inputs, outputs, frames);
    }

private:
    nullmloud::LoudnessMeter fDsp;
    float fResetLast = 0.0f;
    std::atomic<bool> fResetPending { false };
    bool fWasPlaying = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMultichannelLoudness)
};

Plugin* createPlugin()
{
    return new PluginMultichannelLoudness();
}

END_NAMESPACE_DISTRHO
