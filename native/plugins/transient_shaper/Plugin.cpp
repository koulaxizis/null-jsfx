/*
 * NULL JSFX - Transient Shaper (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 1.8 s history as in transient_shaper_gfx.jsfx: one entry per 7 ms chunk,
 * pushed into the Taps ring (scopeL = the chunk's output peak, scopeR = its gain after Mix
 * furthest from 0 dB). inLevel / outLevel = input / output peak meters, extra = the transient
 * detector (0..1) with a 300 ms peak hold.
 */

#include "NullPlugin.hpp"
#include "TransientShaperDSP.hpp"
#include "TransientShaperSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTransientShaper : public NullMultiPlugin
{
public:
    PluginTransientShaper()
        : NullMultiPlugin(nulltransient::kSliders, nulltransient::kNumSliders)
    {
        setup(getSampleRate());
        update();
        for (uint32_t i = 0; i < nulljsfx::Taps::kScopeLen; ++i)
            taps.scopeR[i] = 1.0f;
    }

protected:
    const char* getLabel() const override { return "Transient Shaper"; }
    const char* getDescription() const override
    {
        return "Transient shaper: boosts or cuts the attack and the sustain of each hit, with three detector speeds, a focus "
               "band, a dry/wet mix, output gain and an output soft clip.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 't', 's'); }

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
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            const double x = fDsp.input();
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            const double lv = std::fmax(std::fabs(l), std::fabs(r));
            const double g = fDsp.gain();
            fHit.process(float(fDsp.transient()));
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
        taps.extra.store(fHit.value, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
        fHit.setup(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.007)));
        fN = 0;
        fHl = 0.0;
        fGh = 0.0;
        fGl = 1000.0;
    }

    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), int(std::lround(value(3))), value(4), value(5), value(6) >= 0.5f,
                       value(7));
    }

    nulltransient::TransientProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut, fHit;
    uint32_t fChunk = 1, fN = 0;
    double fHl = 0.0, fGh = 0.0, fGl = 1000.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTransientShaper)
};

Plugin* createPlugin()
{
    return new PluginTransientShaper();
}

END_NAMESPACE_DISTRHO
