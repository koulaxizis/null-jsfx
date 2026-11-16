/*
 * NULL JSFX - Spectral Blur (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the 2047-sample FFT delay as latency (the JSFX's pdc_delay).
 */

#include "NullPlugin.hpp"
#include "SpectralBlurDSP.hpp"
#include "SpectralBlurSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSpectralBlur : public NullMultiPlugin
{
public:
    PluginSpectralBlur()
        : NullMultiPlugin(nullblur::kSliders, nullblur::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "SpectralBlur"; }
    const char* getDescription() const override
    {
        return "Blurs the sound in the frequency domain, with a spectral shift, tilt and per-bin gate.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'b', 'l'); }

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
        // the display: the blurred magnitudes (bins 0..1024) in the scope buffer
        for (int i = 0; i < nullblur::kNB; ++i)
            taps.scopeL[i] = float(fDsp.magnitude(i));
    }

private:
    void update()
    {
        double v[nullblur::kNumSliders];
        for (uint32_t i = 0; i < nullblur::kNumSliders; ++i)
            v[i] = jsfxValue(nullblur::kSliders[i], value(i));
        fDsp.setParams(v);
        setLatency(nullblur::BlurProcessor::latency()); // the JSFX's pdc_delay
    }

    nullblur::BlurProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpectralBlur)
};

Plugin* createPlugin()
{
    return new PluginSpectralBlur();
}

END_NAMESPACE_DISTRHO
