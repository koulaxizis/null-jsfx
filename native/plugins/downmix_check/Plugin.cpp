/*
 * NULL JSFX - Downmix Check (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "DownmixCheckPlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginDownmixCheck : public DownmixCheckBase
{
public:
    PluginDownmixCheck() { update(); }

protected:
    const char* getLabel() const override { return "DownmixCheck"; }
    const char* getDescription() const override
    {
        return "Hears a 5.1, 7.1 or 7.1.4 bed as it is, folded to 5.1, to stereo Lo/Ro (ITU-R BS.775) or to mono, "
               "with any one channel soloed.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'm', 'x'); }

    // the twelve pins, named as the JSFX's
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        port.name = nulldmx::kPinNames[index];
        port.symbol = String(input ? "in_" : "out_") + String(nulldmx::kPinNames[index]).toLower();
    }

    void valueChanged(uint32_t, float) override { update(); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        double in[12], out[12], ip[12] {}, op[12] {};
        for (uint32_t i = 0; i < frames; ++i)
        {
            for (int k = 0; k < 12; ++k)
                in[k] = inputs[k][i];
            fDsp.processSample(in, out, ip, op);
            for (int k = 0; k < 12; ++k)
                outputs[k][i] = float(out[k]);
        }
        for (int k = 0; k < 12; ++k)
        {
            keepPeak(inPeak[k], float(ip[k]));
            keepPeak(outPeak[k], float(op[k]));
        }
        keepPeak(foldPeak, float(fDsp.takePeak()));
    }

private:
    void update()
    {
        fDsp.setParams(nulldmx::sliderValue(0, value(0)), nulldmx::sliderValue(1, value(1)),
                       nulldmx::sliderValue(2, value(2)), nulldmx::sliderValue(3, value(3)),
                       nulldmx::sliderValue(4, value(4)), nulldmx::sliderValue(5, value(5)),
                       nulldmx::sliderValue(6, value(6)), nulldmx::sliderValue(7, value(7)));
    }

    nulldmx::DownmixCheckProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDownmixCheck)
};

Plugin* createPlugin()
{
    return new PluginDownmixCheck();
}

END_NAMESPACE_DISTRHO
