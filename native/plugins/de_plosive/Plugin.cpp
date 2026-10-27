/*
 * NULL JSFX - De-Plosive (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reports the 5 ms look-ahead as latency (the JSFX's pdc_delay). The display shows a 1.8 s
 * history as in de_plosive_gfx.jsfx: one entry per 7 ms chunk, pushed into the Taps ring
 * (scopeL = the chunk's delayed low-band peak, scopeR = its lowest low-band gain). inLevel =
 * the low-band peak meter, gr = the cut in dB with a 0.3 s release (the plosive light).
 */

#include "NullPlugin.hpp"
#include "DePlosiveDSP.hpp"
#include "DePlosiveSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDePlosive : public NullMultiPlugin
{
public:
    PluginDePlosive()
        : NullMultiPlugin(nulldeplosive::kSliders, nulldeplosive::kNumSliders)
    {
        setup(getSampleRate());
        update();
        for (uint32_t i = 0; i < nulljsfx::Taps::kScopeLen; ++i)
            taps.scopeR[i] = 1.0f;
    }

protected:
    const char* getLabel() const override { return "DePlosive"; }
    const char* getDescription() const override
    {
        return "Removes the low-frequency bursts of P and B sounds: a crossover and a look-ahead detector turn down only the low band when it jumps.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'P', 'l'); }

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
        fDsp.block();
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            const double lv = fDsp.delayedLowPeak(), gs = fDsp.reductionDb();
            fLow = lv > fLow ? lv : fLow * fRel;
            fGr = gs > fGr ? gs : fGr * fRel;
            fHl = std::fmax(fHl, lv);
            fHg = std::fmin(fHg, fDsp.lowGain());
            if (++fN >= fChunk)
            {
                taps.push(float(fHl), float(fHg));
                fN = 0;
                fHl = 0.0;
                fHg = 1.0;
            }
        }
        taps.inLevel.store(float(fLow), std::memory_order_relaxed);
        taps.gr.store(float(fGr), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nulldeplosive::sliderValue(i, value(i)); }

    void setup(double sr)
    {
        fDsp.init(sr);
        setLatency(uint32_t(fDsp.latency())); // pdc_delay = 5 ms
        fRel = std::exp(-1.0 / (0.3 * sr));
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fHl = 0.0;
        fHg = 1.0;
        fLow = fGr = 0.0;
    }

    void update()
    {
        nulldeplosive::Params p;
        p.sensitivity = v(0);
        p.frequency = v(1);
        p.maxReduction = v(2);
        p.release = v(3);
        p.listen = int(std::lround(v(4)));
        fDsp.setParams(p);
    }

    nulldeplosive::DePlosiveProcessor fDsp;
    double fRel = 0.9999, fLow = 0.0, fGr = 0.0, fHl = 0.0, fHg = 1.0;
    uint32_t fChunk = 1, fN = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDePlosive)
};

Plugin* createPlugin()
{
    return new PluginDePlosive();
}

END_NAMESPACE_DISTRHO
