/*
 * NULL JSFX - Cabinet Simulator (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "CabinetSimulatorDSP.hpp"
#include "CabinetSimulatorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginCabinetSimulator : public NullMultiPlugin
{
public:
    PluginCabinetSimulator()
        : NullMultiPlugin(nullcab::kSliders, nullcab::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "CabinetSimulator"; }
    const char* getDescription() const override
    {
        return "Miked speaker cabinet without impulse files: eight cabinets, two blended mic positions, low cut, mix and output.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '2', 'C', 'B'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { fDsp.setSampleRate(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void update()
    {
        fDsp.setParams(int(std::lround(value(0))), value(1), value(2), value(3), value(4), value(5), value(6));
    }

    nullcab::CabinetSimulatorProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginCabinetSimulator)
};

Plugin* createPlugin()
{
    return new PluginCabinetSimulator();
}

END_NAMESPACE_DISTRHO
