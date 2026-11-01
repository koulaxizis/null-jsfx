/*
 * NULL JSFX - Test Signal (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TestSignalDSP.hpp"
#include "TestSignalSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in test_signal_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SIGNAL", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "FREQUENCY", 0, -1, false, true },
    { 2, NullKnobsUI::kKnob, "LEVEL", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SWEEP TIME", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "CHANNELS", 1, -1, false },
    { 5, NullKnobsUI::kSelect, "INVERT R", 1, -1, false },
    { 6, NullKnobsUI::kSelect, "PASS INPUT", 1, -1, false },
};

class UITestSignal : public NullKnobsUI
{
public:
    UITestSignal()
        : NullKnobsUI({ "TEST SIGNAL", "ESSENTIALS", 480, 120, nullptr, 2, nulltestsignal::kUiSliders,
                        nulltestsignal::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in test_signal_gfx.jsfx: the signal, its frequency and level on the left, the output
    // waveform on the right
    void drawDisplay() override
    {
        static const char* const kNames[] = { "SINE", "SWEEP", "PINK NOISE", "WHITE NOISE", "POLARITY" };
        const nulltestsignal::Params p
            = nulltestsignal::paramsFrom([this](uint32_t i) { return nulltestsignal::sliderValue(i, value(i)); });
        const int sig = std::min(4, std::max(0, p.signal));
        const float tx = fVX + 8.0f;
        label(tx, fVY + 14.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, kNames[sig], true);

        nulljsfx::Taps* t = taps();
        char buf[48];
        if (sig <= 1)
        {
            const double f = sig == 0 ? p.frequency : (t ? double(t->aux[0].load(std::memory_order_relaxed)) : 20.0);
            if (f >= 1000.0)
                std::snprintf(buf, sizeof(buf), "%.2f kHz", f / 1000.0);
            else
                std::snprintf(buf, sizeof(buf), "%.0f Hz", f);
        }
        else
            std::snprintf(buf, sizeof(buf), "%s", sig == 4 ? "PULSE 2/S" : "20 HZ - 20 KHZ");
        label(tx, fVY + 30.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        std::snprintf(buf, sizeof(buf), sig == 2 || sig == 3 ? "%.1f dBFS RMS" : "%.1f dBFS PEAK", p.level);
        label(tx, fVY + 46.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        label(tx, fVY + 62.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted,
              p.channels == 1 ? "LEFT ONLY" : p.channels == 2 ? "RIGHT ONLY" : p.invert ? "BOTH, RIGHT INVERTED" : "BOTH",
              true);
        const float out = t ? t->outLevel.load(std::memory_order_relaxed) : 0.0f;
        meter(tx, fVY + fVH - 22.0f, 150.0f, 8.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);

        setPlot(fVX + 180.0f, fVY + 4.0f, fVW - 184.0f, fVH - 8.0f);
        beginPath();
        moveTo(fPX, fPY + fPH * 0.5f);
        lineTo(fPX + fPW, fPY + fPH * 0.5f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        // scaled to the Level, so the shape shows at any level
        scope(float(0.8 / std::fmax(std::pow(10.0, p.level / 20.0), 0.01)));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nulltestsignal::sliderValue(param, v);
        if (param == 1)
        {
            if (x >= 1000.0)
                std::snprintf(buf, n, "%.2f kHz", x / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(x));
        }
        else if (param == 2)
            std::snprintf(buf, n, "%.1f dB", x);
        else if (param == 3)
            std::snprintf(buf, n, "%.1f s", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UITestSignal();
}

END_NAMESPACE_DISTRHO
