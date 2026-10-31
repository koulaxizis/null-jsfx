/*
 * NULL JSFX - Speaker Sim (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Taps for the display (as in speaker_sim_gfx.jsfx): inLevel = the input peak, outLevel = the
 * output peak.
 */

#include "NullPlugin.hpp"
#include "SpeakerSimDSP.hpp"
#include "SpeakerSimSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginSpeakerSim : public NullMultiPlugin
{
public:
    PluginSpeakerSim()
        : NullMultiPlugin(nullspeakersim::kSliders, nullspeakersim::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "SpeakerSim"; }
    const char* getDescription() const override
    {
        return "Small and everyday speakers for checking a mix or for effect: phone, laptop, earbuds, TV, radio, telephone, megaphone and car, with drive, mix and output.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'p', 'k'); }

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
        fDsp.setParams(nullspeakersim::paramsFrom([this](uint32_t i) { return nullspeakersim::sliderValue(i, value(i)); }));
    }

    nullspeakersim::SpeakerSimProcessor fDsp;
    nulljsfx::PeakFollower fIn, fOut;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSpeakerSim)
};

Plugin* createPlugin()
{
    return new PluginSpeakerSim();
}

END_NAMESPACE_DISTRHO
