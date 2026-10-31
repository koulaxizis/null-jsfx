/*
 * NULL JSFX - Console Channel and Console Buss (VST3/CLAP), the DSP side both plugins share
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in console_*_gfx.jsfx): inLevel / outLevel = input / output peak.
 */

#pragma once

#include "NullPlugin.hpp"
#include "ConsoleDSP.hpp"
#include "ConsoleSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginConsoleBase : public NullMultiPlugin
{
public:
    explicit PluginConsoleBase(bool buss)
        : NullMultiPlugin(nullconsole::kSliders, nullconsole::kNumSliders), fDsp(buss)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
            fIn.process(std::fmax(std::fabs(inputs[0][i]), std::fabs(inputs[1][i])));
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        for (uint32_t i = 0; i < frames; ++i)
            fOut.process(std::fmax(std::fabs(outputs[0][i]), std::fabs(outputs[1][i])));
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nullconsole::sliderValue(i, value(i)); }

    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    void update() { fDsp.setParams(int(v(0)), v(1), v(2)); }

    nullconsole::ConsoleProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;
};

END_NAMESPACE_DISTRHO
