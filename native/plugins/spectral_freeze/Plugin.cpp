/*
 * NULL JSFX - Spectral Freeze (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "SpectralFreezeDSP.hpp"
#include "SpectralFreezeSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSpectralFreeze : public NullMultiPlugin
{
public:
    PluginSpectralFreeze()
        : NullMultiPlugin(nullfreeze::kSliders, nullfreeze::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SpectralFreeze"; }
    const char* getDescription() const override
    {
        return "Holds the spectrum of the moment Freeze is switched on as an endless drone, with smear and fade.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '9', 'S', 'f'); }

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
        // the display: the held magnitudes (bins 0..1024) in the scope buffer, frozen and wet
        for (int i = 0; i < nullfreeze::kNB; ++i)
            taps.scopeL[i] = float(fDsp.magnitude(i));
        taps.extra.store(fDsp.frozen() ? 1.0f : 0.0f, std::memory_order_relaxed);
        taps.gr.store(float(fDsp.wet()), std::memory_order_relaxed);
    }

private:
    void update()
    {
        // Level as the JSFX slider holds it (min + k * 0.1 in double)
        const double level = -24.0 + std::round((double(value(3)) + 24.0) / 0.1) * 0.1;
        fDsp.setParams(value(0) >= 0.5f, value(1), value(2), level, value(4));
    }

    nullfreeze::FreezeProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpectralFreeze)
};

Plugin* createPlugin()
{
    return new PluginSpectralFreeze();
}

END_NAMESPACE_DISTRHO
