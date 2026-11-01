/*
 * NULL JSFX - Transient Split (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Two stereo buses out, like the JSFX pins: channels 1/2 (Balance: the mix; Split: the transient
 * part) and channels 3/4 (Split: the tonal part, else silence). For the display: inLevel = the
 * input peak, gr = the transient weight (0..1, held for 50 ms), and the scope ring gets the
 * weight (left) and the input peak (right) every 1/100 s (2.6 s of history on screen).
 */

#include "NullPlugin.hpp"
#include "TransientSplitDSP.hpp"
#include "TransientSplitSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginTransientSplit : public NullMultiPlugin
{
public:
    PluginTransientSplit()
        : NullMultiPlugin(nulltransientsplit::kSliders, nulltransientsplit::kNumSliders)
    {
        setup(getSampleRate());
        update();
        fDsp.reset();
    }

protected:
    const char* getLabel() const override { return "TransientSplit"; }
    const char* getDescription() const override
    {
        return "Splits the signal into its transient and tonal parts without frequency bands: balance them, or "
               "send the transients to channels 1/2 and the tonal part to channels 3/4.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'T', 'S', 'p'); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        static const char* const kSide[] = { "Left", "Right" };
        port.groupId = index / 2;
        if (input || index < 2)
            port.name = kSide[index % 2];
        else
            port.name = String("Tonal ") + kSide[index % 2];
        port.symbol = String(input ? "in" : "out") + String(int(index + 1));
    }

    void initPortGroup(uint32_t groupId, PortGroup& group) override
    {
        group.name = groupId == 0 ? "Main / Transient" : "Tonal";
        group.symbol = groupId == 0 ? "main" : "tonal";
    }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        setup(getSampleRate());
        fDsp.reset();
    }

    void sampleRateChanged(double newSampleRate) override { setup(newSampleRate); }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        double o[4];
        for (uint32_t i = 0; i < frames; ++i)
        {
            const float il = inputs[0][i], ir = inputs[1][i];
            fDsp.processSample(il, ir, o);
            for (int c = 0; c < 4; ++c)
                outputs[c][i] = float(o[c]);
            const float k = std::fmax(std::fabs(il), std::fabs(ir));
            fIn.process(k);
            const float w = float(fDsp.weight());
            fW = w > fW ? w : fW * fWRel;
            fHistW = std::fmax(fHistW, w);
            fHistK = std::fmax(fHistK, k);
            if (++fHistN >= fHistLen)
            {
                taps.push(fHistW, fHistK);
                fHistW = fHistK = 0.0f;
                fHistN = 0;
            }
        }
        taps.inLevel.store(fIn.value, std::memory_order_relaxed);
        taps.gr.store(fW, std::memory_order_relaxed);
    }

private:
    void setup(double sr)
    {
        fDsp.setSampleRate(sr);
        fIn.setup(sr);
        fWRel = float(std::exp(-1.0 / (0.05 * (sr > 0.0 ? sr : 48000.0))));
        fHistLen = std::max<uint32_t>(1, uint32_t((sr > 0.0 ? sr : 48000.0) / 100.0));
    }

    void update()
    {
        fDsp.setParams(nulltransientsplit::paramsFrom(
            [this](uint32_t i) { return nulltransientsplit::sliderValue(i, value(i)); }));
    }

    nulltransientsplit::TransientSplitProcessor fDsp;
    nulljsfx::PeakFollower fIn;
    float fW = 0.0f, fWRel = 0.9995f, fHistW = 0.0f, fHistK = 0.0f;
    uint32_t fHistN = 0, fHistLen = 240;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTransientSplit)
};

Plugin* createPlugin()
{
    return new PluginTransientSplit();
}

END_NAMESPACE_DISTRHO
