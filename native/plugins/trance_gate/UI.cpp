/*
 * NULL JSFX - Trance Gate (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TranceGateDSP.hpp"
#include "TranceGateSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    // GATE tab
    { 16, NullKnobsUI::kSelect, "MODE", 0, 0, false },
    { 17, NullKnobsUI::kSelect, "NOTE", 0, 0, false },
    { 18, NullKnobsUI::kKnob, "FREE RATE", 0, 0, false },
    { 19, NullKnobsUI::kKnob, "ATTACK", 1, 0, false },
    { 20, NullKnobsUI::kKnob, "RELEASE", 1, 0, false },
    { 21, NullKnobsUI::kKnob, "DEPTH", 1, 0, false },
    // STEPS 1-8 and STEPS 9-16 tabs, four per row
    { 0, NullKnobsUI::kSelect, "STEP 1", 0, 1, false },
    { 1, NullKnobsUI::kSelect, "STEP 2", 0, 1, false },
    { 2, NullKnobsUI::kSelect, "STEP 3", 0, 1, false },
    { 3, NullKnobsUI::kSelect, "STEP 4", 0, 1, false },
    { 4, NullKnobsUI::kSelect, "STEP 5", 1, 1, false },
    { 5, NullKnobsUI::kSelect, "STEP 6", 1, 1, false },
    { 6, NullKnobsUI::kSelect, "STEP 7", 1, 1, false },
    { 7, NullKnobsUI::kSelect, "STEP 8", 1, 1, false },
    { 8, NullKnobsUI::kSelect, "STEP 9", 0, 2, false },
    { 9, NullKnobsUI::kSelect, "STEP 10", 0, 2, false },
    { 10, NullKnobsUI::kSelect, "STEP 11", 0, 2, false },
    { 11, NullKnobsUI::kSelect, "STEP 12", 0, 2, false },
    { 12, NullKnobsUI::kSelect, "STEP 13", 1, 2, false },
    { 13, NullKnobsUI::kSelect, "STEP 14", 1, 2, false },
    { 14, NullKnobsUI::kSelect, "STEP 15", 1, 2, false },
    { 15, NullKnobsUI::kSelect, "STEP 16", 1, 2, false },
};

class UITranceGate : public NullKnobsUI
{
public:
    UITranceGate()
        : NullKnobsUI({ "TRANCE GATE", "ADVANCED", 480, 120, "GATE|STEPS 1-8|STEPS 9-16", 2, nulltrance::kSliders,
                        nulltrance::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the plot rectangle of the display (design units), as in trance_gate_gfx.jsfx
    float plotX() const { return 24.0f + 26.0f; }
    float plotY() const { return 62.0f + 4.0f; }
    float plotW() const { return float(kWidth) - 48.0f - 30.0f; }
    float plotH() const { return 120.0f - 16.0f - 20.0f; }

    // as in trance_gate_gfx.jsfx: the 16 steps (click one to switch it), the playing step
    // marked below, and the gain over the pattern as Attack, Release and Depth shape it
    void drawDisplay() override
    {
        setPlot(plotX(), plotY(), plotW(), plotH());
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "100%");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "50%");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");

        int cur = -1;
        double bpm = 120.0;
        if (nulljsfx::Taps* t = taps())
        {
            cur = int(std::lround(t->extra.load(std::memory_order_relaxed)));
            const float tp = t->gr.load(std::memory_order_relaxed);
            if (tp > 0.0f)
                bpm = tp;
        }
        const nulltrance::Params p = nulltrance::paramsFrom([this](uint32_t i) { return value(i); });
        const float cw = fPW / 16.0f;
        char buf[8];
        for (int k = 0; k < 16; ++k)
        {
            const float x0 = fPX + float(k) * cw + 1.0f, w = cw - 2.0f;
            beginPath();
            rect(x0, fPY, w, fPH);
            fillColor(p.steps[k] ? col(theme::kAccent, k == cur ? 0.40f : 0.22f)
                                 : col(k == cur ? theme::kBorder : theme::kBackground));
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

        const double step = p.freeMode ? 1.0 / p.freeRate : 60.0 / bpm / std::pow(2.0, double(p.stepLength));
        double ts[64], gs[64];
        const int n = nulltrance::displayEnvelope(p, step, ts, gs);
        float xs[64], ys[64];
        for (int i = 0; i < n; ++i)
        {
            xs[i] = fPX + fPW * float(ts[i] / 16.0);
            ys[i] = linY(float(gs[i]), 0.0f, 1.0f);
        }
        curve(xs, ys, n);
    }

    // a click on a step in the display switches it
    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button == 1 && ev.press)
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
                    setGesture(uint32_t(k), value(uint32_t(k)) >= 0.5f ? 0.0f : 1.0f);
                    return true;
                }
            }
        }
        return NullKnobsUI::onMouse(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 18)
            std::snprintf(buf, n, "%.1f Hz", nulltrance::tenths(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UITranceGate();
}

END_NAMESPACE_DISTRHO
