/*
 * NULL JSFX - Sampler (VST3/CLAP) metadata
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "NULL JSFX"
#define DISTRHO_PLUGIN_NAME    "Sampler"
#define DISTRHO_PLUGIN_URI     "https://nulljsfx.tech/plugins/sampler"
#define DISTRHO_PLUGIN_CLAP_ID "tech.nulljsfx.sampler"

#define DISTRHO_PLUGIN_BRAND_ID  NulJ
#define DISTRHO_PLUGIN_UNIQUE_ID NSmp

#ifdef NULL_NO_UI
#define DISTRHO_PLUGIN_HAS_UI        0
#else
#define DISTRHO_PLUGIN_HAS_UI        1
#endif
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_IS_SYNTH      1
#define DISTRHO_PLUGIN_NUM_INPUTS    0
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 1
#define DISTRHO_PLUGIN_WANT_TIMEPOS  0
#define DISTRHO_PLUGIN_WANT_LATENCY  0
#define DISTRHO_PLUGIN_WANT_STATE    1 // "sample": the sample file (a path)
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_UI_USE_NANOVG        1
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_DEFAULT_WIDTH     640
#define DISTRHO_UI_DEFAULT_HEIGHT    448
// the FILE cell opens DPF's own file browser (requestStateFile) to pick any WAV/FLAC file.
// DPF's file browser needs DGL_USE_FILE_BROWSER, which the DGL library (and so the UI, VST3
// and CLAP targets) carries; the DSP-only target does not link DGL and needs no browser.
#ifdef DGL_USE_FILE_BROWSER
#define DISTRHO_UI_FILE_BROWSER      1
#endif

#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:InstrumentPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Instrument|Sampler|Stereo"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "instrument", "sampler", "stereo"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
