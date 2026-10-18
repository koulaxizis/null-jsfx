/*
 * NULL JSFX - Wah (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "WahDSP.hpp"
#include "WahSliders.hpp"

START_NAMESPACE_DISTRHO

// as wah_gfx.jsfx: pedal, mode, model, resonance, mix, output / sensitivity, attack, release,
// rate, sync
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "PEDAL", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "MODEL", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "RESONANCE", 0, -1, false },
    { 9, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 10, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
    { 4, NullKnobsUI::kKnob, "SENSITIVITY", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "ATTACK", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "RELEASE", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "RATE", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "SYNC", 1, -1, false },
};

class UIWah : public NullKnobsUI
{
public:
    UIWah()
        : NullKnobsUI({ "WAH", "ESSENTIALS", 480, 120, nullptr, 2, nullwah::kUiSliders, nullwah::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    double rate() const
    {
        const double sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    // the live pedal position (from the plugin), else the Pedal slider
    double pedal() const
    {
        if (nulljsfx::Taps* t = taps())
            return std::max(0.0, std::min(1.0, double(t->extra.load(std::memory_order_relaxed))));
        return value(0) / 100.0;
    }

    // as in wah_gfx.jsfx: the response at the live pedal position (the dashed line at the peak,
    // two in Vocal), and on the right the pedal itself (heel at the bottom, toe at the top)
    void drawDisplay() override
    {
        const double sr = rate(), p = pedal();
        const int mode = int(std::lround(value(1))), model = int(std::lround(value(2)));
        const double res = std::lround(value(3)) / 100.0, mix = std::lround(value(9)) / 100.0;
        const double out = nullwah::sliderValue(10, value(10));
        const double f1 = nullwah::centre1(model, p);
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 86.0f, fVH - 20.0f);
        gridFreq(36.0f);
        dashedV(freqX(float(f1)));
        if (model == 1)
            dashedV(freqX(float(nullwah::centre2(p))));
        constexpr int n = 347;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullwah::responseDb(f, sr, model, res, p, mix, out)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s  %s", nullwah::kModeShort[std::max(0, std::min(2, mode))],
                      nullwah::kModelShort[std::max(0, std::min(2, model))]);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        // the pedal
        label(430.0f, 69.0f, ALIGN_CENTER, 7.0f, 1.0f, theme::kMuted, "PEDAL", true);
        const float tx = 425.0f, ty = 77.0f, tw = 10.0f, th = 58.0f;
        beginPath();
        roundedRect(tx, ty, tw, th, 3.0f);
        fillColor(col(theme::kBackground));
        fill();
        const float fh = th * float(p);
        if (fh > 0.0f)
        {
            beginPath();
            rect(tx, ty + th - fh, tw, fh);
            fillColor(col(theme::kAccent));
            fill();
        }
        beginPath();
        roundedRect(tx, ty, tw, th, 3.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        std::snprintf(buf, sizeof(buf), "%d%%", int(std::floor(p * 100.0 + 0.5)));
        label(430.0f, 146.0f, ALIGN_CENTER, 8.5f, 0.0f, theme::kText, buf);
        const double f = std::floor(f1 + 0.5);
        if (f >= 1000.0)
            std::snprintf(buf, sizeof(buf), "%.2fk", f / 1000.0);
        else
            std::snprintf(buf, sizeof(buf), "%d", int(f));
        label(430.0f, 158.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        static const char* const kNotes[] = { "1/1", "1/2", "1/4", "1/8", "1/16" };
        const int sync = int(std::lround(value(8)));
        if (param == 7 && sync > 0)
            std::snprintf(buf, n, "%s", kNotes[std::min(4, sync - 1)]);
        else if (param == 7)
            std::snprintf(buf, n, "%.1f Hz", nullwah::sliderValue(7, v));
        else if (param == 10)
            std::snprintf(buf, n, v > 0.0f ? "+%.1f dB" : "%.1f dB", nullwah::sliderValue(10, v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIWah();
}

END_NAMESPACE_DISTRHO
