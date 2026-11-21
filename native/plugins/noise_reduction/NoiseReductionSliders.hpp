/*
 * NULL JSFX - Noise Reduction: the JSFX sliders and the View the plugin shares with the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullPlugin.hpp"
#include "NoiseReductionDSP.hpp"

namespace nullnr {

static const char* const kOffOn[] = { "OFF", "ON" };
static const char* const kListenOptions[] = { "OUTPUT", "REMOVED" };

// slider1..slider9 and slider11 of noise_reduction.jsfx, in order (slider10, the Noise File, is
// the "noisefile" state, not a parameter, like Convolution's IR File)
static const DISTRHO::SliderInfo kSliders[] = {
    { "Learn", "learn", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Auto", "auto", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn },
    { "Reduction", "reduction", 0.0f, 60.0f, 18.0f, "dB", 1.0f },
    { "Threshold", "threshold", -10.0f, 20.0f, 3.0f, "dB", 0.5f },
    { "Smoothing", "smoothing", 0.0f, 100.0f, 50.0f, "%", 1.0f },
    { "Attack", "attack", 0.0f, 100.0f, 10.0f, "ms", 1.0f },
    { "Release", "release", 10.0f, 1000.0f, 150.0f, "ms", 1.0f },
    { "Listen", "listen", 0.0f, 1.0f, 0.0f, "", 1.0f, kListenOptions },
    { "Mix", "mix", 0.0f, 100.0f, 100.0f, "%", 1.0f },
    { "Use Noise File", "use_noise_file", 0.0f, 1.0f, 0.0f, "", 1.0f, kOffOn }, // 4.2, appended
};
static constexpr uint32_t kNumSliders = sizeof(kSliders) / sizeof(kSliders[0]);
// the Rack module keeps the first nine: it has no files (rack/modules/convolution.hpp)
static constexpr uint32_t kNumModuleSliders = 9;

} // namespace nullnr

START_NAMESPACE_DISTRHO

// the plugin with its View, so the interface (direct access) can draw the spectrum, the noise
// estimate and the reduction curve; two states (4.2): "noisefile", the path of the Noise File,
// and "noiseprofile", the profile in use as saved with the project (NoiseFile.hpp)
class NoiseReductionBase : public NullMultiPlugin
{
public:
    NoiseReductionBase() : NullMultiPlugin(nullnr::kSliders, nullnr::kNumSliders, 2) {}
    nullnr::View view;

#if DISTRHO_PLUGIN_WANT_STATE // (the Rack includes this file with its own plugin info)
protected:
    void initState(uint32_t index, State& state) override
    {
        if (index == 0)
        {
            state.hints = kStateIsFilenamePath;
            state.key = "noisefile";
            state.defaultValue = "";
            state.label = "Noise File";
            state.description = "WAV file of the noise alone (room tone) the profile is learnt from";
        }
        else if (index == 1)
        {
            state.hints = kStateIsOnlyForDSP;
            state.key = "noiseprofile";
            state.defaultValue = "";
            state.label = "Noise Profile";
            state.description = "The noise profile in use, saved with the project";
        }
    }
#endif
};

END_NAMESPACE_DISTRHO
