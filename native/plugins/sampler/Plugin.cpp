/*
 * NULL JSFX - Sampler (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are slider2..slider21 of the JSFX. The sample is the "sample" state, a file
 * path (the host saves it with the project): read off the audio thread when it is set, handed
 * to the DSP under a lock at the start of a block, like Convolution's IR file. An empty or
 * unreadable path plays the built-in sound, as the JSFX does without a file.
 */

#include "NullPlugin.hpp"
#include "SamplerShared.hpp"
#include "SamplerRead.hpp"
#include "SamplerSliders.hpp"

#include <cstring>
#include <mutex>
#include <string>

START_NAMESPACE_DISTRHO

class PluginSampler : public SamplerPluginBase
{
public:
    PluginSampler()
        : SamplerPluginBase(nullsmp::kSliders, nullsmp::kNumParams)
    {
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Sampler"; }
    const char* getDescription() const override
    {
        return "Plays your WAV or FLAC files over the keyboard: one-shot or chromatic, ADSR, low-pass with "
               "envelope, loop, start offset, reverse, 16 voices.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'S', 'm', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override { init(newSampleRate); }

    void initState(uint32_t index, State& state) override
    {
        if (index != 0)
            return;
        state.hints = kStateIsFilenamePath;
        state.key = "sample";
        state.defaultValue = "";
        state.label = "Sample File";
        state.description = "WAV or FLAC file the sampler plays (none: the built-in sound)";
    }

    // the sample file: read and converted off the audio thread, handed over to run() under the lock
    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "sample") != 0)
            return;
        nullsmp::SampleFile file;
        nullsmp::readSample(value, file, nullsmp::kMaxValues);
        std::vector<double> data(file.data.begin(), file.data.end()); // file_mem's values
        float ov[nullsmp::kOverview] = {};
        const int nch = file.channels;
        if (nch > 0 && data.size() >= size_t(4 * nch))
            nullsmp::makeOverview(data.data(), nch, double(data.size() / size_t(nch)), ov);
        const std::lock_guard<std::mutex> lock(fFileLock);
        fPending.swap(data);
        fPendingCh = nch;
        fPendingRate = file.sampleRate;
        std::memcpy(fPendingOv, ov, sizeof(ov));
        ++fFileId;
    }

    void process(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        // a new file: the JSFX's @slider after slider1 changed
        if (fFileLock.try_lock())
        {
            if (fFileId != fAppliedFileId)
            {
                fDsp.setSample(fPending, fPendingCh, fPendingRate); // the old data goes back to fPending
                fAppliedFileId = fFileId;
                if (fDsp.fileOk())
                    std::memcpy(overview, fPendingOv, sizeof(overview));
                else
                    std::memcpy(overview, fBuiltinOv, sizeof(overview));
                publish();
                update();
            }
            fFileLock.unlock();
        }

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < nullsmp::kMaxEvents; ++i)
        {
            const MidiEvent& ev = midi[i];
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 1 && ev.size <= 3 && d[0] >= 0x80 && d[0] < 0xF0)
                fEvents[n++] = { ev.frame, d[0], uint8_t(ev.size > 1 ? d[1] : 0), uint8_t(ev.size > 2 ? d[2] : 0) };
        }
        fDsp.beginBlock(fEvents, n, taps.request.exchange(0) != 0);
        fDsp.process(outputs[0], outputs[1], frames);

        // where each voice plays, as a part of the file (-1: silent)
        const double nfr = fDsp.frames();
        for (int i = 0; i < nullsmp::kVoices; ++i)
        {
            const nullsmp::Sampler::Voice& v = fDsp.voice(i);
            float x = -1.0f;
            if (v.on && nfr > 0)
                x = float((fDsp.reverse() ? nfr - 1 - v.pos : v.pos) / nfr);
            taps.aux[i].store(x, std::memory_order_relaxed);
        }
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        update();
        // the built-in sound's waveform, shown when no file plays
        const std::vector<double>& b = fDsp.builtin();
        nullsmp::makeOverview(b.data(), 1, double(b.size()), fBuiltinOv);
        if (!fDsp.fileOk())
            std::memcpy(overview, fBuiltinOv, sizeof(overview));
        publish();
    }

    void publish()
    {
        fileOk.store(fDsp.fileOk(), std::memory_order_relaxed);
        channels.store(fDsp.channels(), std::memory_order_relaxed);
        fileRate.store(float(fDsp.fileRate()), std::memory_order_relaxed);
        seconds.store(float(fDsp.frames() / fDsp.fileRate()), std::memory_order_relaxed);
    }

    void update()
    {
        double v[nullsmp::kNumParams];
        for (uint32_t i = 0; i < nullsmp::kNumParams; ++i)
        {
            const SliderInfo& in = nullsmp::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            v[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                  : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                v[i] = std::round(v[i] * 10.0) / 10.0;
        }
        fDsp.setParams(v);
    }

    nullsmp::Sampler fDsp;
    nullsmp::MidiEv fEvents[nullsmp::kMaxEvents];
    float fBuiltinOv[nullsmp::kOverview] = {};

    std::mutex fFileLock;
    std::vector<double> fPending;
    int fPendingCh = 0;
    double fPendingRate = 0.0;
    float fPendingOv[nullsmp::kOverview] = {};
    int fFileId = 0, fAppliedFileId = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSampler)
};

Plugin* createPlugin()
{
    return new PluginSampler();
}

END_NAMESPACE_DISTRHO
