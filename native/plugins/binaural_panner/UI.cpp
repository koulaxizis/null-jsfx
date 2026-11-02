/*
 * NULL JSFX - Binaural Panner (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BinauralPannerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "AZIMUTH", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "ELEVATION", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DISTANCE", 0, -1, false, true },
    { 3, NullKnobsUI::kKnob, "ROOM", 0, -1, false },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
    { 4, NullKnobsUI::kKnob, "HEAD SIZE", 1, -1, false },
    { 5, NullKnobsUI::kSelect, "INPUT", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
};

class UIBinauralPanner : public NullKnobsUI
{
public:
    UIBinauralPanner()
        : NullKnobsUI({ "BINAURAL PANNER", "ADVANCED", 480, 150, nullptr, 2, nullbp::kSliders, nullbp::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in binaural_panner_gfx.jsfx: from above (front up), rings at 1, 5 and 20 m on a log scale,
    // the head and the source (two in Stereo); from the side, the elevation; the position as text
    void drawDisplay() override
    {
        const float az = value(0), el = value(1), dist = value(2), room = value(3), width = value(6);
        const bool stereo = value(5) >= 0.5f;
        const float w = (fVW - 8.0f) / 2.0f;
        const float cx = fVX + w / 2.0f, cy = fVY + fVH / 2.0f + 4.0f, r = fVH / 2.0f - 12.0f;
        box(fVX, fVY, w, fVH, theme::kBackground);
        box(fVX + w + 8.0f, fVY, w, fVH, theme::kBackground);
        strokeWidth(1.0f);
        static const char* const kRing[3] = { "1 M", "5 M", "20 M" };
        static const float kD[3] = { 4.0f, 20.0f, 80.0f };
        for (int k = 0; k < 3; ++k)
        {
            const float q = r * std::log(kD[k]) / std::log(80.0f);
            beginPath();
            circle(cx, cy, q);
            strokeColor(col(theme::kBorder));
            stroke();
            label(cx + q * 0.71f + 2.0f, cy - q * 0.71f - 6.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, kRing[k], true);
        }
        head(cx, cy, 0.0f, -1.0f);
        const float rr = r * std::log(dist / 0.25f) / std::log(80.0f) * std::cos(el * float(M_PI) / 180.0f);
        for (int i = 0; i < (stereo ? 2 : 1); ++i)
        {
            const float a = (stereo ? az + width * float(2 * i - 1) : az) * float(M_PI) / 180.0f;
            dot(cx + rr * std::sin(a), cy - rr * std::cos(a));
        }
        label(fVX + 6.0f, fVY + 4.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, "TOP", true);

        // the side view (front right): an arc from -40 to 90 degrees in front of the head
        const float sx = fVX + w + 8.0f + 46.0f, sy = cy + 10.0f, sr = r - 4.0f;
        beginPath();
        arc(sx, sy, sr, -float(M_PI) / 2.0f, 40.0f * float(M_PI) / 180.0f, CW);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        head(sx, sy, 1.0f, 0.0f);
        const float p = el * float(M_PI) / 180.0f;
        dot(sx + sr * std::cos(p), sy - sr * std::sin(p));
        label(fVX + w + 14.0f, fVY + 4.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, "SIDE", true);

        // the position as text
        const float tx = fVX + 2.0f * w + 8.0f - 84.0f;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "AZ %d", int(std::lround(az)));
        label(tx, cy - 34.0f, ALIGN_LEFT, 8.0f, 1.0f, theme::kText, buf, true);
        std::snprintf(buf, sizeof(buf), "EL %d", int(std::lround(el)));
        label(tx, cy - 14.0f, ALIGN_LEFT, 8.0f, 1.0f, theme::kText, buf, true);
        std::snprintf(buf, sizeof(buf), "%.2f M", double(dist));
        label(tx, cy + 6.0f, ALIGN_LEFT, 8.0f, 1.0f, theme::kText, buf, true);
        std::snprintf(buf, sizeof(buf), "ROOM %d%%", int(std::lround(room)));
        label(tx, cy + 26.0f, ALIGN_LEFT, 8.0f, 1.0f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0 || param == 1 || param == 6)
            std::snprintf(buf, n, "%d deg", int(std::lround(v)));
        else if (param == 2)
            std::snprintf(buf, n, "%.2f m", double(v));
        else if (param == 4)
            std::snprintf(buf, n, "%.1f cm", double(v));
        else if (param == 7)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    void box(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb));
        fill();
    }

    // the head (radius 7) with its nose 5 long in direction (nx, ny)
    void head(float x, float y, float nx, float ny)
    {
        beginPath();
        circle(x, y, 7.0f);
        fillColor(col(theme::kMuted));
        fill();
        beginPath();
        moveTo(x + 7.0f * nx, y + 7.0f * ny);
        lineTo(x + 12.0f * nx, y + 12.0f * ny);
        strokeColor(col(theme::kMuted));
        strokeWidth(1.0f);
        stroke();
    }

    void dot(float x, float y)
    {
        beginPath();
        circle(x, y, 3.5f);
        fillColor(col(theme::kAccent));
        fill();
    }
};

UI* createUI()
{
    return new UIBinauralPanner();
}

END_NAMESPACE_DISTRHO
