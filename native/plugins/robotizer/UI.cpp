/*
 * NULL JSFX - Robotizer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "RobotizerDSP.hpp"
#include "RobotizerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "PITCH", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "METALLIC", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RING", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIRobotizer : public NullKnobsUI
{
public:
    UIRobotizer()
        : NullKnobsUI({ "ROBOTIZER", "ADVANCED", 480, 120, nullptr, 1, nullrobotizer::kSliders,
                        nullrobotizer::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in robotizer_gfx.jsfx: the output waveform with a dashed line every pitch period
    void drawDisplay() override
    {
        setPlot(fVX, fVY + 14.0f, fVW, fVH - 14.0f);
        grid(8, 2, true);
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        const int p = nullrobotizer::period(sr, value(0));
        const int n = int(fPW);
        const int per = std::max(1, int(nulljsfx::Taps::kScopeLen) / n);
        for (int k = 1; k * p < n * per; ++k)
            dashedV(fPX + float(k * p) / float(per), 0.35f);
        scope(1.0f);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "PERIOD %.2f ms", p / sr * 1000.0);
        label(fPX, fVY + 4.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        std::snprintf(buf, sizeof(buf), "%d Hz", int(std::lround(value(0))));
        label(fPX + fPW, fVY + 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
    }
};

UI* createUI()
{
    return new UIRobotizer();
}

END_NAMESPACE_DISTRHO
