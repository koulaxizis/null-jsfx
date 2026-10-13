/*
 * NULL JSFX - Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "FilterDSP.hpp"
#include "FilterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "CUTOFF", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "RESONANCE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "GAIN", 0, -1, true },
    { 3, NullKnobsUI::kSelect, "MODE", 0, -1, false },
};

class UIFilter : public NullKnobsUI
{
public:
    UIFilter()
        : NullKnobsUI({ "FILTER", "ESSENTIALS", 480, 120, nullptr, 1, nullfilter::kSliders, nullfilter::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // ---- log taper for the Cutoff knob (control 0), as fl_knob_log in filter_gfx.jsfx ----------

    static constexpr float kLo = 20.0f, kHi = 20000.0f, kDef = 1000.0f;
    static float toNorm(float hz) { return std::log(hz / kLo) / std::log(kHi / kLo); }
    static float fromNorm(float n) { return clampf(std::floor(kLo * std::exp(n * std::log(kHi / kLo)) + 0.5f), kLo, kHi); }

    void onNanoDisplay() override
    {
        NullKnobsUI::onNanoDisplay();
        // the base drew the Cutoff knob on a linear scale; paint over it with the log one
        // (the base's scale(uiScale()) is still in effect)
        float cx, ry;
        cell(0, cx, ry);
        beginPath();
        rect(cx - 28.0f, ry + 17.0f, 56.0f, 54.0f);
        fillColor(col(theme::kCard));
        fill();
        drawKnob(cx, ry + 44.0f, 14.0f, fDragging ? fNorm : toNorm(value(0)), false, fDragging || fHover);
        drawPresetMenu(); // again, over the knob
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && controlAt(x, y) == 0)
        {
            if (fLastClick != 0 && ev.time - fLastClick < 350)
            {
                fLastClick = 0;
                setGesture(0, kDef);
                return true;
            }
            fLastClick = ev.time;
            fDragging = true;
            fNorm = toNorm(value(0));
            fX = x;
            fY = y;
            editParameter(0, true);
            repaint();
            return true;
        }
        if (ev.button == 1 && !ev.press && fDragging)
        {
            fDragging = false;
            editParameter(0, false);
            repaint();
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        const bool h = controlAt(x, y) == 0;
        if (h != fHover)
        {
            fHover = h;
            repaint();
        }
        if (!fDragging)
            return NullKnobsUI::onMotion(ev);
        const bool fine = (ev.mod & (kModifierControl | kModifierShift)) != 0;
        fNorm = clampf(fNorm + ((x - fX) + (fY - y)) / 200.0f * (fine ? 0.1f : 1.0f), 0.0f, 1.0f);
        fX = x;
        fY = y;
        setFromUI(0, fromNorm(fNorm));
        repaint();
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        if (presetOpen())
            return true;
        const float s = uiScale();
        if (controlAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s) == 0)
        {
            const float d = float(ev.delta.getY()) + float(ev.delta.getX());
            if (d == 0.0f)
                return false;
            setGesture(0, fromNorm(clampf(toNorm(value(0)) + (d > 0.0f ? 0.01f : -0.01f), 0.0f, 1.0f)));
            return true;
        }
        return NullKnobsUI::onScroll(ev);
    }

    // ---- display -------------------------------------------------------------------------------

    // as in filter_gfx.jsfx: the filter's magnitude response with Gain (-24..+24 dB), cutoff dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);
        dashedV(freqX(value(0)));

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nullfilter::Coefs k;
        k.compute(value(0), value(1), value(2), int(std::lround(value(3))), sr);
        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(k.magnitudeDb(f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 1)
            std::snprintf(buf, n, "%.1f", double(v));
        else if (param == 2)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    bool fDragging = false, fHover = false;
    float fNorm = 0.0f, fX = 0.0f, fY = 0.0f;
    uint fLastClick = 0;
};

UI* createUI()
{
    return new UIFilter();
}

END_NAMESPACE_DISTRHO
