/*
 * NULL JSFX - Comb Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "CombFilterDSP.hpp"
#include "CombFilterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "FREQ", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "FEEDBACK", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "GAIN", 0, -1, true },
};

class UICombFilter : public NullKnobsUI
{
public:
    UICombFilter()
        : NullKnobsUI({ "COMB FILTER", "ADVANCED", 480, 120, nullptr, 1, nullcomb::kSliders, nullcomb::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in comb_filter_gfx.jsfx: the magnitude response with the level compensation and Gain;
    // where the teeth get denser than a column it shows their range
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(24.0f);
        dashedV(freqX(value(0)));

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullcomb::Coeffs c = nullcomb::coeffs(sr, nullcomb::bufferSize(sr), value(0), value(1),
                                                    int(std::lround(value(2))), value(3));
        const float zy = linY(0.0f, -16.0f, 16.0f);
        const int n = std::min(512, int(fPW));
        float y1[512], y2[512];
        for (int i = 0; i < n; ++i)
        {
            double mn = 1000.0, mx = -1000.0;
            for (int j = 0; j <= 6; ++j)
            {
                const double d = nullcomb::responseDb(c, 20.0 * std::pow(1000.0, (i + j / 6.0) / double(fPW)), sr);
                mn = std::min(mn, d);
                mx = std::max(mx, d);
            }
            y1[i] = linY(float(mx), -16.0f, 16.0f);
            y2[i] = linY(float(mn), -16.0f, 16.0f);
        }
        beginPath();
        for (int i = 0; i < n; ++i)
            if (y1[i] < zy)
                rect(fPX + float(i), y1[i], 1.0f, zy - y1[i]);
        fillColor(col(theme::kAccent, 0.10f));
        fill();
        beginPath();
        for (int i = 0; i < n; ++i)
            rect(fPX + float(i), y1[i] - 1.0f, 1.0f, y2[i] - y1[i] + 2.0f);
        fillColor(col(theme::kAccent));
        fill();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", v / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(v));
        }
        else if (param == 1)
            std::snprintf(buf, n, "%.2f", v);
        else
            std::snprintf(buf, n, "%+.1f dB", v);
    }
};

UI* createUI()
{
    return new UICombFilter();
}

END_NAMESPACE_DISTRHO
