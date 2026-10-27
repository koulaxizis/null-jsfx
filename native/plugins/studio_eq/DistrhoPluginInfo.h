/*
 * NULL JSFX - Studio EQ (VST3/CLAP) metadata
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "NULL JSFX"
#define DISTRHO_PLUGIN_NAME    "Studio EQ"
#define DISTRHO_PLUGIN_URI     "https://nulljsfx.tech/plugins/studio_eq"
#define DISTRHO_PLUGIN_CLAP_ID "tech.nulljsfx.studio_eq"

// VST3 class id is derived by DPF from these two 4-char codes (+ plugin name).
#define DISTRHO_PLUGIN_BRAND_ID  NulJ
#define DISTRHO_PLUGIN_UNIQUE_ID NSEq

#ifdef NULL_NO_UI
#define DISTRHO_PLUGIN_HAS_UI        0
#else
#define DISTRHO_PLUGIN_HAS_UI        1
#endif
#define DISTRHO_PLUGIN_IS_RT_SAFE    1
#define DISTRHO_PLUGIN_NUM_INPUTS    2
#define DISTRHO_PLUGIN_NUM_OUTPUTS   2
#define DISTRHO_PLUGIN_WANT_LATENCY  1
#define DISTRHO_PLUGIN_WANT_STATE    0
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1
#define DISTRHO_PLUGIN_WANT_TIMEPOS  0
#define DISTRHO_UI_USE_NANOVG        1
#define DISTRHO_UI_USER_RESIZABLE    1
#define DISTRHO_UI_DEFAULT_WIDTH     480
#define DISTRHO_UI_DEFAULT_HEIGHT    468

#define DISTRHO_PLUGIN_LV2_CATEGORY    "lv2:ParaEQPlugin"
#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|EQ|Mastering|Stereo"
#define DISTRHO_PLUGIN_CLAP_FEATURES   "audio-effect", "equalizer", "mastering", "stereo"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
