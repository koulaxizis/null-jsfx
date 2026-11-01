/*
 * NULL JSFX - Kick (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "KickDSP.hpp"
#include "KickSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in kick_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "PITCH", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "FOLLOW", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SWEEP", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SWEEP TIME", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "DECAY", 0, -1, false, true },
    { 5, NullKnobsUI::kKnob, "CLICK", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "CLICK TONE", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "DRIVE", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "VELOCITY", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "VOLUME", 1, -1, true },
};

class UIKick : public NullKnobsUI
{
public:
    UIKick()
        : NullKnobsUI({ "KICK", "INSTRUMENTS", 560, 150, nullptr, 2, nullkick::kUiSliders, nullkick::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in kick_gfx.jsfx: one kick over time, its level (filled) and its pitch (20 Hz to 2 kHz,
    // log), the start and end pitch and the output level
    void drawDisplay() override
    {
        const nullkick::Params p = nullkick::paramsFrom([this](uint32_t i) { return nullkick::sliderValue(i, value(i)); });
        setPlot(fVX + 4.0f, fVY + 18.0f, fVW - 170.0f, fVH - 22.0f);
        grid(8, 4, false);
        const double len = std::fmax(0.2, p.decay / 1000.0 * 1.1); // seconds shown
        const double sw = p.sweep / 12.0;
        const int n = std::min(512, int(fPW) + 1);
        float xs[512], ya[512], yp[512];
        for (int i = 0; i < n; ++i)
        {
            const double t = len * i / double(fPW);
            xs[i] = fPX + float(i);
            ya[i] = fPY + fPH * float(1.0 - std::exp(-6.907755278982137 * t / (p.decay / 1000.0)));
            const double f = p.pitch * std::pow(2.0, sw * std::exp(-t / (p.sweepTime / 1000.0)));
            yp[i] = fPY + fPH * float(1.0 - std::log(std::fmin(2000.0, std::fmax(20.0, f)) / 20.0) / std::log(100.0));
        }
        // the level: filled only
        beginPath();
        moveTo(xs[0], fPY + fPH);
        for (int i = 0; i < n; ++i)
            lineTo(xs[i], ya[i]);
        lineTo(xs[n - 1], fPY + fPH);
        closePath();
        fillColor(col(theme::kAccent, 0.16f));
        fill();
        curve(xs, yp, n);

        char buf[48];
        const double ms = len * 1000.0;
        if (len >= 1.0)
            std::snprintf(buf, sizeof(buf), "%.1f S", len);
        else
            std::snprintf(buf, sizeof(buf), "%d MS", int(std::floor(ms + 0.5)));
        label(fVX + fVW - 170.0f, fVY + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, buf, true);
        label(fVX + 6.0f, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted,
              p.follow ? "FOLLOWS NOTES (C1 = PITCH)" : "EVERY KEY PLAYS PITCH", true);
        const float tx = fVX + fVW - 150.0f;
        std::snprintf(buf, sizeof(buf), "%.1f HZ", p.pitch);
        label(tx, fVY + 30.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        std::snprintf(buf, sizeof(buf), "FROM %.0f HZ", p.pitch * std::pow(2.0, sw));
        label(tx, fVY + 46.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        std::snprintf(buf, sizeof(buf), "CLICK %d%%", int(p.click));
        label(tx, fVY + 62.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        nulljsfx::Taps* t = taps();
        const float out = t ? t->outLevel.load(std::memory_order_relaxed) : 0.0f;
        meter(tx, fVY + fVH - 22.0f, 140.0f, 8.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullkick::sliderValue(param, v);
        switch (param)
        {
        case 0: std::snprintf(buf, n, "%.1f Hz", x); break;
        case 2: std::snprintf(buf, n, "%.1f st", x); break;
        case 3: std::snprintf(buf, n, "%.1f ms", x); break;
        case 4: std::snprintf(buf, n, "%d ms", int(x)); break;
        case 6:
            if (x >= 1000.0)
                std::snprintf(buf, n, "%.1f kHz", x / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(x));
            break;
        case 5: case 7: case 8: std::snprintf(buf, n, "%d%%", int(x)); break;
        case 9: std::snprintf(buf, n, "%+.1f dB", x); break;
        default: NullKnobsUI::formatValue(param, v, buf, n);
        }
    }
};

UI* createUI()
{
    return new UIKick();
}

END_NAMESPACE_DISTRHO
