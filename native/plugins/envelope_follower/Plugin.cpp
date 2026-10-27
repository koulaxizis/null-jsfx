/*
 * NULL JSFX - Envelope Follower (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 2.6 s history as in envelope_follower_gfx.jsfx: one entry per 10 ms
 * chunk, pushed into the Taps ring (scopeL = the chunk's input peak, scopeR = the envelope
 * position 0..1 at its end). inLevel = input peak meter, gr = the envelope position now (0..1,
 * not a gain reduction here), extra = the filter cutoff in Hz.
 */

#include "NullPlugin.hpp"
#include "EnvelopeFollowerDSP.hpp"
#include "EnvelopeFollowerSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginEnvelopeFollower : public NullMultiPlugin
{
public:
    PluginEnvelopeFollower()
        : NullMultiPlugin(nullenvfollower::kSliders, nullenvfollower::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Envelope Follower"; }
    const char* getDescription() const override
    {
        return "Auto-wah and touch filter: the input's loudness moves a low-, band- or high-pass filter between two frequencies, or the volume.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '1', 'e', 'f'); }

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
            const double x = std::fmax(std::fabs(l), std::fabs(r));
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);

            fIn.process(float(x));
            fHl = std::fmax(fHl, x);
            if (++fN >= fChunk)
            {
                taps.push(float(fHl), float(fDsp.position()));
                fN = 0;
                fHl = 0.0;
            }
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.gr.store(float(fDsp.position()), std::memory_order_relaxed);
        taps.extra.store(float(fDsp.cutoff()), std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.01)));
        fN = 0;
        fHl = 0.0;
    }

    void update()
    {
        fDsp.setParams(int(std::lround(value(0))), value(1), value(2), value(3), value(4), value(5), value(6),
                       value(7) >= 0.5f, value(8));
    }

    nullenvfollower::EnvelopeFollowerProcessor fDsp;
    nulljsfx::PeakFollower fIn;
    uint32_t fChunk = 1, fN = 0;
    double fHl = 0.0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEnvelopeFollower)
};

Plugin* createPlugin()
{
    return new PluginEnvelopeFollower();
}

END_NAMESPACE_DISTRHO
