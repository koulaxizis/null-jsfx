/*
 * NULL JSFX - Arpeggiator (VST3/CLAP) metadata
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "NULL JSFX"
#define DISTRHO_PLUGIN_NAME    "Arpeggiator"
#define DISTRHO_PLUGIN_URI     "https://nulljsfx.tech/plugins/arpeggiator"
#define DISTRHO_PLUGIN_CLAP_ID "tech.nulljsfx.arpeggiator"

#define DISTRHO_PLUGIN_BRAND_ID  NulJ
#define DISTRHO_PLUGIN_UNIQUE_ID NArp

#ifdef NULL_NO_UI
#define DISTRHO_PLUGIN_HAS_UI        0
#else
#define DISTRHO_PLUGIN_HAS_UI        1
#endif
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
// a MIDI effect: MIDI in, MIDI out; the stereo audio passes through untouched (so that hosts
// that only load audio effects in an effect slot accept it)
#define DISTRHO_PLUGIN_NUM_INPUTS    2
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_MIDI_OUTPUT 1
#define DISTRHO_PLUGIN_WANT_LATENCY  0
#define DISTRHO_PLUGIN_WANT_TIMEPOS  1
#define DISTRHO_PLUGIN_WANT_STATE    0
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_UI_USE_NANOVG        1
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_DEFAULT_WIDTH     480
#define DISTRHO_UI_DEFAULT_HEIGHT    430

#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:MIDIPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Tools"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "note-effect", "utility"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
