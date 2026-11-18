/*
 * NULL JSFX - Step Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StepFilterDSP.hpp"
#include "StepFilterSliders.hpp"

START_NAMESPACE_DISTRHO

// as step_filter_gfx.jsfx: tab FILTER (low, high, resonance, type; steps, note, glide, seed, mix)
// and tabs STEPS 1-8 and STEPS 9-16, four knobs per row
static const NullKnobsUI::Control kControls[] = {
    { 18, NullKnobsUI::kKnob, "LOW", 0, 0, false, true },
    { 19, NullKnobsUI::kKnob, "HIGH", 0, 0, false, true },
    { 20, NullKnobsUI::kKnob, "RESONANCE", 0, 0, false },
    { 21, NullKnobsUI::kSelect, "TYPE", 0, 0, false },
    { 16, NullKnobsUI::kKnob, "STEPS", 1, 0, false },
    { 17, NullKnobsUI::kSelect, "NOTE", 1, 0, false },
    { 22, NullKnobsUI::kKnob, "GLIDE", 1, 0, false },
    { 23, NullKnobsUI::kKnob, "SEED", 1, 0, false },
    { 24, NullKnobsUI::kKnob, "MIX", 1, 0, false },
    { 0, NullKnobsUI::kKnob, "STEP 1", 0, 1, false },
    { 1, NullKnobsUI::kKnob, "STEP 2", 0, 1, false },
    { 2, NullKnobsUI::kKnob, "STEP 3", 0, 1, false },
    { 3, NullKnobsUI::kKnob, "STEP 4", 0, 1, false },
    { 4, NullKnobsUI::kKnob, "STEP 5", 1, 1, false },
    { 5, NullKnobsUI::kKnob, "STEP 6", 1, 1, false },
    { 6, NullKnobsUI::kKnob, "STEP 7", 1, 1, false },
    { 7, NullKnobsUI::kKnob, "STEP 8", 1, 1, false },
    { 8, NullKnobsUI::kKnob, "STEP 9", 0, 2, false },
    { 9, NullKnobsUI::kKnob, "STEP 10", 0, 2, false },
    { 10, NullKnobsUI::kKnob, "STEP 11", 0, 2, false },
    { 11, NullKnobsUI::kKnob, "STEP 12", 0, 2, false },
    { 12, NullKnobsUI::kKnob, "STEP 13", 1, 2, false },
    { 13, NullKnobsUI::kKnob, "STEP 14", 1, 2, false },
    { 14, NullKnobsUI::kKnob, "STEP 15", 1, 2, false },
    { 15, NullKnobsUI::kKnob, "STEP 16", 1, 2, false },
};

class UIStepFilter : public NullKnobsUI
{
public:
    UIStepFilter()
        : NullKnobsUI({ "STEP FILTER", "ESSENTIALS", 480, 120, "FILTER|STEPS 1-8|STEPS 9-16", 2, nullstepfilter::kSliders,
                        nullstepfilter::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the plot rectangle of the display (design units), as in step_filter_gfx.jsfx
    float plotX() const { return 24.0f + 26.0f; }
    float plotY() const { return 62.0f + 4.0f; }
    float plotW() const { return float(kWidth) - 48.0f - 30.0f; }
    float plotH() const { return 120.0f - 16.0f - 20.0f; }

    static void hzShort(char* buf, size_t n, double hz)
    {
        if (hz >= 1000.0)
            std::snprintf(buf, n, "%.1fk", hz / 1000.0);
        else
            std::snprintf(buf, n, "%d", int(hz));
    }

    // as in step_filter_gfx.jsfx: one bar per step (its place between Low and High), the steps past
    // Steps dimmed, the playing step marked below, and the cutoff's path with Glide on top
    void drawDisplay() override
    {
        setPlot(plotX(), plotY(), plotW(), plotH());
        const nullstepfilter::Params p = nullstepfilter::paramsFrom([this](uint32_t i) { return value(i); });
        char buf[16];
        hzShort(buf, sizeof(buf), p.highHz);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        hzShort(buf, sizeof(buf), p.lowHz);
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        int cur = -1;
        if (nulljsfx::Taps* t = taps())
            cur = int(std::lround(t->extra.load(std::memory_order_relaxed)));
        double sv[16];
        nullstepfilter::stepValues(p, sv);
        const int n = nullstepfilter::clampSteps(p.numSteps);
        const float cw = fPW / 16.0f;
        for (int k = 0; k < 16; ++k)
        {
            const float x0 = fPX + float(k) * cw + 1.0f, w = cw - 2.0f;
            beginPath();
            rect(x0, fPY, w, fPH);
            fillColor(col(k == cur ? theme::kBorder : theme::kBackground));
            fill();
            const float h = float(sv[k]) * fPH;
            beginPath();
            rect(x0, fPY + fPH - h, w, h);
            fillColor(col(theme::kAccent, k >= n ? 0.08f : k == cur ? 0.45f : 0.25f));
            fill();
            if (k == cur)
            {
                beginPath();
                rect(x0, fPY + fPH + 2.0f, w, 2.0f);
                fillColor(col(theme::kText));
                fill();
            }
            if (k % 4 == 0)
            {
                std::snprintf(buf, sizeof(buf), "%d", k + 1);
                label(x0 + w * 0.5f, fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
            }
        }

        // the cutoff's path over the steps that play: Glide from the previous value, then held
        const double glide = p.glidePct / 100.0;
        float xs[16 * 10 + 2], ys[16 * 10 + 2];
        int m = 0;
        for (int k = 0; k < n; ++k)
        {
            const double vp = sv[k > 0 ? k - 1 : n - 1];
            xs[m] = fPX + float(k) * cw;
            ys[m] = linY(float(glide > 0.0 ? vp : (k == 0 ? sv[0] : sv[k - 1])), 0.0f, 1.0f);
            ++m;
            if (glide > 0.0)
                for (int i = 1; i <= 8; ++i)
                {
                    xs[m] = fPX + float((k + glide * i / 8.0) * cw);
                    ys[m] = linY(float(vp + (sv[k] - vp) * (0.5 - 0.5 * std::cos(3.141592653589793 * i / 8.0))), 0.0f, 1.0f);
                    ++m;
                }
            else
            {
                xs[m] = fPX + float(k) * cw;
                ys[m] = linY(float(sv[k]), 0.0f, 1.0f);
                ++m;
            }
            xs[m] = fPX + float(k + 1) * cw;
            ys[m] = linY(float(sv[k]), 0.0f, 1.0f);
            ++m;
        }
        curve(xs, ys, m);
    }

    // with Seed 0, a click in a step's bar sets it to the height clicked
    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        if (ev.button == 1 && ev.press && value(23) < 0.5f)
        {
            const float s = uiScale();
            const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
            const float px = plotX(), py = plotY(), cw = plotW() / 16.0f;
            if (x >= px && x < px + plotW() && y >= py && y < py + plotH())
            {
                const int k = std::min(15, int((x - px) / cw));
                const float x0 = px + float(k) * cw + 1.0f;
                if (x >= x0 && x < x0 + cw - 2.0f)
                {
                    const float v = std::floor((py + plotH() - y) / plotH() * 100.0f + 0.5f);
                    setGesture(uint32_t(k), std::max(0.0f, std::min(100.0f, v)));
                    return true;
                }
            }
        }
        return NullKnobsUI::onMouse(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 18 || param == 19)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 23 && std::lround(v) <= 0)
            std::snprintf(buf, n, "OFF");
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIStepFilter();
}

END_NAMESPACE_DISTRHO
