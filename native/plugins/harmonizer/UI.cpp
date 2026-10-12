/*
 * NULL JSFX - Harmonizer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "HarmonizerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "VOICE 1", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "VOICE 2", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "LEVEL 1", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "LEVEL 2", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DETUNE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "DELAY", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIHarmonizer : public NullKnobsUI
{
public:
    UIHarmonizer()
        : NullKnobsUI({ "HARMONIZER", "ADVANCED", 480, 120, nullptr, 2, nullharmonizer::kSliders, nullharmonizer::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    void dot(float x, float y, float r, uint32_t rgb, float alpha)
    {
        beginPath();
        circle(x, y, r);
        fillColor(col(rgb, alpha));
        fill();
    }

    // as in harmonizer_gfx.jsfx: the voices' pitch (-12 to +12 semitones, detune included)
    // against stereo position; dot size follows the level, brightness the dry/wet Mix
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(4, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+12");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-12");
        label(fPX, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "L");
        label(fPX + fPW * 0.5f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "C");
        label(fPX + fPW, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "R");

        const float sp = value(6) / 100.0f, det = value(4) / 100.0f, mix = value(7) / 100.0f;
        const float cx = fPX + fPW * 0.5f, cy = linY(0.0f, -12.0f, 12.0f);
        const float x1 = linX((1.0f - sp) * 0.5f, 0.0f, 1.0f), x2 = linX(1.0f - (1.0f - sp) * 0.5f, 0.0f, 1.0f);
        const float y1 = linY(value(0) + det, -12.0f, 12.0f), y2 = linY(value(1) - det, -12.0f, 12.0f);
        const float r1 = 3.0f + 5.0f * (value(2) + 48.0f) / 48.0f, r2 = 3.0f + 5.0f * (value(3) + 48.0f) / 48.0f;

        beginPath();
        moveTo(cx, cy);
        lineTo(x1, y1);
        moveTo(cx, cy);
        lineTo(x2, y2);
        strokeColor(col(theme::kAccent, 0.35f));
        strokeWidth(1.0f);
        stroke();
        dot(cx, cy, 4.0f, theme::kMuted, 0.35f + 0.65f * std::cos(mix * float(M_PI) / 2.0f));
        const float wet = 0.35f + 0.65f * std::sin(mix * float(M_PI) / 2.0f);
        dot(x1, y1, r1, theme::kAccent, wet);
        dot(x2, y2, r2, theme::kAccent, wet);
        label(x1 + r1 + 4.0f, y1, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, "1");
        label(x2 + r2 + 4.0f, y2, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, "2");
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param <= 1)
            std::snprintf(buf, n, "%+d st", int(std::lround(v)));
        else if (param <= 3)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 4)
            std::snprintf(buf, n, "%.1f ct", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIHarmonizer();
}

END_NAMESPACE_DISTRHO
