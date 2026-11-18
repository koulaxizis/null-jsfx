/*
 * NULL JSFX - Band Spin (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The display reads the rotors as band_spin_gfx.jsfx reads PH[] and FQ[]: aux[0..3] = each
 * rotor's phase (0..1), aux[4..7] = its speed now (Hz), at the end of each block.
 */

#include "NullPlugin.hpp"
#include "BandSpinDSP.hpp"
#include "BandSpinSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginBandSpin : public NullMultiPlugin
{
public:
    PluginBandSpin()
        : NullMultiPlugin(nullbandspin::kSliders, nullbandspin::kNumSliders)
    {
        update();
        fDsp.setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Band Spin"; }
    const char* getDescription() const override
    {
        return "Band spin: four bands, each on its own rotor with its own speed, swinging the pan, the volume and a "
               "Doppler pitch; the rotors can speed up with each band's level.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'd', 'S'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        update();
        fDsp.setSampleRate(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        update();
        fDsp.setSampleRate(newSampleRate);
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            double l = inputs[0][i], r = inputs[1][i];
            fDsp.processSample(l, r);
            outputs[0][i] = float(l);
            outputs[1][i] = float(r);
        }
        for (int b = 0; b < 4; ++b)
        {
            taps.aux[b].store(float(fDsp.phase(b)), std::memory_order_relaxed);
            taps.aux[4 + b].store(float(fDsp.speed(b)), std::memory_order_relaxed);
        }
    }

private:
    void update()
    {
        float v[nullbandspin::kNumSliders];
        for (uint32_t i = 0; i < nullbandspin::kNumSliders; ++i)
            v[i] = value(i);
        double s[nullbandspin::kNumSliders];
        nullbandspin::readSliders(v, s);
        fDsp.setParams(s);
    }

    nullbandspin::BandSpinProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBandSpin)
};

Plugin* createPlugin()
{
    return new PluginBandSpin();
}

END_NAMESPACE_DISTRHO
