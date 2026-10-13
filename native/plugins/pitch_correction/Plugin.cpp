/*
 * NULL JSFX - Pitch Correction (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display shows a 3 s pitch history as in pitch_correction_gfx.jsfx: one entry per 10 ms,
 * pushed into the Taps ring (scopeL = the detected pitch, scopeR = the corrected pitch, as MIDI
 * notes, 0 = unpitched). inLevel = the detected pitch, outLevel = the target note, gr = the
 * shift applied now (semitones, Transpose included), extra = 1 while the input is pitched.
 */

#include "NullPlugin.hpp"
#include "PitchCorrectionDSP.hpp"
#include "PitchCorrectionSliders.hpp"

#include <algorithm>

START_NAMESPACE_DISTRHO

class PluginPitchCorrection : public NullMultiPlugin
{
public:
    PluginPitchCorrection()
        : NullMultiPlugin(nullpitchcorrection::kSliders, nullpitchcorrection::kNumSliders)
    {
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "PitchCorrection"; }
    const char* getDescription() const override
    {
        return "Vocal pitch correction: snaps a monophonic voice to Key + Scale at Retune Speed (0 = hard tune), with Humanize, Transpose and Mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'P', 'C', 'r'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
            if (++fN >= fChunk)
            {
                const bool v = fDsp.voiced();
                taps.push(v ? float(fDsp.detected()) : 0.0f, v ? float(fDsp.detected() + fDsp.shift()) : 0.0f);
                fN = 0;
            }
        }
        taps.inLevel.store(float(fDsp.detected()), std::memory_order_relaxed);
        taps.outLevel.store(float(fDsp.target()), std::memory_order_relaxed);
        taps.gr.store(float(fDsp.totalShift()), std::memory_order_relaxed);
        taps.extra.store(fDsp.voiced() ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        setLatency(fDsp.latency()); // pdc_delay = lat
        fChunk = std::max<uint32_t>(1, uint32_t(std::floor(sr * 0.01)));
        fN = 0;
        update();
    }

    void update()
    {
        fDsp.setParams(int(std::lround(value(0))), int(std::lround(value(1))), value(2), value(3),
                       double(std::lround(value(4))), value(5), value(6));
    }

    nullpitchcorrection::PitchCorrectionProcessor fDsp;
    uint32_t fChunk = 1, fN = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPitchCorrection)
};

Plugin* createPlugin()
{
    return new PluginPitchCorrection();
}

END_NAMESPACE_DISTRHO
