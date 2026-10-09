/*
 * NULL Tilt Native - DPF plugin UI (DGL + NanoVG)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "DistrhoUI.hpp"
#include "Theme.hpp"
#include "TiltDSP.hpp"

#include <cmath>
#include <cstdio>

START_NAMESPACE_DISTRHO

namespace {

constexpr uint kWidth  = 360;
constexpr uint kHeight = 240;

constexpr float kMinAmount = -100.0f;
constexpr float kMaxAmount = 100.0f;

// Layout (1x coordinates), the same as the GFX JSFX
constexpr float kKnobCardX = 14.0f, kKnobCardY = 56.0f, kKnobCardW = 136.0f, kKnobCardH = 170.0f;
constexpr float kKnobCX = 82.0f, kKnobCY = 132.0f, kKnobR = 30.0f;
constexpr float kGraphX = 160.0f, kGraphY = 56.0f, kGraphW = 186.0f, kGraphH = 170.0f;
constexpr float kPlotX = 186.0f, kPlotY = 70.0f, kPlotW = 150.0f, kPlotH = 130.0f;

constexpr float kFMin = 20.0f, kFMax = 20000.0f, kDbRange = 8.0f;

constexpr float kAngleStart = 0.75f * float(nulltilt::kPi);  // 135 deg (lower-left)
constexpr float kAngleSweep = 1.5f * float(nulltilt::kPi);   // 270 deg

inline Color col(uint32_t rgb, float alpha = 1.0f)
{
    return Color(int((rgb >> 16) & 0xff), int((rgb >> 8) & 0xff), int(rgb & 0xff), alpha);
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

class UITilt : public UI
{
public:
    UITilt()
        : UI(kWidth, kHeight, false)
    {
        // We scale drawing and mouse input ourselves (onNanoDisplay, uiScale) instead of DPF's
        // automatic scaling, which stretches a 1x frame and blurs the text when resized.
        const double sf = getScaleFactor();
        setGeometryConstraints(uint(kWidth * sf), uint(kHeight * sf), true, false);
        if (sf != 1.0)
            setSize(uint(kWidth * sf), uint(kHeight * sf));
        loadSharedResources();
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // DSP/Plugin callbacks

    void parameterChanged(uint32_t index, float value) override
    {
        if (index != 0)
            return;
        fAmount = value;
        repaint();
    }

    void sampleRateChanged(double) override
    {
        repaint();
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Drawing

    void onNanoDisplay() override
    {
        const float s = float(getWidth()) / float(kWidth);
        scale(s, s);

        beginPath();
        rect(0, 0, kWidth, kHeight);
        fillColor(col(theme::kBackground));
        fill();

        fontFace(NANOVG_DEJAVU_SANS_TTF);

        drawHeader("TILT", "ONE SLIDER");
        drawKnobCard();
        drawGraph();
    }

    // text with letter spacing; bold is faked with a second pass shifted by a fraction of a pixel
    void label(float x, float y, int align, float size, float spacing, uint32_t rgb, const char* str, bool bold = false)
    {
        fontSize(size);
        textLetterSpacing(spacing);
        textAlign(align | ALIGN_MIDDLE);
        fillColor(col(rgb));
        text(x, y, str, nullptr);
        if (bold)
            text(x + 0.35f, y, str, nullptr);
        textLetterSpacing(0.0f);
    }

    void drawLogo(float x, float y, float size)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, size - 1.0f, size - 1.0f, size * 0.15f);
        fillColor(col(theme::kLogoTile));
        fill();
        strokeColor(col(theme::kLogoEdge));
        strokeWidth(1.0f);
        stroke();

        beginPath();
        moveTo(x + size * 0.30f, y + size * 0.75f);
        lineTo(x + size * 0.30f, y + size * 0.25f);
        lineTo(x + size * 0.70f, y + size * 0.75f);
        lineTo(x + size * 0.70f, y + size * 0.25f);
        strokePaint(linearGradient(x, y + size * 0.75f, x, y + size * 0.25f, col(theme::kIconLo), col(theme::kIconHi)));
        strokeWidth(size * 0.075f);
        lineJoin(ROUND);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);
    }

    void drawHeader(const char* name, const char* category)
    {
        drawLogo(14.0f, 10.0f, 24.0f);
        label(46.0f, 15.0f, ALIGN_LEFT, 8.5f, 2.5f, theme::kMuted, "NULL JSFX", true);
        label(46.0f, 28.0f, ALIGN_LEFT, 15.0f, 2.0f, theme::kText, name, true);
        label(kWidth - 14.0f, 22.0f, ALIGN_RIGHT, 8.5f, 2.0f, theme::kMuted, category, true);

        beginPath();
        moveTo(0, 44.5f);
        lineTo(kWidth, 44.5f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
    }

    void card(float x, float y, float w, float h)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 6.0f);
        fillColor(col(theme::kCard));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
    }

    void drawKnobCard()
    {
        card(kKnobCardX, kKnobCardY, kKnobCardW, kKnobCardH);
        label(kKnobCX, 70.0f, ALIGN_CENTER, 8.5f, 2.0f, theme::kMuted, "AMOUNT", true);

        // angles: 0 = up, clockwise, +-135 deg; NanoVG measures from +x, so subtract 90 deg
        const float amax = 0.75f * float(nulltilt::kPi);
        const float aVal = (fAmount / kMaxAmount) * amax;
        const float q = 0.5f * float(nulltilt::kPi);
        const float tr = kKnobR + 10.0f;

        beginPath();
        arc(kKnobCX, kKnobCY, tr, -amax - q, amax - q, CW);
        strokeColor(col(fDragging || fHover ? theme::kTrackHover : theme::kBorder));
        strokeWidth(4.0f);
        stroke();

        if (std::fabs(fAmount) > 0.05f)
        {
            beginPath();
            arc(kKnobCX, kKnobCY, tr, std::fmin(0.0f, aVal) - q, std::fmax(0.0f, aVal) - q, CW);
            strokeColor(col(theme::kAccent));
            strokeWidth(4.0f);
            stroke();
        }

        // centre tick
        beginPath();
        moveTo(kKnobCX, kKnobCY - tr - 5.0f);
        lineTo(kKnobCX, kKnobCY - tr - 8.0f);
        strokeColor(col(theme::kMuted));
        strokeWidth(1.0f);
        stroke();

        // body: shadow, grey-gradient rim like the site icons, dark cap
        beginPath();
        circle(kKnobCX, kKnobCY + 2.0f, kKnobR + 1.5f);
        fillColor(Color(0, 0, 0, 0.35f));
        fill();

        beginPath();
        circle(kKnobCX, kKnobCY, kKnobR);
        fillPaint(linearGradient(kKnobCX, kKnobCY + kKnobR, kKnobCX, kKnobCY - kKnobR,
                                 col(theme::kIconLo), col(theme::kIconHi)));
        fill();

        beginPath();
        circle(kKnobCX, kKnobCY, kKnobR - 3.0f);
        fillPaint(radialGradient(kKnobCX, kKnobCY - 3.0f, 2.0f, kKnobR - 3.0f,
                                 col(theme::kCapTop), col(theme::kCard)));
        fill();

        // pointer
        const float sa = std::sin(aVal), ca = std::cos(aVal);
        beginPath();
        moveTo(kKnobCX + sa * kKnobR * 0.30f, kKnobCY - ca * kKnobR * 0.30f);
        lineTo(kKnobCX + sa * (kKnobR - 7.0f), kKnobCY - ca * (kKnobR - 7.0f));
        strokeColor(col(theme::kText));
        strokeWidth(2.8f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);

        label(kKnobCX - 32.0f, kKnobCY + 36.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted, "DARK", true);
        label(kKnobCX + 32.0f, kKnobCY + 36.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted, "BRIGHT", true);

        // value readout
        char buf[48];
        const int v = int(std::lround(fAmount));
        if (v > 0)
            std::snprintf(buf, sizeof(buf), "+%d", v);
        else
            std::snprintf(buf, sizeof(buf), "%d", v);
        label(kKnobCX, 196.0f, ALIGN_CENTER, 23.0f, 0.0f, theme::kText, buf, true);

        const double hiDb = fAmount / 100.0 * nulltilt::kMaxDb;
        if (v == 0)
            std::snprintf(buf, sizeof(buf), "FLAT");
        else
            std::snprintf(buf, sizeof(buf), "LOW %+.1f  HIGH %+.1f dB", -hiDb, hiDb);
        label(kKnobCX, 214.0f, ALIGN_CENTER, 8.0f, 0.3f, theme::kMuted, buf);
    }

    static float freqToX(float f)
    {
        return kPlotX + kPlotW * std::log(f / kFMin) / std::log(kFMax / kFMin);
    }

    static float dbToY(float db)
    {
        return kPlotY + kPlotH * 0.5f * (1.0f - clampf(db, -kDbRange, kDbRange) / kDbRange);
    }

    void drawGraph()
    {
        card(kGraphX, kGraphY, kGraphW, kGraphH);

        // frequency grid: 1..9 x decade, decades brighter
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * d;
                    if (f < kFMin || f > kFMax || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::round(freqToX(f)) + 0.5f;
                    moveTo(x, kPlotY);
                    lineTo(x, kPlotY + kPlotH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }

        // dB grid and labels
        for (int db = -6; db <= 6; db += 3)
        {
            const float y = std::round(dbToY(float(db))) + 0.5f;
            beginPath();
            moveTo(kPlotX, y);
            lineTo(kPlotX + kPlotW, y);
            strokeColor(db == 0 ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
            if (db % 6 == 0)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), db > 0 ? "+%d" : "%d", db);
                label(kPlotX - 5.0f, dbToY(float(db)), ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
            }
        }
        label(freqToX(100.0f), 212.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqToX(1000.0f), 212.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqToX(10000.0f), 212.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");

        // 700 Hz pivot (dashed)
        {
            const float x = std::round(freqToX(float(nulltilt::kPivotHz))) + 0.5f;
            beginPath();
            for (float y = kPlotY; y < kPlotY + kPlotH; y += 6.0f)
            {
                moveTo(x, y);
                lineTo(x, std::fmin(y + 3.0f, kPlotY + kPlotH));
            }
            strokeColor(col(theme::kAccent, 0.5f));
            stroke();
        }

        // response curve
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const int count = int(kPlotW) + 1;
        float ys[512];
        for (int i = 0; i < count; ++i)
        {
            const double f = kFMin * std::pow(double(kFMax / kFMin), double(i) / double(count - 1));
            ys[i] = dbToY(float(nulltilt::magnitudeDb(std::fmin(f, sr * 0.4999), fAmount, sr)));
        }

        scissor(kPlotX, kPlotY, kPlotW, kPlotH);

        const float y0 = dbToY(0.0f);
        beginPath();
        moveTo(kPlotX, y0);
        for (int i = 0; i < count; ++i)
            lineTo(kPlotX + kPlotW * float(i) / float(count - 1), ys[i]);
        lineTo(kPlotX + kPlotW, y0);
        closePath();
        fillColor(col(theme::kAccent, 0.10f));
        fill();

        beginPath();
        for (int i = 0; i < count; ++i)
        {
            const float x = kPlotX + kPlotW * float(i) / float(count - 1);
            if (i == 0) moveTo(x, ys[i]); else lineTo(x, ys[i]);
        }
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();

        resetScissor();
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Interaction

    bool inKnobArea(float x, float y) const
    {
        const float dx = x - kKnobCX, dy = y - kKnobCY;
        return std::sqrt(dx * dx + dy * dy) < kKnobR + 16.0f;
    }

    float uiScale() const { return float(getWidth()) / float(kWidth); }

    void setAmountFromUI(float v)
    {
        v = clampf(v, kMinAmount, kMaxAmount);
        if (v == fAmount)
            return;
        fAmount = v;
        setParameterValue(0, v);
        repaint();
    }

    void setAmountGesture(float v)
    {
        editParameter(0, true);
        setAmountFromUI(v);
        editParameter(0, false);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;

        if (ev.press)
        {
            if (!inKnobArea(x, y))
                return false;

            // double-click -> reset to 0
            if (fLastClickTime != 0 && ev.time - fLastClickTime < 350 &&
                std::fabs(x - fLastClickX) < 4.0f && std::fabs(y - fLastClickY) < 4.0f)
            {
                fLastClickTime = 0;
                setAmountGesture(0.0f);
                return true;
            }
            fLastClickTime = ev.time;
            fLastClickX = x;
            fLastClickY = y;

            fDragging = true;
            fDragLastX = x;
            fDragLastY = y;
            fDragValue = fAmount;
            editParameter(0, true);
            return true;
        }

        if (fDragging)
        {
            fDragging = false;
            editParameter(0, false);
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;

        const bool hover = inKnobArea(x, y);
        if (hover != fHover)
        {
            fHover = hover;
            repaint();
        }
        if (!fDragging)
            return false;

        const float dx = x - fDragLastX, dy = fDragLastY - y;
        fDragLastX = x;
        fDragLastY = y;

        const bool fine = (ev.mod & (kModifierControl | kModifierShift)) != 0;
        // 200 units over ~200 px normally, 10x finer with Ctrl/Shift
        const float perPx = fine ? 0.1f : 1.0f;
        fDragValue = clampf(fDragValue + (dx + dy) * perPx, kMinAmount, kMaxAmount);

        const float q = fine ? std::round(fDragValue * 10.0f) / 10.0f : std::round(fDragValue);
        setAmountFromUI(q);
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (!inKnobArea(x, y))
            return false;

        const float d = float(ev.delta.getY()) + float(ev.delta.getX());
        if (d == 0.0f)
            return false;

        setAmountGesture(std::round(fAmount) + (d > 0.0f ? 1.0f : -1.0f));
        return true;
    }

private:
    float fAmount = 0.0f;

    bool fDragging = false;
    bool fHover = false;
    float fDragLastX = 0.0f, fDragLastY = 0.0f, fDragValue = 0.0f;

    uint fLastClickTime = 0;
    float fLastClickX = 0.0f, fLastClickY = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UITilt)
};

UI* createUI()
{
    return new UITilt();
}

END_NAMESPACE_DISTRHO
