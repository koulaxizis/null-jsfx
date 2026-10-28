/*
 * NULL JSFX - Surround Panner (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "SurroundPannerPlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginSurroundPanner : public SurroundPannerBase
{
public:
    PluginSurroundPanner()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SurroundPanner"; }
    const char* getDescription() const override
    {
        return "Pans a mono or stereo track into a 5.1, 7.1 or 7.1.4 bed: azimuth, elevation, spread, "
               "a centre that can turn phantom, and an LFE send.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'r', 'P'); }

    // a stereo input; the twelve outputs are one bed, named as the JSFX pins
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        if (input)
        {
            port.groupId = kPortGroupStereo;
            NullMultiPlugin::initAudioPort(input, index, port);
            return;
        }
        port.name = nullpan::kPinNames[index];
        port.symbol = String("out_") + String(nullpan::kPinNames[index]).toLower();
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

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        double out[nullpan::kPins];
        float pk[nullpan::kPins] {};
        for (uint32_t i = 0; i < frames; ++i)
        {
            fDsp.processSample(inputs[0][i], inputs[1][i], out);
            for (int k = 0; k < nullpan::kPins; ++k)
            {
                outputs[k][i] = float(out[k]);
                pk[k] = std::max(pk[k], float(std::fabs(out[k])));
            }
        }
        for (int k = 0; k < nullpan::kPins; ++k)
            if (pk[k] > peak[k].load(std::memory_order_relaxed))
                peak[k].store(pk[k], std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullpan::sliderValue(0, value(0)), nullpan::sliderValue(1, value(1)),
                       nullpan::sliderValue(2, value(2)), nullpan::sliderValue(3, value(3)),
                       nullpan::sliderValue(4, value(4)), nullpan::sliderValue(5, value(5)),
                       nullpan::sliderValue(6, value(6)), nullpan::sliderValue(7, value(7)));
        for (int k = 0; k < nullpan::kPins; ++k)
            power[k].store(float(fDsp.pinPower(k)), std::memory_order_relaxed);
    }

    nullpan::SurroundPannerProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSurroundPanner)
};

Plugin* createPlugin()
{
    return new PluginSurroundPanner();
}

END_NAMESPACE_DISTRHO
