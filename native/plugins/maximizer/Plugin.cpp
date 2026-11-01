/*
 * NULL JSFX - Maximizer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the look-ahead as latency (the JSFX's pdc_delay). The display shows a 1.8 s history
 * as in maximizer_gfx.jsfx: one entry per 7 ms chunk, pushed into the Taps ring (scopeL = the
 * chunk's output peak, scopeR = its lowest gain, the lower channel when unlinked). inLevel / outLevel = input / output peak
 * meters, gr = the gain reduction now in dB (Multiband: of the most reduced band), extra = the
 * output's short-term loudness in LUFS (display only, as the GFX JSFX). Learn (Target LUFS):
 * the plugin moves Input Gain and Learn itself as the JSFX writes slider1 / slider16; aux[1] counts
 * those writes for the interface, aux[2] = measuring or not.
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
        return "Loudness maximizer: Input Gain into a look-ahead limiter that holds the peaks at Ceiling, with stereo link, an optional character stage and 3-band mode, an output trim and TPDF dither with optional noise shaping.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'm', 'x'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        setup(newSampleRate);
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.block(); // the JSFX's @block: Learn turned on starts measuring
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
            fLoud.process(l, r);
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
        taps.extra.store(float(fLoud.lufs()), std::memory_order_relaxed);
        taps.gr.store(float(-20.0 * std::log10(std::fmax(g, 0.0000001))), std::memory_order_relaxed);
        if (fDsp.takeLearned())
        {
            // Learn moved Input Gain or set Learn back to Off, as the JSFX's sliderchange does;
            // the interface sees the count go up and tells the host
            setOwnValue(0, float(fDsp.inputDb()));
            setOwnValue(15, fDsp.learnOn() ? 1.0f : 0.0f);
            taps.aux[1].store(taps.aux[1].load(std::memory_order_relaxed) + 1.0f, std::memory_order_release);
        }
        taps.aux[2].store(fDsp.learning() ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
        fLoud.setup(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fHl = 0.0;
        fGh = 0.0;
        fGl = 1000.0;
    }

    void update()
    {
        using nullmaximizer::sliderValue;
        fDsp.setParams(sliderValue(0, value(0)), sliderValue(1, value(1)), sliderValue(2, value(2)), sliderValue(3, value(3)),
                       sliderValue(4, value(4)), value(5) >= 0.5f,
                       int(std::lround(value(6))), value(7) >= 0.5f, int(std::lround(value(8))), value(9) >= 0.5f,
                       std::round(value(10)), std::round(value(11)), value(12) >= 0.5f, value(13) >= 0.5f);
        fDsp.setLearn(sliderValue(14, value(14)), value(15) >= 0.5f);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay = look (+ 6 with True Peak)
    }

    nullmaximizer::MaximizerProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;
    nullmaximizer::ShortTermLoudness fLoud;
    uint32_t fChunk = 1, fN = 0;
    double fHl = 0.0, fGh = 0.0, fGl = 1000.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMaximizer)
};

Plugin* createPlugin()
{
    return new PluginMaximizer();
}

END_NAMESPACE_DISTRHO
