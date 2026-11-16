/*
 * NULL JSFX - MIDI Monitor (VST3/CLAP), DSP side: the class is in MidiMonitorPlugin.hpp
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "MidiMonitorPlugin.hpp"

START_NAMESPACE_DISTRHO

Plugin* createPlugin()
{
    return new PluginMidiMonitor();
}

END_NAMESPACE_DISTRHO
