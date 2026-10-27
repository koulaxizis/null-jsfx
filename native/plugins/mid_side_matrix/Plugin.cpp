/*
 * NULL JSFX - Mid/Side Matrix (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "MidSideMatrixDSP.hpp"
#include "MidSideMatrixSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginMidSideMatrix : public NullMultiPlugin
{
public:
    PluginMidSideMatrix()
        : NullMultiPlugin(nullmidside::kSliders, nullmidside::kNumSliders)
    {
        setup(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "MidSideMatrix"; }
    const char* getDescription() const override
    {
        return "Mid/side encoder and decoder with mid and side gain.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '3', 'm', 's'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { setup(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        // display: vectorscope taps of the input (read before the outputs are written)
        for (uint32_t i = 0; i < frames; ++i)
            taps.push(inputs[0][i], inputs[1][i]);
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        // display: peak followers of the two outputs (as in mid_side_matrix_gfx.jsfx)
        for (uint32_t i = 0; i < frames; ++i)
        {
            fPeakL.process(outputs[0][i]);
            fPeakR.process(outputs[1][i]);
        }
        taps.inLevel.store(fPeakL.value, std::memory_order_relaxed);
        taps.outLevel.store(fPeakR.value, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fPeakL.setup(sr);
        fPeakR.setup(sr);
    }

    void update()
    {
        fDsp.setParams(nullmidside::tenth(value(0)), nullmidside::tenth(value(1)), double(std::lround(value(2))));
    }

    nullmidside::MidSideProcessor fDsp;
    nulljsfx::PeakFollower fPeakL, fPeakR;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMidSideMatrix)
};

Plugin* createPlugin()
{
    return new PluginMidSideMatrix();
}

END_NAMESPACE_DISTRHO
