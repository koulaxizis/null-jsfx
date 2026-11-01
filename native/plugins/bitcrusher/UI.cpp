/*
 * NULL JSFX - Bitcrusher (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BitcrusherDSP.hpp"
#include "BitcrusherSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in bitcrusher_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "BITS", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "RATE", 0, -1, false, true },
    { 2, NullKnobsUI::kSelect, "ANTI-ALIAS", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SMOOTH", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DRIVE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIBitcrusher : public NullKnobsUI
{
public:
    UIBitcrusher()
        : NullKnobsUI({ "BITCRUSHER", "ESSENTIALS", 480, 120, nullptr, 2, nullbitcrusher::kUiSliders,
                        nullbitcrusher::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in bitcrusher_gfx.jsfx: one cycle of a 250 Hz sine at the Drive level (dim) and what the
    // decimator and Bits make of it (Anti Alias and Smooth left out), with the levels and the rate
    void drawDisplay() override
    {
        const float px = fVX + 8.0f, py = fVY + 6.0f, pw = fVW - 16.0f, ph = fVH - 22.0f;
        setPlot(px, py, pw, ph);
        grid(4, 4, false);
        const double bits = nullbitcrusher::sliderValue(0, value(0));
        const double rate = nullbitcrusher::sliderValue(1, value(1));
        const double drive = nullbitcrusher::sliderValue(4, value(4));
        const double q = std::pow(2.0, bits - 1.0);
        const double amp = std::fmin(1.5, 0.7 * std::pow(10.0, drive / 20.0));
        const int n = std::min(511, int(fPW)) + 1;
        const double hold = rate >= 250.0 * double(n - 1) ? 1.0 / double(n - 1) : 250.0 / rate;
        float xs[512], ys[512];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(std::fmax(-1.6, std::fmin(1.6, amp * std::sin(2.0 * nullbitcrusher::kPi * i / double(n - 1))))),
                         -1.6f, 1.6f);
        }
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kMuted));
        strokeWidth(2.0f);
        stroke();
        resetScissor();
        for (int i = 0; i < n; ++i)
        {
            const double t = hold * std::floor(double(i) / double(n - 1) / hold);
            const double v = std::floor(amp * std::sin(2.0 * nullbitcrusher::kPi * t) * q + 0.5) / q;
            ys[i] = linY(float(std::fmax(-1.6, std::fmin(1.6, v))), -1.6f, 1.6f);
        }
        curve(xs, ys, n);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%d BITS   %d LEVELS", int(bits), int(std::fmin(99999.0, 2.0 * q + 1.0)));
        label(px + pw - 4.0f, py + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kAccent, buf, true);
        if (rate >= 1000.0)
            std::snprintf(buf, sizeof(buf), "%.2f kHz", rate / 1000.0);
        else
            std::snprintf(buf, sizeof(buf), "%d Hz", int(rate));
        label(px + pw - 4.0f, py + 20.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullbitcrusher::sliderValue(param, v);
        if (param == 0)
            std::snprintf(buf, n, "%d", int(x));
        else if (param == 1)
            std::snprintf(buf, n, x >= 1000.0 ? "%.1f kHz" : "%.0f Hz", x >= 1000.0 ? x / 1000.0 : x);
        else if (param == 3 || param == 5)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 4)
            std::snprintf(buf, n, "%.1f dB", x);
        else if (param == 6)
            std::snprintf(buf, n, "%+.1f dB", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIBitcrusher();
}

END_NAMESPACE_DISTRHO
