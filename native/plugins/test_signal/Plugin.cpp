/*
 * NULL JSFX - Test Signal (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in test_signal_gfx.jsfx): the output in the scope, outLevel = the
 * output peak, aux[0] = the frequency now (Hz).
 */

#include "NullPlugin.hpp"
#include "TestSignalDSP.hpp"
#include "TestSignalSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTestSignal : public NullMultiPlugin
{
public:
    PluginTestSignal()
        : NullMultiPlugin(nulltestsignal::kSliders, nulltestsignal::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "TestSignal"; }
    const char* getDescription() const override
    {
        return "Test tones at a known level: sine, logarithmic sweep, pink and white noise and polarity pulses, on both sides or one, for calibrating speakers and checking the wiring.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 's', 'g'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            const float ol = float(l), orr = float(r);
            outputs[0][i] = ol;
            outputs[1][i] = orr;
            taps.push(ol, orr);
            fOut.process(std::fmax(std::fabs(ol), std::fabs(orr)));
        }
        taps.outLevel.store(fOut.value, std::memory_order_relaxed);
        taps.aux[0].store(float(fDsp.currentFrequency()), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fOut.setup(sr);
    }

    void update()
    {
        fDsp.setParams(nulltestsignal::paramsFrom([this](uint32_t i) { return nulltestsignal::sliderValue(i, value(i)); }));
    }

    nulltestsignal::TestSignalProcessor fDsp;
    nulljsfx::PeakFollower fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTestSignal)
};

Plugin* createPlugin()
{
    return new PluginTestSignal();
}

END_NAMESPACE_DISTRHO
