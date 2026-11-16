/*
 * NULL JSFX - MIDI Monitor (VST3/CLAP), the plugin class, shared by Plugin.cpp and UI.cpp (the
 * interface reads the monitor directly; DISTRHO_PLUGIN_WANT_DIRECT_ACCESS)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A MIDI effect that changes nothing: the audio and every MIDI event (SysEx, which the JSFX
 * never sees, too) go out as they came in; the short messages feed the Monitor.
 */

#pragma once

#include "NullPlugin.hpp"
#include "MidiMonitorDSP.hpp"
#include "MidiMonitorSliders.hpp"

#include <cstring>

START_NAMESPACE_DISTRHO

class PluginMidiMonitor : public NullMultiPlugin
{
public:
    PluginMidiMonitor()
        : NullMultiPlugin(nullmidimonitor::kSliders, nullmidimonitor::kNumSliders)
    {
        update();
        fMon.reset();
    }

    const nullmidimonitor::Monitor& monitor() const { return fMon; }

protected:
    const char* getLabel() const override { return "MidiMonitor"; }
    const char* getDescription() const override
    {
        return "Shows the MIDI that passes, without changing it: the latest messages, the held notes and every channel's bend and pressure.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'M', 'i', 'd'); }

    void valueChanged(uint32_t, float) override { update(); }

    void onActivate() override { fMon.reset(); }

    void process(const float** inputs, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (uint32_t i = 0; i < midiCount; ++i)
        {
            writeMidiEvent(midi[i]);
            if (midi[i].size >= 1 && midi[i].size <= 3)
                fMon.input(midi[i].frame < frames ? midi[i].frame : (frames ? frames - 1 : 0), midi[i].data[0],
                           midi[i].size > 1 ? midi[i].data[1] : 0, midi[i].size > 2 ? midi[i].data[2] : 0,
                           fFreeze, getSampleRate());
        }
        fMon.end(frames);
        for (int c = 0; c < 2; ++c)
            if (outputs[c] != inputs[c])
                std::memmove(outputs[c], inputs[c], sizeof(float) * frames);
    }

private:
    void update() { fFreeze = nullmidimonitor::sliderValue(2, value(2)) >= 0.5; }

    nullmidimonitor::Monitor fMon;
    bool fFreeze = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginMidiMonitor)
};

END_NAMESPACE_DISTRHO
