/*
 * NULL JSFX - Parametric EQ (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * For the display: the mono input and output go into the analyzer ring (EqAnalyzer.hpp); the
 * audio is processed exactly as before.
 */

#include "ParametricEqDSP.hpp"
#include "ParametricEqPlugin.hpp"

START_NAMESPACE_DISTRHO

class PluginParametricEq : public ParametricEqBase
{
public:
    PluginParametricEq()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "ParametricEQ"; }
    const char* getDescription() const override
    {
        return "Five fully parametric bands in series, each one of eight filter shapes, with bypass and solo per band.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '2', 'P', 'E'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        analyzer.sampleRate.store(float(getSampleRate()), std::memory_order_relaxed);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        analyzer.sampleRate.store(float(newSampleRate), std::memory_order_relaxed);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // in chunks, so the input is kept for the spectrum when the host processes in place
        float pre[256];
        for (uint32_t pos = 0; pos < frames; pos += 256)
        {
            const uint32_t n = std::min<uint32_t>(256, frames - pos);
            for (uint32_t i = 0; i < n; ++i)
                pre[i] = (inputs[0][pos + i] + inputs[1][pos + i]) * 0.5f;
            fDsp.process(inputs[0] + pos, inputs[1] + pos, outputs[0] + pos, outputs[1] + pos, n);
            for (uint32_t i = 0; i < n; ++i)
                analyzer.push(pre[i], (outputs[0][pos + i] + outputs[1][pos + i]) * 0.5f);
        }
    }

private:
    void update()
    {
        nullpeq::BandParams p[nullpeq::kBands];
        nullpeq::bandParams([this](uint32_t i) { return double(value(i)); }, p);
        fDsp.setParams(p);
    }

    nullpeq::ParametricEqProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginParametricEq)
};

Plugin* createPlugin()
{
    return new PluginParametricEq();
}

END_NAMESPACE_DISTRHO
