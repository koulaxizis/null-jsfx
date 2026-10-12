/*
 * NULL JSFX - Rotary Speaker (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "RotarySpeakerDSP.hpp"
#include "RotarySpeakerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SPEED", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "ACCEL", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BALANCE", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "WIDTH", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIRotarySpeaker : public NullKnobsUI
{
public:
    UIRotarySpeaker()
        : NullKnobsUI({ "ROTARY SPEAKER", "ADVANCED", 480, 120, nullptr, 1, nullrotary::kUiSliders,
                        nullrotary::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // a rotor seen from above (as rs_rotor in rotary_speaker_gfx.jsfx): the body, the two
    // microphones (L at the top, R turned by Width) and the rotor pointing at angle ph
    void rotor(float cx, float cy, float r, float ph, const char* name, float hz)
    {
        beginPath();
        circle(cx, cy, r);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        const float off = 2.0f * float(M_PI) * 0.25f * value(4) / 100.0f;
        label(cx, cy - r - 7.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "L");
        label(cx + std::sin(off) * (r + 7.0f), cy - std::cos(off) * (r + 7.0f), ALIGN_CENTER, 8.0f, 0.0f,
              theme::kMuted, "R");
        const float a = 2.0f * float(M_PI) * ph, s = std::sin(a), c = std::cos(a);
        beginPath();
        moveTo(cx - s * r * 0.45f, cy + c * r * 0.45f);
        lineTo(cx + s * (r - 3.0f), cy - c * (r - 3.0f));
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineCap(ROUND);
        stroke();
        beginPath();
        circle(cx + s * (r - 3.0f), cy - c * (r - 3.0f), 2.5f);
        circle(cx, cy, 3.0f);
        fillColor(col(theme::kAccent));
        fill();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s %.1f Hz", name, double(hz));
        label(cx, cy + r + 15.0f, ALIGN_CENTER, 7.5f, 1.5f, theme::kMuted, buf, true);
    }

    // as in rotary_speaker_gfx.jsfx: the horn and the drum turning at their current speed,
    // and the curve of the tube amp's overdrive (Drive)
    void drawDisplay() override
    {
        float hph = 0.0f, dph = 0.25f, hhz = 0.8f, dhz = 0.7f;
        if (nulljsfx::Taps* t = taps())
        {
            hph = t->extra.load(std::memory_order_relaxed);
            dph = t->gr.load(std::memory_order_relaxed);
            hhz = t->inLevel.load(std::memory_order_relaxed);
            dhz = t->outLevel.load(std::memory_order_relaxed);
        }
        rotor(104.0f, 102.0f, 28.0f, hph, "HORN", hhz);
        rotor(222.0f, 102.0f, 28.0f, dph, "DRUM", dhz);

        setPlot(330.0f, 66.0f, 122.0f, 84.0f);
        grid(4, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+1");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");
        label(fPX + fPW * 0.5f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "AMP CURVE");
        const double drive = 1.0 + std::round(value(3)) / 100.0 * 6.0;
        constexpr int n = 123;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullrotary::softClip((2.0 * i / (n - 1) - 1.0) * drive, drive)), -1.0f, 1.0f);
        }
        curve(xs, ys, n);
    }
};

UI* createUI()
{
    return new UIRotarySpeaker();
}

END_NAMESPACE_DISTRHO
