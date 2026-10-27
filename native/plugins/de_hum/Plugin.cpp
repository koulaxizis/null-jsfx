/*
 * NULL JSFX - De-Hum (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "DeHumDSP.hpp"
#include "DeHumSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDeHum : public NullMultiPlugin
{
public:
    PluginDeHum()
        : NullMultiPlugin(nulldehum::kSliders, nulldehum::kNumSliders)
    {
        fDsp.init(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "DeHum"; }
    const char* getDescription() const override
    {
        return "Removes mains hum: narrow notches at 50 or 60 Hz and its harmonics, with an optional low cut for rumble.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'H', 'm'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.init(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.init(newSampleRate);
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    double v(uint32_t i) const { return nulldehum::sliderValue(i, value(i)); }

    void update()
    {
        nulldehum::Params p;
        p.mains = int(std::lround(v(0)));
        p.harmonics = v(1);
        p.q = v(2);
        p.depth = v(3);
        p.fine = v(4);
        p.lowCut = int(std::lround(v(5)));
        fDsp.setParams(p);
    }

    nulldehum::DeHumProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeHum)
};

Plugin* createPlugin()
{
    return new PluginDeHum();
}

END_NAMESPACE_DISTRHO
