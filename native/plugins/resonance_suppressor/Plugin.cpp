/*
 * NULL JSFX - Resonance Suppressor (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "ResonanceSuppressorDSP.hpp"
#include "ResonanceSuppressorSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginResonanceSuppressor : public NullMultiPlugin
{
public:
    PluginResonanceSuppressor()
        : NullMultiPlugin(nullrsp::kSliders, nullrsp::kNumSliders)
    {
        for (int k = 0; k < nullrsp::kNB; ++k)
            taps.scopeL[k] = -120.0f; // no level yet
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "ResonanceSuppressor"; }
    const char* getDescription() const override
    {
        return "Dynamic resonance suppression: turns down the frequencies that stick out of the spectral envelope, moment by moment.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 'S', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // the display: bin levels (dB) in scopeL, reductions (dB) in scopeR, bins 0..1024
        for (int k = 0; k < nullrsp::kNB; ++k)
        {
            taps.scopeL[k] = float(fDsp.level(k));
            taps.scopeR[k] = float(fDsp.reduction(k));
        }
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        setLatency(uint32_t(nullrsp::ResonanceSuppressorProcessor::latency())); // pdc_delay = N - 1
        update();
    }

    void update()
    {
        // Depth as the JSFX slider holds it (min + k * 0.1 in double)
        const double depth = std::round(double(value(0)) / 0.1) * 0.1;
        fDsp.setParams(depth, value(1), value(2), value(3), value(4), value(5), value(6), value(7) >= 0.5f,
                       value(8) >= 0.5f, value(9));
    }

    nullrsp::ResonanceSuppressorProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginResonanceSuppressor)
};

Plugin* createPlugin()
{
    return new PluginResonanceSuppressor();
}

END_NAMESPACE_DISTRHO
