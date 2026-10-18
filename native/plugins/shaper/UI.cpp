/*
 * NULL JSFX - Shaper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ShaperDSP.hpp"
#include "ShaperSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SHAPE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BIAS", 0, -1, true },
    { 5, NullKnobsUI::kKnob, "TONE", 0, -1, false, true },
    { 3, NullKnobsUI::kSelect, "SYMMETRY", 1, -1, false },
    { 4, NullKnobsUI::kSelect, "OVERSAMPLE", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, false },
};

static const char* const kShapeNames[] = { "CUSTOM", "SOFT", "HARD", "TUBE", "FOLD", "SINE", "STEPS", "ASYM" };

class UIShaper : public NullKnobsUI
{
public:
    UIShaper()
        : NullKnobsUI({ "SHAPER", "ESSENTIALS", 480, 200, nullptr, 2, nullshaper::kSliders, nullshaper::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the plot, as in shaper_gfx.jsfx: (52, 68, 300, 172), input -1..+1 across, output up
    static constexpr float kPX = 52.0f, kPY = 68.0f, kPW = 300.0f, kPH = 172.0f;
    static constexpr float kCX = kPX + kPW * 0.5f, kCY = kPY + kPH * 0.5f;

    double v(uint32_t i) const { return nullshaper::sliderValue(i, value(i)); }
    int shape() const { return std::max(0, std::min(7, int(std::lround(value(0))))); }

    // the handles: the P sliders for Custom, else the Shape's built-in points
    void handles(double* pts) const
    {
        const int s = shape();
        for (uint32_t i = 0; i < 16; ++i)
            pts[i] = s == 0 ? v(nullshaper::kFirstPoint + i) : nullshaper::kShapePoints[s][i];
    }

    // the handle within 7 units of (x, y), nearest first, or -1; n = the number of used points
    int handleAt(float x, float y, int& n) const
    {
        double pts[16];
        handles(pts);
        n = 0;
        int hit = -1;
        float best = 7.0f;
        for (int i = 0; i < 8; ++i)
        {
            if (!(pts[2 * i] > 0.0))
                continue;
            ++n;
            const float d = std::hypot(x - (kCX + kPW * 0.5f * float(pts[2 * i])), y - (kCY - kPH * 0.5f * float(pts[2 * i + 1])));
            if (d < best)
            {
                best = d;
                hit = i;
            }
        }
        return hit;
    }

    static float snapPt(float t, float lo) { return clampf(std::floor(t * 1000.0f + 0.5f) / 1000.0f, lo, 1.0f); }

    // the Shape's built-in points into the P sliders (sh_load)
    void loadPoints(int s)
    {
        for (uint32_t i = 0; i < 16; ++i)
            setGesture(nullshaper::kFirstPoint + i, float(nullshaper::kShapePoints[s][i]));
    }

    // before a point edit: Custom, starting from the current Shape (sh_custom)
    void makeCustom()
    {
        const int s = shape();
        if (s == 0)
            return;
        loadPoints(s);
        if (s == 7)
            setGesture(3, 1.0f);
        setGesture(0, 0.0f);
    }

    void drawDisplay() override
    {
        setPlot(kPX, kPY, kPW, kPH);
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

        float in = 0.0f, out = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            out = t->outLevel.load(std::memory_order_relaxed);
        }
        const float pk = std::fmin(1.0f, in);
        if (pk > 0.001f)
        {
            beginPath();
            for (float sg : { 1.0f, -1.0f })
            {
                const float x = std::round(linX(sg * pk, -1.0f, 1.0f)) + 0.5f;
                moveTo(x, fPY);
                lineTo(x, fPY + fPH);
            }
            strokeColor(col(theme::kAccent, 0.3f));
            strokeWidth(1.0f);
            stroke();
        }

        double pts[16];
        for (uint32_t i = 0; i < 16; ++i)
            pts[i] = v(nullshaper::kFirstPoint + i);
        fCurve.set(shape(), int(std::lround(value(3))), pts);
        float xs[301], ys[301];
        for (int i = 0; i <= 300; ++i)
        {
            xs[i] = fPX + fPW * float(i) / 300.0f;
            ys[i] = linY(float(fCurve(-1.0 + 2.0 * i / 300.0)), -1.0f, 1.0f);
        }
        curve(xs, ys, 301);

        // the anchor and the handles
        beginPath();
        circle(kCX, kCY, 2.0f);
        fillColor(col(theme::kText));
        fill();
        handles(pts);
        int n = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (!(pts[2 * i] > 0.0))
                continue;
            ++n;
            const float x = linX(float(pts[2 * i]), -1.0f, 1.0f), y = linY(float(pts[2 * i + 1]), -1.0f, 1.0f);
            beginPath();
            circle(x, y, 4.0f);
            fillColor(col(fPtDrag == i ? theme::kAccent : theme::kCard));
            fill();
            strokeColor(col(fPtDrag == i || fPtHover == i ? theme::kText : theme::kAccent));
            strokeWidth(1.0f);
            stroke();
        }

        // right column
        label(368.0f, 76.0f, ALIGN_LEFT, 7.0f, 1.5f, theme::kMuted, "SHAPE", true);
        label(368.0f, 112.0f, ALIGN_LEFT, 7.0f, 1.5f, theme::kMuted, "POINTS", true);
        label(368.0f, 91.0f, ALIGN_LEFT, 10.0f, 0.5f, theme::kText, kShapeNames[shape()], true);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d / 8", n);
        label(368.0f, 127.0f, ALIGN_LEFT, 10.0f, 0.5f, theme::kText, buf, true);
        label(368.0f, 150.0f, ALIGN_LEFT, 7.0f, 0.0f, theme::kMuted, "Drag a point to edit.");
        label(368.0f, 162.0f, ALIGN_LEFT, 7.0f, 0.0f, theme::kMuted, "Double-click: add or");
        label(368.0f, 174.0f, ALIGN_LEFT, 7.0f, 0.0f, theme::kMuted, "remove a point.");
        meter(368.0f, 204.0f, 80.0f, 8.0f, "IN", nulljsfx::toDb(in), -140.0f, -48.0f);
        meter(368.0f, 222.0f, 80.0f, 8.0f, "OUT", nulljsfx::toDb(out), -140.0f, -48.0f);
    }

    static bool insidePlot(float x, float y) { return x >= 46.0f && x <= 358.0f && y >= 62.0f && y <= 246.0f; }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && insidePlot(x, y))
        {
            int n = 0;
            const int hit = handleAt(x, y, n);
            if (fLastClick != 0 && ev.time - fLastClick < 300 && fLastHit == hit)
            {
                fLastClick = 0;
                if (hit >= 0 && n > 1)
                {
                    // remove the point
                    makeCustom();
                    setGesture(nullshaper::kFirstPoint + 2 * hit, 0.0f);
                    setGesture(nullshaper::kFirstPoint + 2 * hit + 1, 0.0f);
                }
                else if (hit < 0 && x > kCX && n < 8)
                {
                    // add one in the first free slot
                    makeCustom();
                    int k = 0;
                    while (k < 7 && v(nullshaper::kFirstPoint + 2 * k) > 0.0)
                        ++k;
                    setGesture(nullshaper::kFirstPoint + 2 * k, snapPt((x - kCX) / (kPW * 0.5f), 0.001f));
                    setGesture(nullshaper::kFirstPoint + 2 * k + 1, snapPt((kCY - y) / (kPH * 0.5f), -1.0f));
                }
                endDrag();
                return true;
            }
            fLastClick = ev.time;
            fLastHit = hit;
            if (hit >= 0)
            {
                makeCustom();
                fPtDrag = hit;
                editParameter(nullshaper::kFirstPoint + 2 * hit, true);
                editParameter(nullshaper::kFirstPoint + 2 * hit + 1, true);
                repaint();
            }
            return true;
        }
        if (ev.button == 1 && !ev.press && fPtDrag >= 0)
        {
            endDrag();
            return true;
        }
        // the Shape selector: a new Shape (other than Custom) loads its points into the P sliders
        const int before = shape();
        const bool r = NullKnobsUI::onMouse(ev);
        shapeChanged(before);
        return r;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (fPtDrag >= 0)
        {
            setFromUI(nullshaper::kFirstPoint + 2 * fPtDrag, snapPt((x - kCX) / (kPW * 0.5f), 0.001f));
            setFromUI(nullshaper::kFirstPoint + 2 * fPtDrag + 1, snapPt((kCY - y) / (kPH * 0.5f), -1.0f));
            return true;
        }
        int n = 0;
        const int h = insidePlot(x, y) && !presetOpen() ? handleAt(x, y, n) : -1;
        if (h != fPtHover)
        {
            fPtHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const int before = shape();
        const bool r = NullKnobsUI::onScroll(ev);
        shapeChanged(before);
        return r;
    }

    void shapeChanged(int before)
    {
        const int now = shape();
        if (now != before && now > 0)
            loadPoints(now);
    }

    void endDrag()
    {
        if (fPtDrag >= 0)
        {
            editParameter(nullshaper::kFirstPoint + 2 * fPtDrag, false);
            editParameter(nullshaper::kFirstPoint + 2 * fPtDrag + 1, false);
        }
        fPtDrag = -1;
        repaint();
    }

    void formatValue(uint32_t param, float val, char* buf, size_t n) override
    {
        const double x = nullshaper::sliderValue(param, val);
        if (param == 1)
            std::snprintf(buf, n, "%.1f dB", x);
        else if (param == 2)
        {
            if (x == 0.0)
                std::snprintf(buf, n, "0%%");
            else
                std::snprintf(buf, n, "%+d%%", int(std::lround(x)));
        }
        else if (param == 5)
        {
            if (x >= 20000.0)
                std::snprintf(buf, n, "OFF");
            else
                std::snprintf(buf, n, "%.1f kHz", x / 1000.0);
        }
        else if (param == 7)
        {
            if (std::fabs(x) < 0.05)
                std::snprintf(buf, n, "0.0 dB");
            else
                std::snprintf(buf, n, "%+.1f dB", x);
        }
        else
            NullKnobsUI::formatValue(param, val, buf, n);
    }

private:
    nullshaper::Curve fCurve;
    int fPtDrag = -1, fPtHover = -1, fLastHit = -2;
    uint fLastClick = 0;
};

UI* createUI()
{
    return new UIShaper();
}

END_NAMESPACE_DISTRHO
