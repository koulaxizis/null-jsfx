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

// Layout (1x coordinates)
constexpr float kPad = 12.0f;
constexpr float kTop = 42.0f;
constexpr float kKnobPanelX = kPad, kKnobPanelY = kTop, kKnobPanelW = 140.0f, kKnobPanelH = kHeight - kTop - kPad;
constexpr float kKnobCX = kKnobPanelX + kKnobPanelW * 0.5f;
constexpr float kKnobCY = kKnobPanelY + 78.0f;
constexpr float kKnobR  = 40.0f;
constexpr float kGraphX = kKnobPanelX + kKnobPanelW + 10.0f;
constexpr float kGraphY = kTop;
constexpr float kGraphW = kWidth - kGraphX - kPad;
constexpr float kGraphH = kHeight - kTop - kPad;
// Plot rect inside graph panel
constexpr float kPlotX = kGraphX + 8.0f;
constexpr float kPlotY = kGraphY + 10.0f;
constexpr float kPlotW = kGraphW - 16.0f;
constexpr float kPlotH = kGraphH - 32.0f;

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
        : UI(kWidth, kHeight, true)
    {
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

        // background
        beginPath();
        rect(0, 0, kWidth, kHeight);
        fillColor(col(theme::kBackground));
        fill();

        fontFace(NANOVG_DEJAVU_SANS_TTF);

        drawHeader();
        drawKnobPanel();
        drawGraph();
    }

    void drawHeader()
    {
        // title
        fontSize(15.0f);
        textLetterSpacing(2.5f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(col(theme::kText));
        text(kPad + 2.0f, 22.0f, "NULL TILT", nullptr);
        text(kPad + 2.4f, 22.0f, "NULL TILT", nullptr); // faux-bold

        // NATIVE tag
        fontSize(9.0f);
        textLetterSpacing(1.5f);
        Rectangle<float> bounds;
        textBounds(0, 0, "NATIVE", nullptr, bounds);
        const float tw = bounds.getWidth();
        const float tagW = tw + 12.0f, tagH = 16.0f;
        const float tagX = kWidth - kPad - tagW, tagY = 22.0f - tagH * 0.5f;
        beginPath();
        roundedRect(tagX, tagY, tagW, tagH, 3.0f);
        strokeColor(col(theme::kTextMuted));
        strokeWidth(1.0f);
        stroke();
        fillColor(col(theme::kTextMuted));
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(tagX + tagW * 0.5f + 0.75f, 22.5f, "NATIVE", nullptr);
        textLetterSpacing(0.0f);
    }

    void panel(float x, float y, float w, float h)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 4.0f);
        fillColor(col(theme::kPanel));
        fill();
        strokeColor(col(theme::kPanelEdge));
        strokeWidth(1.0f);
        stroke();
    }

    void drawKnobPanel()
    {
        panel(kKnobPanelX, kKnobPanelY, kKnobPanelW, kKnobPanelH);

        fontSize(9.0f);
        textLetterSpacing(1.5f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(col(theme::kTextMuted));
        text(kKnobCX, kKnobPanelY + 16.0f, "AMOUNT", nullptr);

        const float norm = (fAmount - kMinAmount) / (kMaxAmount - kMinAmount);
        const float aVal = kAngleStart + norm * kAngleSweep;
        const float aMid = kAngleStart + 0.5f * kAngleSweep;
        const float arcR = kKnobR + 7.0f;

        // track
        beginPath();
        arc(kKnobCX, kKnobCY, arcR, kAngleStart, kAngleStart + kAngleSweep, CW);
        strokeColor(col(theme::kTrack));
        strokeWidth(3.0f);
        lineCap(ROUND);
        stroke();

        // bipolar value arc from centre
        if (std::fabs(fAmount) > 0.05f)
        {
            beginPath();
            if (aVal > aMid)
                arc(kKnobCX, kKnobCY, arcR, aMid, aVal, CW);
            else
                arc(kKnobCX, kKnobCY, arcR, aVal, aMid, CW);
            strokePaint(linearGradient(kKnobCX - arcR, kKnobCY, kKnobCX + arcR, kKnobCY,
                                       col(theme::kAccentLo), col(theme::kAccentHi)));
            strokeWidth(3.0f);
            stroke();
        }
        lineCap(BUTT);

        // centre (0) tick, outside the arc
        beginPath();
        circle(kKnobCX, kKnobCY - arcR - 6.0f, 1.3f);
        fillColor(col(theme::kTextMuted));
        fill();

        // knob body: soft drop shadow, then cap with grey gradient rim
        beginPath();
        circle(kKnobCX, kKnobCY + 2.0f, kKnobR + 1.0f);
        fillPaint(radialGradient(kKnobCX, kKnobCY + 2.0f, kKnobR - 4.0f, kKnobR + 4.0f,
                                 Color(0, 0, 0, 0.45f), Color(0, 0, 0, 0.0f)));
        fill();

        beginPath();
        circle(kKnobCX, kKnobCY, kKnobR);
        fillPaint(linearGradient(kKnobCX, kKnobCY - kKnobR, kKnobCX, kKnobCY + kKnobR,
                                 col(theme::kAccentHi), col(theme::kAccentLo)));
        fill();

        beginPath();
        circle(kKnobCX, kKnobCY, kKnobR - 3.0f);
        fillPaint(linearGradient(kKnobCX, kKnobCY - kKnobR, kKnobCX, kKnobCY + kKnobR,
                                 col(0x2a2a2a), col(theme::kKnobBody)));
        fill();

        // pointer
        const float ca = std::cos(aVal), sa = std::sin(aVal);
        beginPath();
        moveTo(kKnobCX + ca * (kKnobR * 0.35f), kKnobCY + sa * (kKnobR * 0.35f));
        lineTo(kKnobCX + ca * (kKnobR - 9.0f), kKnobCY + sa * (kKnobR - 9.0f));
        strokeColor(col(theme::kAccentHi));
        strokeWidth(3.0f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);

        // end labels under the arc ends
        const float endY = kKnobCY + std::sin(kAngleStart) * arcR + 12.0f;
        fontSize(8.5f);
        textLetterSpacing(1.2f);
        fillColor(col(theme::kTextMuted));
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(kKnobCX + std::cos(kAngleStart) * arcR, endY, "DARK", nullptr);
        text(kKnobCX - std::cos(kAngleStart) * arcR, endY, "BRIGHT", nullptr);

        // value readout
        char buf[32];
        const float rounded = std::round(fAmount);
        if (std::fabs(fAmount - rounded) < 0.05f)
            std::snprintf(buf, sizeof(buf), "%+d", int(rounded));
        else
            std::snprintf(buf, sizeof(buf), "%+.1f", fAmount);
        if (rounded == 0.0f && std::fabs(fAmount) < 0.05f)
            std::snprintf(buf, sizeof(buf), "0");
        fontSize(20.0f);
        textLetterSpacing(0.5f);
        fillColor(col(theme::kText));
        text(kKnobCX, kKnobPanelY + kKnobPanelH - 30.0f, buf, nullptr);

        // high-shelf gain caption
        const double hiDb = fAmount / 100.0 * nulltilt::kMaxDb;
        std::snprintf(buf, sizeof(buf), "LO %+.1f   HI %+.1f dB", -hiDb, hiDb);
        if (std::fabs(hiDb) < 0.05)
            std::snprintf(buf, sizeof(buf), "FLAT");
        fontSize(8.5f);
        textLetterSpacing(0.4f);
        fillColor(col(theme::kTextMuted));
        text(kKnobCX, kKnobPanelY + kKnobPanelH - 12.0f, buf, nullptr);
        textLetterSpacing(0.0f);
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
        panel(kGraphX, kGraphY, kGraphW, kGraphH);

        // frequency grid
        static const float minor[] = { 50, 200, 500, 2000, 5000 };
        static const float major[] = { 100, 1000, 10000 };
        strokeWidth(1.0f);
        beginPath();
        for (float f : minor)
        {
            const float x = std::round(freqToX(f)) + 0.5f;
            moveTo(x, kPlotY);
            lineTo(x, kPlotY + kPlotH);
        }
        strokeColor(col(theme::kGrid, 0.6f));
        stroke();

        beginPath();
        for (float f : major)
        {
            const float x = std::round(freqToX(f)) + 0.5f;
            moveTo(x, kPlotY);
            lineTo(x, kPlotY + kPlotH);
        }
        for (float db : { -6.0f, 6.0f })
        {
            const float y = std::round(dbToY(db)) + 0.5f;
            moveTo(kPlotX, y);
            lineTo(kPlotX + kPlotW, y);
        }
        strokeColor(col(theme::kGrid));
        stroke();

        // faint 0 dB line
        {
            const float y = std::round(dbToY(0.0f)) + 0.5f;
            beginPath();
            moveTo(kPlotX, y);
            lineTo(kPlotX + kPlotW, y);
            strokeColor(col(theme::kTextMuted, 0.45f));
            stroke();
        }

        // 700 Hz pivot marker (dashed)
        {
            const float x = std::round(freqToX(float(nulltilt::kPivotHz))) + 0.5f;
            beginPath();
            for (float y = kPlotY + 12.0f; y < kPlotY + kPlotH; y += 6.0f)
            {
                moveTo(x, y);
                lineTo(x, std::fmin(y + 3.0f, kPlotY + kPlotH));
            }
            strokeColor(col(theme::kAccentLo, 0.7f));
            stroke();
            fontSize(8.0f);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(col(theme::kTextMuted));
            text(x, kPlotY + 5.0f, "700", nullptr);
        }

        // axis captions
        fontSize(8.0f);
        fillColor(col(theme::kTextMuted));
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        const float capY = kPlotY + kPlotH + 11.0f;
        text(freqToX(100.0f), capY, "100", nullptr);
        text(freqToX(1000.0f), capY, "1k", nullptr);
        text(freqToX(10000.0f), capY, "10k", nullptr);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(kPlotX + 2.0f, dbToY(6.0f) - 6.0f, "+6 dB", nullptr);
        text(kPlotX + 2.0f, dbToY(-6.0f) + 6.0f, "-6 dB", nullptr);

        // response curve
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const int n = int(kPlotW);
        float ys[512];
        const int count = n + 1 < 512 ? n + 1 : 512;
        for (int i = 0; i < count; ++i)
        {
            const float x = kPlotX + kPlotW * float(i) / float(count - 1);
            const double f = kFMin * std::pow(double(kFMax / kFMin), double(x - kPlotX) / double(kPlotW));
            const double db = f < sr * 0.5 ? nulltilt::magnitudeDb(f, fAmount, sr)
                                           : nulltilt::magnitudeDb(sr * 0.4999, fAmount, sr);
            ys[i] = dbToY(float(db));
        }

        scissor(kPlotX, kPlotY, kPlotW, kPlotH);

        // soft fill between curve and 0 dB
        const float y0 = dbToY(0.0f);
        beginPath();
        moveTo(kPlotX, y0);
        for (int i = 0; i < count; ++i)
            lineTo(kPlotX + kPlotW * float(i) / float(count - 1), ys[i]);
        lineTo(kPlotX + kPlotW, y0);
        closePath();
        fillColor(col(theme::kAccentHi, 0.10f));
        fill();

        beginPath();
        for (int i = 0; i < count; ++i)
        {
            const float x = kPlotX + kPlotW * float(i) / float(count - 1);
            if (i == 0) moveTo(x, ys[i]); else lineTo(x, ys[i]);
        }
        strokePaint(linearGradient(kPlotX, 0, kPlotX + kPlotW, 0,
                                   col(theme::kAccentLo), col(theme::kAccentHi)));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();

        resetScissor();
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Interaction

    bool inKnobArea(float x, float y) const
    {
        return x >= kKnobPanelX && x < kKnobPanelX + kKnobPanelW && y >= kKnobPanelY && y < kKnobPanelY + kKnobPanelH;
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
        if (!fDragging)
            return false;

        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
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
