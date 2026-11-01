/*
 * NULL JSFX - Granular (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are slider2..slider14 of the JSFX. The sound is the "sample" state, a file
 * path (the host saves it with the project): read off the audio thread when it is set, handed
 * to the DSP without a lock at the start of a block, as in the Sampler. An empty or unreadable
 * path plays the built-in sound, as the JSFX does without a file.
 */

#include "GranularSliders.hpp"
#include "../sampler/SamplerRead.hpp"
#include "NullMailbox.hpp"

#include <cstring>
#include <string>

START_NAMESPACE_DISTRHO

class PluginGranular : public GranularPluginBase
{
public:
    PluginGranular()
        : GranularPluginBase(nullgrn::kSliders, nullgrn::kNumParams)
    {
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Granular"; }
    const char* getDescription() const override
    {
        return "Granular instrument: every key plays a cloud of grains of your WAV or FLAC file (or a built-in "
               "tone), with position, spray, scan, grain size, density, pitch and stereo spread, reverse grains "
               "and an envelope.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'G', 'r', 'n'); }

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
        state.description = "WAV or FLAC file the grains are taken from (none: the built-in sound)";
    }

    // the sample file: read and converted off the audio thread, handed over to run() without a lock
    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "sample") != 0)
            return;
        nullsmp::SampleFile file;
        nullsmp::readSample(value, file, nullgrn::kMaxValues);
        Pending* p = new Pending();
        p->data.assign(file.data.begin(), file.data.end()); // file_mem's values
        p->channels = file.channels;
        p->rate = file.sampleRate;
        if (p->channels > 0 && p->data.size() >= size_t(4 * p->channels))
            nullgrn::makeOverview(p->data.data(), p->channels, double(p->data.size() / size_t(p->channels)), p->ov);
        fFiles.post(p);
    }

    void process(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        // the JSFX sends every message it receives on (midisend in @block); SysEx, which the
        // JSFX never sees, passes through there as well
        for (uint32_t i = 0; i < midiCount; ++i)
            writeMidiEvent(midi[i]);

        // a new file: the JSFX's @slider after slider1 changed
        if (Pending* p = fFiles.take())
        {
            fDsp.setSample(p->data, p->channels, p->rate); // the old data goes back in p, freed off the audio thread
            if (fDsp.fileOk())
                std::memcpy(overview, p->ov, sizeof(overview));
            else
                std::memcpy(overview, fBuiltinOv, sizeof(overview));
            fFiles.giveBack(p);
            publish();
        }

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < uint32_t(nullgrn::kMaxEvents); ++i)
        {
            const MidiEvent& ev = midi[i];
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 1 && ev.size <= 3 && d[0] >= 0x80 && d[0] < 0xF0)
                fEvents[n++] = { ev.frame, d[0], uint8_t(ev.size > 1 ? d[1] : 0), uint8_t(ev.size > 2 ? d[2] : 0) };
        }
        fDsp.beginBlock(fEvents, n);
        fDsp.process(outputs[0], outputs[1], frames);

        // the grains for the display
        const double nfr = fDsp.frames();
        for (int i = 0; i < nullgrn::kGrains; ++i)
        {
            const nullgrn::Granular::Grain& g = fDsp.grain(i);
            const bool on = g.on != 0 && nfr > 0;
            grainPos[i] = on ? float(g.pos / nfr) : -1.0f;
            grainWin[i] = on ? float(std::sin(nullgrn::kPi * g.t / g.len)) : 0.0f;
        }
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        update();
        // the built-in sound's waveform, shown when no file plays
        const std::vector<double>& b = fDsp.builtin();
        nullgrn::makeOverview(b.data(), 1, double(b.size()), fBuiltinOv);
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
        double v[nullgrn::kNumParams];
        for (uint32_t i = 0; i < nullgrn::kNumParams; ++i)
            v[i] = jsfxValue(nullgrn::kSliders[i], value(i));
        fDsp.setParams(v);
    }

    nullgrn::Granular fDsp;
    nullgrn::MidiEv fEvents[nullgrn::kMaxEvents];
    float fBuiltinOv[nullgrn::kOverview] = {};

    // a loaded sample on its way to run(): the file_mem values, its format and its waveform
    struct Pending {
        std::vector<double> data;
        int channels = 0;
        double rate = 0.0;
        float ov[nullgrn::kOverview] = {};
    };
    nulljsfx::Mailbox<Pending> fFiles;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginGranular)
};

Plugin* createPlugin()
{
    return new PluginGranular();
}

END_NAMESPACE_DISTRHO
