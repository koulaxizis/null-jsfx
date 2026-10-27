/*
 * NULL JSFX - Convolution (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "ConvolutionShared.hpp"
#include "ConvolutionSliders.hpp"
#include "NullMailbox.hpp"

#include <cstring>
#include <string>

START_NAMESPACE_DISTRHO

class PluginConvolution : public ConvolutionPluginBase
{
public:
    PluginConvolution()
        : ConvolutionPluginBase(nullconv::kSliders, nullconv::kNumSliders)
    {
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Convolution"; }
    const char* getDescription() const override
    {
        return "Convolution reverb: five built-in rooms or your own WAV impulse response, with length, "
               "pre-delay, tone and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', '9', 'C', 'v'); }

    void valueChanged(uint32_t, float) override { setParams(); }

    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    // the IR file: read off the audio thread, handed over to run() without a lock
    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "irfile") != 0)
            return;
        nullconv::IrFile* file = new nullconv::IrFile();
        if (value && value[0])
            nullconv::readWav(value, *file);
        fFiles.post(file);
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        // the JSFX's @slider before the block: a new file, then a rebuild if anything changed
        if (nullconv::IrFile* file = fFiles.take())
        {
            fDsp.swapFile(*file, ++fFileId); // the old data goes back in file, freed off the audio thread
            fFiles.giveBack(file);
        }
        fDsp.update();
        if (fDsp.builds() != fShownBuild)
            publish();
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        setLatency(uint32_t(nullconv::ConvolutionProcessor::latency())); // pdc_delay = B
        setParams();
    }

    void setParams()
    {
        // Output as the JSFX slider holds it (min + k * 0.1 in double)
        const double out = -24.0 + std::round((double(value(6)) + 24.0) / 0.1) * 0.1;
        fDsp.setParams(int(std::lround(value(0))), value(1), value(2), value(3), value(4), value(5), out);
    }

    // the IR envelope and length for the display
    void publish()
    {
        const float* env = fDsp.envelope();
        for (int j = 0; j < nullconv::kEnvBins; ++j)
            envelope[j] = env[j];
        fShownBuild = fDsp.builds();
        irSeconds.store(float(fDsp.irLength() / getSampleRate()), std::memory_order_relaxed);
    }

    nullconv::ConvolutionProcessor fDsp;
    nulljsfx::Mailbox<nullconv::IrFile> fFiles;
    int fFileId = 0, fShownBuild = -1;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginConvolution)
};

Plugin* createPlugin()
{
    return new PluginConvolution();
}

END_NAMESPACE_DISTRHO
