/*
 * NULL JSFX - Band Splitter (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "BandSplitterDSP.hpp"
#include "BandSplitterSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginBandSplitter : public NullMultiPlugin
{
public:
    PluginBandSplitter()
        : NullMultiPlugin(nullbandsplit::kSliders, nullbandsplit::kNumSliders)
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        update();
    }

protected:
    const char* getLabel() const override { return "BandSplitter"; }
    const char* getDescription() const override
    {
        return "Linkwitz-Riley crossover that splits the input into 2-4 bands on channels 1-8 (and merges them back), "
               "for multiband chains with any plugin.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'B', 'S', 'p'); }

    // four stereo buses each way, like the JSFX pins: channels 1/2 the input (or band 1), then
    // bands 2, 3 and 4
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        static const char* const kSide[] = { "Left", "Right" };
        const uint32_t band = index / 2;
        port.groupId = band;
        if (input && band == 0)
            port.name = kSide[index % 2];
        else
            port.name = String("Band ") + String(int(band + 1)) + " " + kSide[index % 2];
        port.symbol = String(input ? "in" : "out") + String(int(index + 1));
    }

    void initPortGroup(uint32_t groupId, PortGroup& group) override
    {
        if (groupId == 0)
        {
            group.name = "Main / Band 1";
            group.symbol = "band1";
            return;
        }
        group.name = String("Band ") + String(int(groupId + 1));
        group.symbol = String("band") + String(int(groupId + 1));
    }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override
    {
        fDsp.setSampleRate(getSampleRate());
        fDsp.reset();
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        fDsp.reset();
        update();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        fDsp.process(inputs, outputs, frames);
    }

private:
    void update()
    {
        nullbandsplit::Params p;
        p.merge = value(0) >= 0.5f;
        p.bands = int(std::lround(value(1)));
        for (int k = 0; k < 3; ++k)
            p.xover[k] = std::round(double(value(2 + uint32_t(k))));
        p.listen = int(std::lround(value(5)));
        fDsp.setParams(p);
    }

    nullbandsplit::BandSplitterProcessor fDsp;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBandSplitter)
};

Plugin* createPlugin()
{
    return new PluginBandSplitter();
}

END_NAMESPACE_DISTRHO
