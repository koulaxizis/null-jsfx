/*
 * NULL JSFX - Key Detect (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A meter: the audio passes through unchanged and KeyDetect (key_detect.jsfx's @sample) finds
 * the key. For the display: aux[0..11] = the chroma, aux[12] / aux[14] = the key and the
 * runner-up (0..11 major, 12..23 minor, -1 none), aux[13] / aux[15] = their correlations.
 */

#include "NullPlugin.hpp"
#include "KeyDetectDSP.hpp"
#include "KeyDetectSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginKeyDetect : public NullMultiPlugin
{
public:
    PluginKeyDetect()
        : NullMultiPlugin(nullkeydetect::kSliders, nullkeydetect::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "KeyDetect"; }
    const char* getDescription() const override
    {
        return "Finds the key of what it hears (chroma and key profiles) and shows the Key and Scale to set in Scale and Pitch Correction.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'K', 'd', 't'); }

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
        for (int i = 0; i < 12; ++i)
            taps.aux[i].store(float(fDsp.chroma(i)), std::memory_order_relaxed);
        taps.aux[12].store(float(fDsp.key()), std::memory_order_relaxed);
        taps.aux[13].store(float(fDsp.confidence()), std::memory_order_relaxed);
        taps.aux[14].store(float(fDsp.key2()), std::memory_order_relaxed);
        taps.aux[15].store(float(fDsp.confidence2()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullkeydetect::paramsFrom([this](uint32_t i) { return nullkeydetect::sliderValue(i, value(i)); }));
    }

    nullkeydetect::KeyDetect fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginKeyDetect)
};

Plugin* createPlugin()
{
    return new PluginKeyDetect();
}

END_NAMESPACE_DISTRHO
