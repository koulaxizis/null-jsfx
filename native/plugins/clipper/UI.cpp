/*
 * NULL JSFX - Clipper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ClipperDSP.hpp"
#include "ClipperSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "INPUT", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "CEILING", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "KNEE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "OVERSAMPLE", 1, -1, false },
    { 6, NullKnobsUI::kSelect, "LINK", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "OUTPUT", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
};

class UIClipper : public NullKnobsUI
{
public:
    UIClipper()
        : NullKnobsUI({ "CLIPPER", "ESSENTIALS", 480, 120, nullptr, 2, nullclipper::kSliders, nullclipper::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    double v(uint32_t i) const { return nullclipper::sliderValue(i, value(i)); }

    // as in clipper_gfx.jsfx: left, the static curve (input -1..+1 across, output -1..+1 up);
    // right, input (after Input, Ceiling marked), clip gain-reduction and output meters
    void drawDisplay() override
    {
        setPlot(64.0f, 66.0f, 92.0f, 92.0f);
        grid(4, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+1");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-1");

        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        nullclipper::Params p;
        p.input = v(0);
        p.ceiling = v(1);
        p.knee = v(2);
        p.mix = v(4);
        p.output = v(5);
        nullclipper::Curve c;
        c.set(p);
        float xs[93], ys[93];
        for (int i = 0; i < 93; ++i)
        {
            xs[i] = fPX + fPW * float(i) / 92.0f;
            ys[i] = linY(float(c(-1.0 + 2.0 * i / 92.0)), -1.0f, 1.0f);
        }
        curve(xs, ys, 93);

        float in = 0.0f, out = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        meter(180.0f, 82.0f, 268.0f, 10.0f, "IN", nulljsfx::toDb(in), value(1), -48.0f);
        grMeter(180.0f, 106.0f, 268.0f, 10.0f, "GR", gr, 12.0f);
        meter(180.0f, 130.0f, 268.0f, 10.0f, "OUT", nulljsfx::toDb(out), value(1), -48.0f);
    }

    void formatValue(uint32_t param, float val, char* buf, size_t n) override
    {
        const double x = nullclipper::sliderValue(param, val);
        if (param == 0 || param == 5)
        {
            if (std::fabs(x) < 0.05)
                std::snprintf(buf, n, "0.0 dB");
            else
                std::snprintf(buf, n, "%+.1f dB", x);
        }
        else if (param == 1)
            std::snprintf(buf, n, "%.1f dB", x);
        else
            NullKnobsUI::formatValue(param, val, buf, n);
    }
};

UI* createUI()
{
    return new UIClipper();
}

END_NAMESPACE_DISTRHO
