/*
 * NULL JSFX - Tape Echo (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The host tempo (120 when it reports none) drives Tempo Sync; the repeat time in use goes to
 * the display through taps.extra.
 */

#include "NullPlugin.hpp"
#include "TapeEchoDSP.hpp"
#include "TapeEchoSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTapeEcho : public NullMultiPlugin
{
public:
    PluginTapeEcho()
        : NullMultiPlugin(nulltapeecho::kSliders, nulltapeecho::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "TapeEcho"; }
    const char* getDescription() const override
    {
        return "Multi-head tape echo: three playback heads at 1x, 2x and 3x the repeat time share one feedback loop with tape saturation, worn-tape band limit and wow/flutter; bass and treble, a spring reverb in the R modes, tempo sync and freeze.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 'E', 'c'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo);
        taps.extra.store(float(fDsp.repeatMsNow()), std::memory_order_relaxed);
    }

private:
    double v(uint32_t i) const { return nulltapeecho::sliderValue(i, value(i)); }

    void update()
    {
        nulltapeecho::Params p;
        p.time = v(0);
        p.sync = v(1);
        p.heads = v(2);
        p.intensity = v(3);
        p.bass = v(4);
        p.treble = v(5);
        p.wowFlutter = v(6);
        p.saturation = v(7);
        p.spring = v(8);
        p.mix = v(9);
        p.freeze = v(10);
        fDsp.setParams(p);
    }

    nulltapeecho::TapeEchoProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTapeEcho)
};

Plugin* createPlugin()
{
    return new PluginTapeEcho();
}

END_NAMESPACE_DISTRHO
