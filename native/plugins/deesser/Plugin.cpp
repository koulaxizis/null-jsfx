/*
 * NULL JSFX - DeEsser (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in deesser_gfx.jsfx): inLevel = the sibilance-band envelope,
 * outLevel = output peak meter, extra = the band's current cut (0..1).
 */

#include "NullPlugin.hpp"
#include "DeEsserDSP.hpp"
#include "DeEsserSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginDeEsser : public NullMultiPlugin
{
public:
    PluginDeEsser()
        : NullMultiPlugin(nulldeesser::kSliders, nulldeesser::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "DeEsser"; }
    const char* getDescription() const override
    {
        return "De-esser: the band around Target (Split) or the whole signal (Wide) is turned down, by up to Reduction "
               "capped by Range, when the sibilance passes an automatic or manual threshold; Listen plays what is removed.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'd', 's'); }

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
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
        }
        taps.inLevel.store(float(fDsp.envelope()), std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.extra.store(float(fDsp.cut()), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fOut.setup(sr);
    }

    void update() { fDsp.setParams(value(0), value(1), value(2) >= 0.5f, value(3) >= 0.5f, value(4), value(5), value(6) >= 0.5f); }

    nulldeesser::DeEsserProcessor fDsp;
    nulljsfx::PeakFollower fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDeEsser)
};

Plugin* createPlugin()
{
    return new PluginDeEsser();
}

END_NAMESPACE_DISTRHO
