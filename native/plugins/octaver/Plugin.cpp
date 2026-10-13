/*
 * NULL JSFX - Octaver (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "OctaverDSP.hpp"
#include "OctaverSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginOctaver : public OctaverBase
{
public:
    PluginOctaver()
    {
        reset(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "Octaver"; }
    const char* getDescription() const override
    {
        return "Analog-style octave divider for bass and guitar: one and two octaves down plus an octave up, zero latency.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'O', 'c', 't'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        reset(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        reset(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // display only, as in octaver_gfx.jsfx: peak levels per voice and the input period
        // from the flip-flop (it toggles once per input cycle)
        const double sr = fDsp.sampleRate();
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames,
                     [&](const nulloctaver::Probe& p, double l, double r) {
                         peak(fIn, std::fabs(p.in));
                         peak(fM1, std::fabs(p.oct1));
                         peak(fM2, std::fabs(p.oct2));
                         peak(fMu, std::fabs(p.up));
                         peak(fOut, std::max(std::fabs(l), std::fabs(r)));
                         fCnt += 1.0;
                         if (p.ff1 != fFf)
                         {
                             fFf = p.ff1;
                             fHz = p.env > 0.001 && fCnt > 0.0 ? sr / fCnt : 0.0;
                             fCnt = 0.0;
                         }
                         if (fCnt > sr * 0.1)
                             fHz = 0.0;
                         taps.push(float(l), float(r));
                     });
        view.in.store(float(fIn), std::memory_order_relaxed);
        view.oct1.store(float(fM1), std::memory_order_relaxed);
        view.oct2.store(float(fM2), std::memory_order_relaxed);
        view.up.store(float(fMu), std::memory_order_relaxed);
        view.out.store(float(fOut), std::memory_order_relaxed);
        view.hz.store(float(fHz), std::memory_order_relaxed);
    }

private:
    void reset(double sr)
    {
        fDsp.setSampleRate(sr);
        fRel = std::exp(-1.0 / (0.3 * sr));
        fIn = fM1 = fM2 = fMu = fOut = 0.0;
        fFf = 1.0;
        fCnt = 0.0;
        fHz = 0.0;
    }

    void peak(double& m, double v) const
    {
        m = v > m ? v : m * fRel;
        if (m < 1e-15)
            m = 0.0;
    }

    void update()
    {
        fDsp.setParams(value(0), value(1), value(2), value(3), value(4), value(5));
    }

    nulloctaver::OctaverProcessor fDsp;
    double fRel = 0.9999, fIn = 0, fM1 = 0, fM2 = 0, fMu = 0, fOut = 0, fFf = 1, fCnt = 0, fHz = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginOctaver)
};

Plugin* createPlugin()
{
    return new PluginOctaver();
}

END_NAMESPACE_DISTRHO
