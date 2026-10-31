/*
 * NULL JSFX - Glue Comp (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "GlueCompDSP.hpp"
#include "GlueCompSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in glue_comp_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "RATIO", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "ATTACK", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "RELEASE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MAKEUP", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "SC HPF", 1, -1, false },
};

class UIGlueComp : public NullKnobsUI
{
public:
    UIGlueComp()
        : NullKnobsUI({ "GLUE COMP", "ESSENTIALS", 480, 120, nullptr, 2, nullgluecomp::kUiSliders,
                        nullgluecomp::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in glue_comp_gfx.jsfx: left, the static curve (input -60..0 dB across, output -60..0 dB
    // up) with the detector level as a dot; right, detector, gain-reduction and output meters
    void drawDisplay() override
    {
        setPlot(64.0f, 66.0f, 92.0f, 92.0f);
        grid(4, 4, false);
        char buf[8];
        for (int i = 0; i < 3; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", -30 * i);
            label(fPX - 6.0f, fPY + fPH * float(i) / 2.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        beginPath();
        moveTo(fPX, fPY + fPH);
        lineTo(fPX + fPW, fPY);
        strokeColor(col(theme::kAccent, 0.25f));
        strokeWidth(1.0f);
        stroke();

        nullgluecomp::Params p;
        p.threshold = nullgluecomp::sliderValue(0, value(0));
        p.ratio = int(nullgluecomp::sliderValue(1, value(1)));
        p.release = int(nullgluecomp::sliderValue(3, value(3)));
        p.makeup = nullgluecomp::sliderValue(4, value(4));
        p.mix = nullgluecomp::sliderValue(5, value(5));
        p.hpf = nullgluecomp::sliderValue(6, value(6));
        const nullgluecomp::Coeffs c = nullgluecomp::coeffsFor(p, 48000.0);
        constexpr int n = 93;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nullgluecomp::curveDb(c, p.makeup, -60.0 + 60.0 * double(i) / double(n - 1))), -60.0f, 0.0f);
        }
        curve(xs, ys, n);

        float in = 0.0f, out = 0.0f, gr = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
            gr = t->gr.load(std::memory_order_relaxed);
        }
        const float db = nulljsfx::toDb(in);
        if (db > -60.0f)
        {
            const double d = std::fmin(0.0, double(db));
            beginPath();
            circle(linX(float(d), -60.0f, 0.0f), linY(float(nullgluecomp::curveDb(c, p.makeup, d)), -60.0f, 0.0f), 2.5f);
            fillColor(col(theme::kText));
            fill();
        }
        // status line: ratio and release, plus what is switched on (detector high-pass, mix)
        static const char* const kRatios[] = { "2:1", "4:1", "10:1" };
        char st[48];
        std::snprintf(st, sizeof(st), "%s   %s", kRatios[std::min(2, std::max(0, p.ratio))],
                      c.relAuto ? "AUTO RELEASE" : "FIXED RELEASE");
        label(180.0f, 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, st, true);
        char a[24] = "", b[24] = "", st2[64];
        if (p.hpf > 0.0)
            std::snprintf(a, sizeof(a), "   HPF %d HZ", int(std::lround(p.hpf)));
        if (p.mix < 100.0)
            std::snprintf(b, sizeof(b), "   MIX %d%%", int(std::lround(p.mix)));
        std::snprintf(st2, sizeof(st2), "%s%s", a, b);
        label(448.0f, 66.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, st2, true);
        meter(180.0f, 82.0f, 268.0f, 10.0f, "IN", db, float(p.threshold), -60.0f);
        grMeter(180.0f, 106.0f, 268.0f, 10.0f, "GR", std::fabs(gr), 24.0f);
        meter(180.0f, 130.0f, 268.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullgluecomp::sliderValue(param, v);
        if (param == 0)
            std::snprintf(buf, n, "%.1f dB", x);
        else if (param == 4)
            std::snprintf(buf, n, "+%.1f dB", x);
        else if (param == 6 && x <= 0.0)
            std::snprintf(buf, n, "OFF");
        else if (param == 6)
            std::snprintf(buf, n, "%d Hz", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIGlueComp();
}

END_NAMESPACE_DISTRHO
