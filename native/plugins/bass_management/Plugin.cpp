/*
 * NULL JSFX - Bass Management (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "BassManagementPlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginBassManagement : public BassManagementBase
{
public:
    PluginBassManagement()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "BassManagement"; }
    const char* getDescription() const override
    {
        return "Plays a 5.1, 7.1 or 7.1.4 bed like a monitoring system with a subwoofer: the mains' bass crossed "
               "over (Linkwitz-Riley) to the sub with the LFE.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'M', 'g'); }

    // the twelve pins, named as the JSFX's
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        port.name = nullbm::kPinNames[index];
        port.symbol = String(input ? "in_" : "out_") + String(nullbm::kPinNames[index]).toLower();
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
        double in[12], out[12];
        float pk[12] {};
        for (uint32_t i = 0; i < frames; ++i)
        {
            for (int k = 0; k < 12; ++k)
                in[k] = inputs[k][i];
            fDsp.processSample(in, out);
            for (int k = 0; k < 12; ++k)
            {
                outputs[k][i] = float(out[k]);
                pk[k] = std::max(pk[k], float(std::fabs(out[k])));
            }
        }
        for (int k = 0; k < 12; ++k)
            if (pk[k] > peak[k].load(std::memory_order_relaxed))
                peak[k].store(pk[k], std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullbm::sliderValue(0, value(0)), nullbm::sliderValue(1, value(1)),
                       nullbm::sliderValue(2, value(2)), nullbm::sliderValue(3, value(3)),
                       nullbm::sliderValue(4, value(4)), nullbm::sliderValue(5, value(5)),
                       nullbm::sliderValue(6, value(6)));
    }

    nullbm::BassManagementProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBassManagement)
};

Plugin* createPlugin()
{
    return new PluginBassManagement();
}

END_NAMESPACE_DISTRHO
