/*
 * NULL JSFX - Tape (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TapeDSP.hpp"
#include "TapeSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SPEED", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BUMP", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "HISS", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "WOW", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "FLUTTER", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UITape : public NullKnobsUI
{
public:
    UITape()
        : NullKnobsUI({ "TAPE", "ESSENTIALS", 480, 120, nullptr, 2, nulltape::kSliders, nulltape::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in tape_gfx.jsfx: the playback frequency response (low cut, head bump, high-frequency
    // loss of the Speed) and, on the right, the saturation transfer curve at the current Drive
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nulltape::Playback pb;
        pb.set(sr, int(std::lround(value(1))), nulltape::sliderValue(2, value(2)));

        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 126.0f, fVH - 20.0f);
        gridFreq(24.0f);
        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ys[512];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + float(i);
            ys[i] = linY(float(pb.db(20.0 * std::pow(1000.0, i / double(fPW)), sr)), -16.0f, 16.0f);
        }
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));

        setPlot(fVX + fVW - 88.0f, fVY + 4.0f, 84.0f, fVH - 20.0f);
        grid(4, 4, true);
        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kMuted, 0.5f));
        strokeWidth(1.0f);
        stroke();
        label(fPX + fPW * 0.5f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "TRANSFER");
        const double drive = std::pow(10.0, nulltape::sliderValue(0, value(0)) / 20.0);
        const double comp = nulltape::satComp(drive);
        const int m = std::min(512, int(fPW) + 1);
        for (int i = 0; i < m; ++i)
        {
            const double x = i / double(fPW) * 2.0 - 1.0;
            xs[i] = fPX + float(i);
            ys[i] = linY(float(nulltape::satCurve(x * drive) * comp), -1.0f, 1.0f);
        }
        curve(xs, ys, m);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0 || param == 2)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 6)
            std::snprintf(buf, n, "%+.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UITape();
}

END_NAMESPACE_DISTRHO
