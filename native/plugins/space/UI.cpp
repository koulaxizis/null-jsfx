/*
 * NULL JSFX - Space (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpaceDSP.hpp"
#include "SpaceSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in space_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "TYPE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SIZE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DAMPING", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DETUNE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "PRE-DELAY", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UISpace : public NullKnobsUI
{
public:
    UISpace()
        : NullKnobsUI({ "SPACE", "ESSENTIALS", 480, 120, nullptr, 2, nullspace::kUiSliders, nullspace::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in space_gfx.jsfx: the decay (level in dB over time) after the pre-delay, at low and
    // middle frequencies (bright) and at 8 kHz (dim), on a time scale that fits it
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 16.0f, fVW - 34.0f, fVH - 32.0f);
        grid(4, 2, false);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullspace::Params p = nullspace::paramsFrom([this](uint32_t i) { return nullspace::sliderValue(i, value(i)); });
        fCalc.setParamsOnly(p, sr);
        const double rt = fCalc.rt(), r8 = fCalc.rt8(), pre = p.preDelay / 1000.0;
        double top = 0.1;
        int m = 1;
        while (top < (pre + rt) * 1.1)
        {
            top *= m == 2 ? 2.5 : 2.0;
            m = m == 1 ? 2 : m == 2 ? 5 : 1;
        }
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-30");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-60");
        for (int i = 0; i < 3; ++i)
        {
            const double t = top * i / 2.0;
            char buf[16];
            if (t >= 1.0 || t == 0.0)
                std::snprintf(buf, sizeof(buf), "%g s", t);
            else
                std::snprintf(buf, sizeof(buf), "%d ms", int(std::floor(t * 1000.0 + 0.5)));
            label(fPX + fPW * float(i) / 2.0f, fPY + fPH + 11.0f, i == 0 ? ALIGN_LEFT : i == 1 ? ALIGN_CENTER : ALIGN_RIGHT,
                  8.0f, 0.0f, theme::kMuted, buf);
        }
        const float x0 = linX(float(pre), 0.0f, float(top));
        if (pre > 0.0)
            dashedV(x0);
        const float yb = fPY + fPH;
        beginPath();
        moveTo(x0, fPY);
        lineTo(linX(float(pre + r8), 0.0f, float(top)), yb);
        strokeColor(col(theme::kAccent, 0.45f));
        strokeWidth(2.0f);
        stroke();
        const float x1 = linX(float(pre + rt), 0.0f, float(top));
        const float xs[2] = { x0, x1 }, ys[2] = { fPY, yb };
        curve(xs, ys, 2, yb);

        static const char* const kNames[] = { "ROOM", "CHAMBER", "HALL", "GALAXY" };
        label(fVX + 26.0f, fVY + 4.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, kNames[std::min(3, std::max(0, p.type))], true);
        char st[48];
        std::snprintf(st, sizeof(st), "RT60 %.2f S   8 KHZ %.2f S", rt, r8);
        label(fVX + fVW - 8.0f, fVY + 4.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, st, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullspace::sliderValue(param, v);
        if (param == 5)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param >= 1)
            std::snprintf(buf, n, "%d%%", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    nullspace::SpaceProcessor fCalc; // only its parameter maths, no buffers
};

UI* createUI()
{
    return new UISpace();
}

END_NAMESPACE_DISTRHO
