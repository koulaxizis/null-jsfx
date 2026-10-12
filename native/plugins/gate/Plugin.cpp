/*
 * NULL JSFX - Gate (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 1.8 s history as in gate_gfx.jsfx: one entry per 7 ms chunk, pushed into
 * the Taps ring (scopeL = the chunk's highest detector level, scopeR = its lowest gate gain).
 * inLevel = detector peak meter, outLevel = output peak meter, gr = gain reduction in dB.
 */

#include "NullPlugin.hpp"
#include "GateDSP.hpp"
#include "GateSliders.hpp"

#include <algorithm>

START_NAMESPACE_DISTRHO

class PluginGate : public NullMultiPlugin
{
public:
    PluginGate()
        : NullMultiPlugin(nullgate::kSliders, nullgate::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Gate"; }
    const char* getDescription() const override
    {
        return "Noise gate: opens above Threshold and closes below it down to Range, with attack, release and a sidechain high-pass.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'g', 't'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        setup(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        setup(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            const double det = fDsp.detector(), g = fDsp.gain();
            fDet.process(float(det));
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
            fDmax = std::fmax(fDmax, det);
            fGmin = std::fmin(fGmin, g);
            if (++fN >= fChunk)
            {
                taps.push(float(fDmax), float(fGmin));
                fN = 0;
                fDmax = 0.0;
                fGmin = 1.0;
            }
        }
        taps.inLevel.store(fDet.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(fDsp.gain(), 0.0000001))), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fDet.setup(sr);
        fOut.setup(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fDmax = 0.0;
        fGmin = 1.0;
    }

    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4));
    }

    nullgate::GateProcessor fDsp;
    nulljsfx::PeakFollower fDet, fOut;
    uint32_t fChunk = 1, fN = 0;
    double fDmax = 0.0, fGmin = 1.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGate)
};

Plugin* createPlugin()
{
    return new PluginGate();
}

END_NAMESPACE_DISTRHO
