/*
 * NULL JSFX - Dynamics (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DynamicsDSP.hpp"
#include "DynamicsSliders.hpp"

START_NAMESPACE_DISTRHO

// COMPRESSOR tab, then KEY / CHANNELS, as in dynamics_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODE", 0, 0, false },
    { 1, NullKnobsUI::kKnob, "THRESHOLD", 0, 0, false },
    { 2, NullKnobsUI::kKnob, "RATIO", 0, 0, false },
    { 3, NullKnobsUI::kKnob, "KNEE", 0, 0, false },
    { 4, NullKnobsUI::kKnob, "ATTACK", 0, 0, false },
    { 5, NullKnobsUI::kKnob, "RELEASE", 0, 0, false },
    { 7, NullKnobsUI::kSelect, "STYLE", 1, 0, false },
    { 6, NullKnobsUI::kKnob, "MAKEUP", 1, 0, false },
    { 9, NullKnobsUI::kKnob, "MIX", 1, 0, false },
    { 12, NullKnobsUI::kSelect, "LISTEN", 1, 0, false },
    { 8, NullKnobsUI::kKnob, "SC HPF", 0, 1, false, true },
    { 10, NullKnobsUI::kSelect, "SIDECHAIN", 0, 1, false },
    { 11, NullKnobsUI::kSelect, "CHANNELS", 0, 1, false },
};

class UIDynamics : public NullKnobsUI
{
public:
    UIDynamics()
        : NullKnobsUI({ "DYNAMICS", "ESSENTIALS", 480, 120, "COMPRESSOR|KEY / CHANNELS", 2, nulldynamics::kSliders, nulldynamics::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in dynamics_gfx.jsfx: left, the static curve (input -60..0 dB across, output -60..0 dB
    // up) with the input level as a dot; right, input, gain-reduction and output meters
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

        nulldynamics::Params p;
        p.expander = value(0) >= 0.5f;
        p.thresholdDb = value(1);
        p.ratio = value(2);
        p.kneeDb = value(3);
        p.makeupDb = value(6);
        p.style = int(std::lround(value(7)));
        nulldynamics::computeStyle(p);
        constexpr int n = 93;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(nulldynamics::curveDb(p, -60.0 + 60.0 * double(i) / double(n - 1))), -60.0f, 0.0f);
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
            circle(linX(float(d), -60.0f, 0.0f), linY(float(nulldynamics::curveDb(p, d)), -60.0f, 0.0f), 2.5f);
            fillColor(col(theme::kText));
            fill();
        }
        // status line: the style, plus what is switched on (detector high-pass, mix, external key)
        static const char* const kStyles[] = { "CLEAN", "BUS", "FET", "OPTO", "UPWARD" };
        label(180.0f, 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, kStyles[std::min(4, std::max(0, p.style))], true);
        char a[24] = "", b[24] = "", st[96];
        const bool ext = value(10) > 0.5f;
        if (value(8) > 20.0f)
            std::snprintf(a, sizeof(a), "   HPF %d HZ", int(std::lround(value(8))));
        if (value(9) < 100.0f)
            std::snprintf(b, sizeof(b), "   MIX %d%%", int(std::lround(value(9))));
        const int ch = int(std::lround(value(11)));
        std::snprintf(st, sizeof(st), "%s%s%s%s%s", a, b, ext ? "   EXT KEY" : "", ch == 1 ? "   MID" : ch == 2 ? "   SIDE" : "",
                      value(12) >= 0.5f ? "   DELTA" : "");
        label(448.0f, 66.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, st, true);
        meter(180.0f, 82.0f, 268.0f, 10.0f, ext ? "KEY" : "IN", db, value(1), -60.0f);
        // Upward: the lift (gain above 1) on the same meter, as LIFT
        grMeter(180.0f, 106.0f, 268.0f, 10.0f, p.style == nulldynamics::kUpward ? "LIFT" : "GR", std::fabs(gr), 24.0f);
        meter(180.0f, 130.0f, 268.0f, 10.0f, "OUT", nulljsfx::toDb(out), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2)
            std::snprintf(buf, n, "%.1f:1", v);
        else if (param == 6)
            std::snprintf(buf, n, "+%d dB", int(std::lround(v)));
        else if (param == 8 && v <= 20.0f)
            std::snprintf(buf, n, "OFF");
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDynamics();
}

END_NAMESPACE_DISTRHO
