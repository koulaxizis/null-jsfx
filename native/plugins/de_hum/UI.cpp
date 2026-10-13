/*
 * NULL JSFX - De-Hum (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DeHumDSP.hpp"
#include "DeHumSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MAINS", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "HARMONICS", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "Q", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "FINE", 0, -1, true },
    { 5, NullKnobsUI::kSelect, "LOW CUT", 0, -1, false },
};

class UIDeHum : public NullKnobsUI
{
public:
    UIDeHum()
        : NullKnobsUI({ "DE-HUM", "ESSENTIALS", 480, 120, nullptr, 1, nulldehum::kSliders, nulldehum::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    static constexpr float kLo = -64.0f, kHi = 8.0f;

    // design x of f on the 20 Hz .. 2 kHz axis
    float hx(double f) const { return fPX + fPW * float(std::log(std::max(f, 20.0) / 20.0) / std::log(100.0)); }

    // as in de_hum_gfx.jsfx: the response of the whole chain from 20 Hz to 2 kHz; where a notch
    // is narrower than a column it shows its depth
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);

        // grid: frequency lines (decades brighter), 0 / -24 / -48 dB, frequency labels
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 1000.0f; d *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * d;
                    if (f < 20.0f || f > 2000.0f || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::round(hx(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        for (int db = 0; db >= -48; db -= 24)
        {
            const float y = std::round(linY(float(db), kLo, kHi)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(db == 0 ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", db);
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float capY = fPY + fPH + 11.0f;
        label(hx(50.0), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "50");
        label(hx(100.0), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(hx(200.0), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "200");
        label(hx(500.0), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "500");
        label(hx(1000.0), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nulldehum::Params p;
        p.mains = int(std::lround(value(0)));
        p.harmonics = nulldehum::sliderValue(1, value(1));
        p.q = nulldehum::sliderValue(2, value(2));
        p.depth = nulldehum::sliderValue(3, value(3));
        p.fine = nulldehum::sliderValue(4, value(4));
        p.lowCut = int(std::lround(value(5)));
        nulldehum::Coeffs c;
        c.compute(p, sr);

        const float zy = linY(0.0f, kLo, kHi);
        const int n = std::min(512, int(fPW));
        float y1[512], y2[512];
        for (int i = 0; i < n; ++i)
        {
            double mn = 1000.0, mx = -1000.0;
            for (int j = 0; j <= 6; ++j)
            {
                const double d = c.responseDb(20.0 * std::pow(100.0, (i + j / 6.0) / double(fPW)), sr);
                mn = std::min(mn, d);
                mx = std::max(mx, d);
            }
            // a notch centre inside this column
            for (int h = 1; h <= c.numNotches(); ++h)
            {
                const double f = std::min(c.f0 * h, sr * 0.45);
                const double cc = std::log(f / 20.0) / std::log(100.0) * double(fPW);
                if (cc >= i && cc < i + 1)
                    mn = std::min(mn, c.responseDb(f, sr));
            }
            y1[i] = linY(float(mx), kLo, kHi);
            y2[i] = linY(float(mn), kLo, kHi);
        }
        beginPath();
        for (int i = 0; i < n; ++i)
            if (y2[i] > zy)
                rect(fPX + float(i), zy, 1.0f, y2[i] - zy);
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
        if (param == 4)
            std::snprintf(buf, n, "%+.2f Hz", nulldehum::sliderValue(4, v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDeHum();
}

END_NAMESPACE_DISTRHO
