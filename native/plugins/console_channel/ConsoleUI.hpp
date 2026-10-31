/*
 * NULL JSFX - Console Channel and Console Buss (VST3/CLAP), the interface both plugins share
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullUI.hpp"
#include "ConsoleDSP.hpp"
#include "ConsoleSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kConsoleControls[] = {
    { 0, NullKnobsUI::kSelect, "CHARACTER", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UIConsoleBase : public NullKnobsUI
{
public:
    UIConsoleBase(const char* title, bool buss)
        : NullKnobsUI({ title, "ADVANCED", 480, 120, nullptr, 1, nullconsole::kUiSliders, nullconsole::kNumSliders,
                        kConsoleControls, sizeof(kConsoleControls) / sizeof(kConsoleControls[0]), true }),
          fBuss(buss) {}

protected:
    // as in console_*_gfx.jsfx: left, the saturation curve (input -2..+2 across, output up) without
    // the shelves; right, the character, the drive and input and output meters
    void drawDisplay() override
    {
        setPlot(64.0f, 66.0f, 92.0f, 92.0f);
        grid(4, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+2");
        label(fPX - 6.0f, fPY + fPH / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-2");
        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        const int ch = int(nullconsole::sliderValue(0, value(0)));
        const nullconsole::Coeffs c = nullconsole::coeffsFor(fBuss, ch, nullconsole::sliderValue(1, value(1)),
                                                             nullconsole::sliderValue(2, value(2)), 48000.0);
        constexpr int n = 93;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullconsole::curve(c, -2.0 + 4.0 * double(i) / double(n - 1))), -2.0f, 2.0f);
        }
        curve(xs, ys, n);

        static const char* const kNames[] = { "CLEAN", "WARM", "PUNCHY", "BRIGHT" };
        label(180.0f, 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, kNames[std::min(3, std::max(0, ch))], true);
        char st[32];
        std::snprintf(st, sizeof(st), "+%.1f DB DRIVE", c.driveDb);
        label(448.0f, 66.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, st, true);

        float in = 0.0f, out = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
        }
        meter(180.0f, 90.0f, 268.0f, 10.0f, "IN", nulljsfx::toDb(in), -140.0f, -60.0f);
        meter(180.0f, 118.0f, 268.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullconsole::sliderValue(param, v);
        if (param == 1)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 2)
            std::snprintf(buf, n, "%+.1f dB", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    bool fBuss;
};

END_NAMESPACE_DISTRHO
