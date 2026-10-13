/*
 * NULL JSFX - Tape Stop (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TapeStopDSP.hpp"
#include "TapeStopSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "TRIGGER", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "STOP TIME", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "START TIME", 0, -1, false },
};

class UITapeStop : public NullKnobsUI
{
public:
    UITapeStop()
        : NullKnobsUI({ "TAPE STOP", "ADVANCED", 480, 120, nullptr, 1, nulltapestop::kSliders, nulltapestop::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // The JSFX sets Trigger back to Idle when the spin-up ends; the plugin cannot, so the
    // interface does it once it has seen the plugin's trigger latch set and then cleared.
    void uiIdle() override
    {
        if (nulljsfx::Taps* t = taps())
        {
            const bool latched = t->gr.load(std::memory_order_relaxed) >= 0.5f;
            if (latched)
                fSeenLatch = true;
            else if (fSeenLatch)
            {
                fSeenLatch = false;
                if (value(0) >= 0.5f)
                {
                    editParameter(0, true);
                    setParameterValue(0, 0.0f);
                    editParameter(0, false);
                    parameterChanged(0, 0.0f);
                }
            }
        }
        NullKnobsUI::uiIdle();
    }

    // as in tape_stop_gfx.jsfx: the tape speed over a trigger and, while it runs, the playhead
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 16.0f, fVW - 30.0f, fVH - 32.0f);
        grid(4, 2, false);
        const double stop = value(1), start = value(2); // the curve's shape only needs the ratio
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "1x");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        const float bx = fPX + fPW * float(stop / (stop + start));
        label((fPX + bx) * 0.5f, fPY - 8.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "STOP");
        label((bx + fPX + fPW) * 0.5f, fPY - 8.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "START");
        char buf[24];
        label(fPX, fPY + fPH + 8.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0 s");
        std::snprintf(buf, sizeof(buf), "%.2f s", stop / 1000.0);
        label(bx, fPY + fPH + 8.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        std::snprintf(buf, sizeof(buf), "%.2f s", (stop + start) / 1000.0);
        label(fPX + fPW, fPY + fPH + 8.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);

        beginPath();
        moveTo(std::round(bx) + 0.5f, fPY);
        lineTo(std::round(bx) + 0.5f, fPY + fPH);
        strokeColor(col(theme::kAccent, 0.3f));
        strokeWidth(1.0f);
        stroke();

        constexpr int n = 403;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double x = double(i) / double(n - 1);
            xs[i] = fPX + fPW * float(x);
            ys[i] = linY(float(nulltapestop::rateAt(x, stop, start)), 0.0f, 1.0f);
        }
        curve(xs, ys, n, fPY + fPH);

        // playhead
        if (nulljsfx::Taps* t = taps())
        {
            const float p = t->extra.load(std::memory_order_relaxed);
            if (p >= 0.0f)
            {
                const float pc = std::fmin(1.0f, p);
                const float x = std::round(linX(pc, 0.0f, 1.0f)) + 0.5f;
                beginPath();
                moveTo(x, fPY);
                lineTo(x, fPY + fPH);
                strokeColor(col(theme::kText, 0.8f));
                strokeWidth(1.0f);
                stroke();
                beginPath();
                circle(x, linY(float(nulltapestop::rateAt(pc, stop, start)), 0.0f, 1.0f), 3.0f);
                fillColor(col(theme::kText, 0.8f));
                fill();
            }
        }
    }

private:
    bool fSeenLatch = false;
};

UI* createUI()
{
    return new UITapeStop();
}

END_NAMESPACE_DISTRHO
