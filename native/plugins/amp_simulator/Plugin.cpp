/*
 * NULL JSFX - Amp Simulator (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "AmpSimulatorDSP.hpp"
#include "AmpSimulatorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginAmpSimulator : public NullMultiPlugin
{
public:
    PluginAmpSimulator()
        : NullMultiPlugin(nullamp::kSliders, nullamp::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fIn.setup(getSampleRate());
        fOut.setup(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "AmpSimulator"; }
    const char* getDescription() const override
    {
        return "Guitar and bass amp without cabinet: oversampled tube stages, an interactive tone stack, presence and a sagging power amp.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'A', 'm', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fIn.setup(getSampleRate());
        fOut.setup(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
        fIn.setup(newSampleRate);
        fOut.setup(newSampleRate);
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        // the display's drive dot follows the input peak (read before the output overwrites it)
        for (uint32_t i = 0; i < frames; ++i)
            fIn.process(std::fmax(std::fabs(inputs[0][i]), std::fabs(inputs[1][i])));
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        for (uint32_t i = 0; i < frames; ++i)
            fOut.process(std::fmax(std::fabs(outputs[0][i]), std::fabs(outputs[1][i])));
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
    }

private:
    void update()
    {
        using nullamp::snap;
        fDsp.setParams(int(std::lround(value(0))), snap(value(1), 0.0), snap(value(2), 0.0), snap(value(3), 0.0),
                       snap(value(4), 0.0), snap(value(5), 0.0), snap(value(6), 0.0), value(7) >= 0.5f, value(8),
                       value(9) >= 0.5f, snap(value(10), -24.0), snap(value(11), -90.0));
    }

    nullamp::AmpSimulatorProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAmpSimulator)
};

Plugin* createPlugin()
{
    return new PluginAmpSimulator();
}

END_NAMESPACE_DISTRHO
