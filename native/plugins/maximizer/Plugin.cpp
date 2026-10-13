/*
 * NULL JSFX - Maximizer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the look-ahead as latency (the JSFX's pdc_delay). The display shows a 1.8 s history
 * as in maximizer_gfx.jsfx: one entry per 7 ms chunk, pushed into the Taps ring (scopeL = the
 * chunk's output peak, scopeR = its lowest gain, the lower channel when unlinked). inLevel / outLevel = input / output peak
 * meters, gr = the gain reduction now in dB.
 */

#include "NullPlugin.hpp"
#include "MaximizerDSP.hpp"
#include "MaximizerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginMaximizer : public NullMultiPlugin
{
public:
    PluginMaximizer()
        : NullMultiPlugin(nullmaximizer::kSliders, nullmaximizer::kNumSliders)
    {
        setup(getSampleRate());
        update();
        for (uint32_t i = 0; i < nulljsfx::Taps::kScopeLen; ++i)
            taps.scopeR[i] = 1.0f;
    }

protected:
    const char* getLabel() const override { return "Maximizer"; }
    const char* getDescription() const override
    {
        return "Loudness maximizer: Input Gain into a look-ahead limiter that holds the peaks at Ceiling, with stereo link and an output trim.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'm', 'x'); }

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
        double g = 1.0;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            const double x = std::fmax(std::fabs(l), std::fabs(r));
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            const double lv = std::fmax(std::fabs(l), std::fabs(r));
            g = fDsp.gain();
            fIn.process(float(x));
            fOut.process(float(lv));
            fHl = std::fmax(fHl, lv);
            fGh = std::fmax(fGh, g);
            fGl = std::fmin(fGl, g);
            if (++fN >= fChunk)
            {
                taps.push(float(fHl), float(fGh * fGl > 1.0 ? fGh : fGl));
                fN = 0;
                fHl = 0.0;
                fGh = 0.0;
                fGl = 1000.0;
            }
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(g, 0.0000001))), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fHl = 0.0;
        fGh = 0.0;
        fGl = 1000.0;
    }

    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4), value(5) >= 0.5f);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay = look
    }

    nullmaximizer::MaximizerProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;
    uint32_t fChunk = 1, fN = 0;
    double fHl = 0.0, fGh = 0.0, fGl = 1000.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMaximizer)
};

Plugin* createPlugin()
{
    return new PluginMaximizer();
}

END_NAMESPACE_DISTRHO
