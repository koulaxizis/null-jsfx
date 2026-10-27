/*
 * NULL JSFX - Vocoder (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "VocoderDSP.hpp"
#include "VocoderSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginVocoder : public NullMultiPlugin
{
public:
    PluginVocoder()
        : NullMultiPlugin(nullvocoder::kSliders, nullvocoder::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Vocoder"; }
    const char* getDescription() const override
    {
        return "16-band channel vocoder: the left input speaks through the right input or a saw, pulse or noise carrier.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '9', 'V', 'c'); }

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

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // the band envelopes for the display, in the first slots of the scope buffer
        for (int b = 0; b < nullvocoder::kBands; ++b)
            taps.scopeL[b] = float(fDsp.bandEnv(b));
    }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(value(0))), stepped(1), stepped(2), stepped(3), value(4), value(5), value(6),
                       stepped(7), value(8));
    }

    // a 0.1-step parameter as the JSFX slider holds it: min + k * 0.1 in double (the float
    // parameter alone would detune the oscillator, e.g. 396.3 Hz -> 396.29998779 Hz)
    double stepped(uint32_t i) const
    {
        const SliderInfo& in = nullvocoder::kSliders[i];
        const double step = 1.0 / std::round(1.0 / double(in.step));
        return double(in.min) + std::round((double(value(i)) - double(in.min)) / step) * step;
    }

    nullvocoder::VocoderProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginVocoder)
};

Plugin* createPlugin()
{
    return new PluginVocoder();
}

END_NAMESPACE_DISTRHO
