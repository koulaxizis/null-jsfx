/*
 * NULL JSFX - Drum Machine (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The parameters are the JSFX sliders; the User pattern (the JSFX's @serialize) is the state
 * "pattern", 170 numbers separated by spaces (DrumMachinePatterns.hpp), which the interface
 * sets when the grid is edited. Eight outputs in four stereo port groups, like the JSFX's pins.
 */

#include "NullPlugin.hpp"
#include "DrumMachineDSP.hpp"
#include "DrumMachineSliders.hpp"

#include <atomic>
#include <cstdlib>
#include <string>

START_NAMESPACE_DISTRHO

class PluginDrumMachine : public NullMultiPlugin
{
public:
    PluginDrumMachine()
        : NullMultiPlugin(nulldrum::kSliders, nulldrum::kNumParams, 1)
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    // the User pattern as text, and back (false when the text is not a pattern)
    static String encode(const double* p)
    {
        std::string s;
        for (int i = 0; i < nulldrum::kPatternSize; ++i)
        {
            s += i ? " " : "";
            s += std::to_string(int(p[i]));
        }
        return String(s.c_str());
    }
    static bool decode(const char* text, double* p)
    {
        const char* c = text;
        for (int i = 0; i < nulldrum::kPatternSize; ++i)
        {
            char* end = nullptr;
            const long v = std::strtol(c, &end, 10);
            if (end == c)
                return false;
            const bool len = i >= nulldrum::kTracks * nulldrum::kSteps;
            p[i] = double(len ? std::max(1L, std::min(16L, v)) : std::max(0L, std::min(4L, v)));
            c = end;
        }
        return true;
    }

protected:
    const char* getLabel() const override { return "Drum Machine"; }
    const char* getDescription() const override
    {
        return "Ten synthesized drum voices (808, 909 and more) with a 16-step sequencer that follows the host, played by MIDI too.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'D', 'r', 'm'); }

    // four stereo port groups, like the JSFX pins: the main mix (or the kick with Outputs
    // Groups), then snare, hats and perc
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        static const char* const kSide[] = { "Left", "Right" };
        static const char* const kGroup[] = { "Main", "Snare", "Hats", "Perc" };
        if (input)
            return NullMultiPlugin::initAudioPort(input, index, port);
        const uint32_t g = index / 2;
        port.groupId = g;
        port.name = g == 0 ? String(kSide[index % 2]) : String(kGroup[g]) + " " + kSide[index % 2];
        port.symbol = String("out") + String(int(index + 1));
    }

    void initPortGroup(uint32_t groupId, PortGroup& group) override
    {
        static const char* const kGroup[] = { "Main", "Snare", "Hats", "Perc" };
        static const char* const kSym[] = { "main", "snare", "hats", "perc" };
        group.name = kGroup[groupId % 4];
        group.symbol = kSym[groupId % 4];
    }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void initState(uint32_t index, State& state) override
    {
        if (index != 0)
            return;
        double p[nulldrum::kPatternSize];
        nulldrum::defaultUser(p);
        state.key = "pattern";
        state.label = "User pattern";
        state.defaultValue = encode(p);
    }

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "pattern") != 0 || !decode(value, fPending))
            return;
        fPendingReady.store(true, std::memory_order_release);
    }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        if (fPendingReady.exchange(false, std::memory_order_acquire))
            std::copy(fPending, fPending + nulldrum::kPatternSize, fDsp.user());

        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        double beatPos = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beatPos = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                          tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beatPos = double(tp.frame) / getSampleRate() * tempo / 60.0;
        }
        // REAPER runs the JSFX's @init again when the transport starts
        if (tp.playing && !fWasPlaying)
        {
            fDsp.reset();
            update();
        }
        fWasPlaying = tp.playing;

        uint32_t n = 0;
        for (uint32_t i = 0; i < midiCount && n < nulldrum::kMaxNotes; ++i)
        {
            const MidiEvent& ev = midi[i];
            const uint8_t* d = ev.size > MidiEvent::kDataSize ? ev.dataExt : ev.data;
            if (ev.size >= 3 && (d[0] & 0xF0) == 0x90 && d[2] > 0)
                fNotes[n++] = { ev.frame, d[1], d[2] / 127.0 };
        }
        fDsp.beginBlock(fNotes, n, tp.playing, tempo, beatPos);
        if (const int req = taps.request.exchange(0))
            fDsp.trig(req - 1, 1.0);
        fDsp.process(outputs, frames);

        taps.extra.store(float(fDsp.step), std::memory_order_relaxed);
        for (int t = 0; t < nulldrum::kTracks; ++t)
            taps.aux[t].store(float(fDsp.hits[t]), std::memory_order_relaxed);
    }

private:
    void update()
    {
        double v[nulldrum::kNumSliders];
        for (uint32_t i = 0; i < nulldrum::kNumSliders; ++i)
        {
            const SliderInfo& in = nulldrum::kSliders[i];
            // back on the JSFX's grid in double (a float 0.1 step is not 0.1)
            v[i] = in.step > 0.0f ? double(in.min) + std::round((double(value(i)) - in.min) / double(in.step)) * double(in.step)
                                  : double(value(i));
            if (in.step > 0.0f && in.step < 1.0f)
                v[i] = std::round(v[i] * 10.0) / 10.0;
        }
        fDsp.setParams(v);
    }

    nulldrum::DrumMachine fDsp;
    nulldrum::Note fNotes[nulldrum::kMaxNotes];
    double fPending[nulldrum::kPatternSize] = {};
    std::atomic<bool> fPendingReady { false };
    bool fWasPlaying = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginDrumMachine)
};

Plugin* createPlugin()
{
    return new PluginDrumMachine();
}

END_NAMESPACE_DISTRHO
