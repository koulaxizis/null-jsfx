/*
 * NULL JSFX - Hit Layer (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display reads three taps, as hit_layer_gfx.jsfx reads its display-only lines: inLevel =
 * the detector's fast envelope, outLevel = the layer's peak (both with a 300 ms release), and
 * extra = 1 when a hit started since the interface last looked (it lights the HIT lamp).
 */

#include "NullPlugin.hpp"
#include "HitLayerDSP.hpp"
#include "HitLayerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginHitLayer : public NullMultiPlugin
{
public:
    PluginHitLayer()
        : NullMultiPlugin(nullhitlayer::kSliders, nullhitlayer::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Hit Layer"; }
    const char* getDescription() const override
    {
        return "Hit layer: a transient detector adds a tuned tone with a pitch drop and a burst of coloured noise under "
               "every hit, to reinforce kicks, snares and toms.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'H', 'L', 'y'); }

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
        bool hit = false;
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            fIn.process(float(fDsp.detector()));
            fLayer.process(float(std::fmax(std::fabs(fDsp.layerL()), std::fabs(fDsp.layerR()))));
            hit = hit || fDsp.hit();
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fLayer.value, std::memory_order_relaxed);
        if (hit)
            taps.extra.store(1.0f, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fLayer.setup(sr);
    }

    void update()
    {
        double s[nullhitlayer::kNumSliders];
        for (uint32_t i = 0; i < nullhitlayer::kNumSliders; ++i)
            s[i] = value(i);
        // the 0.1 steps as the JSFX holds them
        s[0] = std::round(s[0] * 10.0) / 10.0;
        s[4] = std::round(s[4] * 10.0) / 10.0;
        s[11] = std::round(s[11] * 10.0) / 10.0;
        fDsp.setParams(s);
    }

    nullhitlayer::HitLayerProcessor fDsp;
    nulljsfx::PeakFollower fIn, fLayer;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHitLayer)
};

Plugin* createPlugin()
{
    return new PluginHitLayer();
}

END_NAMESPACE_DISTRHO
