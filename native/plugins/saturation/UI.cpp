/*
 * NULL JSFX - Saturation (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SaturationDSP.hpp"
#include "SaturationSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TONE", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "MODE", 0, -1, false },
};

class UISaturation : public NullKnobsUI
{
public:
    UISaturation()
        : NullKnobsUI({ "SATURATION", "ESSENTIALS", 480, 120, nullptr, 1, nullsaturation::kUiSliders,
                        nullsaturation::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in saturation_gfx.jsfx: the transfer curve (input -1..+1 across, output -1..+1 up)
    // after the dry/wet mix, Tone left out
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(8, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+1");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");
        label(fPX + fPW * 0.5f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "IN");
        char buf[32];
        std::snprintf(buf, sizeof(buf), "DRIVE %.1fx", 1.0 + value(0) / 100.0 * 3.0);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        const double drive = value(0) / 100.0, mix = value(2) / 100.0;
        const int mode = int(std::lround(value(3)));
        const int n = std::max(2, int(fPW) + 1);
        float xs[512], ys[512];
        const int m = std::min(n, 512);
        for (int i = 0; i < m; ++i)
        {
            const double x = -1.0 + 2.0 * double(i) / double(m - 1);
            const double y = x * (1.0 - mix) + nullsaturation::shape(x * (1.0 + drive * 3.0), mode, drive) * mix;
            xs[i] = fPX + fPW * float(i) / float(m - 1);
            ys[i] = linY(float(y), -1.0f, 1.0f);
        }
        curve(xs, ys, m, linY(0.0f, -1.0f, 1.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
        {
            if (std::fabs(v) < 0.05f)
                std::snprintf(buf, n, "0.0 dB");
            else
                std::snprintf(buf, n, "%+.1f dB", v);
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISaturation();
}

END_NAMESPACE_DISTRHO
