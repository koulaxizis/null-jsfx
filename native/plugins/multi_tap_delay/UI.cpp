/*
 * NULL JSFX - Multi-Tap Delay (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "MultiTapDSP.hpp"
#include "MultiTapSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in multi_tap_delay_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TAPS", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "PATTERN", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "TIME", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "SYNC", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "NOTE", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "FEEDBACK", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "LOW CUT", 1, -1, false, true },
    { 9, NullKnobsUI::kKnob, "HIGH CUT", 1, -1, false, true },
    { 10, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIMultiTapDelay : public NullKnobsUI
{
public:
    UIMultiTapDelay()
        : NullKnobsUI({ "MULTI-TAP DELAY", "ESSENTIALS", 480, 120, nullptr, 2, nullmultitap::kUiSliders,
                        nullmultitap::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in multi_tap_delay_gfx.jsfx: the taps on a time line (0 to the last tap, or 4 s at
    // most, drawn at 120 BPM when synced), each as a bar up for its left level and down for its right level
    void drawDisplay() override
    {
        setPlot(fVX + 8.0f, fVY + 14.0f, fVW - 16.0f, fVH - 28.0f);
        grid(8, 2, false);
        const nullmultitap::Params p
            = nullmultitap::paramsFrom([this](uint32_t i) { return nullmultitap::sliderValue(i, value(i)); });
        const double sr = 48000.0;
        int pos[8];
        double gl[8], gr[8];
        const int n = nullmultitap::tapLayout(p, sr, 120.0, int(std::floor(4.0 * sr)), pos, gl, gr);
        double last = 1.0;
        for (int i = 0; i < n; ++i)
            last = std::fmax(last, double(pos[i]));
        const float mid = std::round(fPY + fPH * 0.5f) + 0.5f;
        beginPath();
        moveTo(fPX, mid);
        lineTo(fPX + fPW, mid);
        strokeColor(col(theme::kTrackHover));
        strokeWidth(1.0f);
        stroke();
        beginPath();
        for (int i = 0; i < n; ++i)
        {
            const float x = fPX + fPW * float(double(pos[i]) / last) - 2.0f;
            const float hl = fPH * 0.5f * float(gl[i]), hr = fPH * 0.5f * float(gr[i]);
            rect(x, mid - hl, 4.0f, hl);
            rect(x, mid, 4.0f, hr);
        }
        fillColor(col(theme::kAccent));
        fill();
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%d TAPS   %d MS", n, int(std::lround(last * 1000.0 / sr)));
        label(fVX + 8.0f, fVY + 6.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        label(fVX + fVW - 8.0f, fVY + 6.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, "L", true);
        label(fVX + fVW - 8.0f, fVY + fVH - 6.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, "R", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullmultitap::sliderValue(param, v);
        if (param == 0)
            std::snprintf(buf, n, "%d", int(x));
        else if (param == 2)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param == 5 || param == 6 || param == 7 || param == 10)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 8 || param == 9)
            std::snprintf(buf, n, x >= 1000.0 ? "%.1f kHz" : "%.0f Hz", x >= 1000.0 ? x / 1000.0 : x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIMultiTapDelay();
}

END_NAMESPACE_DISTRHO
