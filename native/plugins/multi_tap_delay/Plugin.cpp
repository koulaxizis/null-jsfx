/*
 * NULL JSFX - Multi-Tap Delay (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in multi_tap_delay_gfx.jsfx): inLevel = the input peak, outLevel = the
 * output peak.
 */

#include "NullPlugin.hpp"
#include "MultiTapDSP.hpp"
#include "MultiTapSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginMultiTapDelay : public NullMultiPlugin
{
public:
    PluginMultiTapDelay()
        : NullMultiPlugin(nullmultitap::kSliders, nullmultitap::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "MultiTapDelay"; }
    const char* getDescription() const override
    {
        return "Up to eight echoes in rhythmic patterns (even, dotted, triplet, swing, speed up, slow down, scatter) at a time or a synced note, with decay, stereo spread, feedback, low and high cut and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'T', 'D'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.beginBlock(tempo);
        for (uint32_t i = 0; i < frames; ++i)
        {
            const float il = inputs[0][i], ir = inputs[1][i];
            double l = il, r = ir;
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            fIn.process(std::fmax(std::fabs(il), std::fabs(ir)));
            fOut.process(std::fmax(std::fabs(float(l)), std::fabs(float(r))));
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fOut.setup(sr);
    }

    void update()
    {
        fDsp.setParams(nullmultitap::paramsFrom([this](uint32_t i) { return nullmultitap::sliderValue(i, value(i)); }));
    }

    nullmultitap::MultiTapProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMultiTapDelay)
};

Plugin* createPlugin()
{
    return new PluginMultiTapDelay();
}

END_NAMESPACE_DISTRHO
