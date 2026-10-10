/*
 * NULL JSFX - shared interface for the VST3/CLAP plugins (DGL + NanoVG)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The C++ twin of DATA/Effects/null_jsfx/gfx/null_gfx.jsfx-inc: same design units, header,
 * cards, slider, knobs, plots, meters and scopes, so the GFX JSFX and the VST3/CLAP look
 * alike. A One Slider plugin derives from NullOneSliderUI (360x240), an Essentials or
 * Advanced plugin from NullKnobsUI; both implement drawDisplay().
 */

#pragma once

#include "DistrhoUI.hpp"
#include "NullPlugin.hpp"
#include "NullTheme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

START_NAMESPACE_DISTRHO

class NullUI : public UI
{
public:
    // design size in design units (the window starts at this size times the display scale)
    const uint kWidth, kHeight;

    explicit NullUI(uint width = 360, uint height = 240)
        : UI(width, height, false), kWidth(width), kHeight(height)
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
    // ---- helpers -----------------------------------------------------------------------------

    static Color col(uint32_t rgb, float alpha = 1.0f)
    {
        return Color(int((rgb >> 16) & 0xff), int((rgb >> 8) & 0xff), int(rgb & 0xff), alpha);
    }

    static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    float uiScale() const { return float(getWidth()) / float(kWidth); }

    // the plugin instance (DISTRHO_PLUGIN_WANT_DIRECT_ACCESS) for its Taps
    nulljsfx::Taps* taps() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<NullPluginBase*>(getPluginInstancePointer());
        return p ? &p->taps : nullptr;
#else
        return nullptr;
#endif
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
        label(kWidth - 14.0f, 15.0f, ALIGN_RIGHT, 7.5f, 2.0f, theme::kSlogan, "AUDIO WITHOUT NOISE", true);
        label(kWidth - 14.0f, 29.0f, ALIGN_RIGHT, 8.5f, 2.0f, theme::kMuted, category, true);

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

    // horizontal slider (the One Slider series); norm 0..1, bipolar fills from the centre
    void drawSlider(float x, float y, float w, float norm, bool bipolar, bool lit)
    {
        const float th = 6.0f;
        const float tx = x + norm * w;
        const float f0 = bipolar ? x + w * 0.5f : x;

        beginPath();
        roundedRect(x, y - th * 0.5f, w, th, th * 0.5f);
        fillColor(col(lit ? theme::kTrackHover : theme::kBorder));
        fill();

        if (std::fabs(tx - f0) > 0.25f)
        {
            beginPath();
            rect(std::fmin(f0, tx), y - th * 0.5f, std::fabs(tx - f0), th);
            fillColor(col(theme::kAccent));
            fill();
        }

        if (bipolar)
        {
            beginPath();
            moveTo(x + w * 0.5f, y - th * 0.5f - 4.0f);
            lineTo(x + w * 0.5f, y - th * 0.5f - 7.0f);
            strokeColor(col(theme::kMuted));
            strokeWidth(1.0f);
            stroke();
        }

        // thumb: shadow, grey-gradient body like the site icons, dark grip line
        const float tw = 12.0f, tth = 22.0f;
        beginPath();
        roundedRect(tx - tw * 0.5f, y - tth * 0.5f + 2.0f, tw, tth, 3.0f);
        fillColor(Color(0, 0, 0, 0.35f));
        fill();

        beginPath();
        roundedRect(tx - tw * 0.5f, y - tth * 0.5f, tw, tth, 3.0f);
        fillPaint(linearGradient(tx, y - tth * 0.5f, tx, y + tth * 0.5f, col(theme::kIconHi), col(theme::kIconLo)));
        fill();

        beginPath();
        rect(tx - 1.0f, y - tth * 0.3f, 2.0f, tth * 0.6f);
        fillColor(col(theme::kCard));
        fill();
    }

    // knob (same as ng_knob): body radius r, value arc on a track at r + 10; norm 0..1
    void drawKnob(float x, float y, float r, float norm, bool bipolar, bool lit)
    {
        const float tr = r + 10.0f, amax = 0.75f * float(M_PI);
        const float ang = (norm * 2.0f - 1.0f) * amax, a0 = bipolar ? 0.0f : -amax;
        // NanoVG angles: 0 = right, clockwise; ours: 0 = up, clockwise
        const float off = -0.5f * float(M_PI);

        beginPath();
        arc(x, y, tr, -amax + off, amax + off, CW);
        strokeColor(col(lit ? theme::kTrackHover : theme::kBorder));
        strokeWidth(4.0f);
        stroke();
        if (std::fabs(ang - a0) > 0.001f)
        {
            beginPath();
            arc(x, y, tr, std::fmin(a0, ang) + off, std::fmax(a0, ang) + off, CW);
            strokeColor(col(theme::kAccent));
            stroke();
        }
        if (bipolar)
        {
            beginPath();
            moveTo(x, y - tr - 5.0f);
            lineTo(x, y - tr - 8.0f);
            strokeColor(col(theme::kMuted));
            strokeWidth(1.0f);
            stroke();
        }

        // body: shadow, grey-gradient rim like the site icons, dark cap
        beginPath();
        circle(x, y + 2.0f, r + 1.5f);
        fillColor(Color(0, 0, 0, 0.35f));
        fill();
        beginPath();
        circle(x, y - 1.0f, r);
        fillPaint(linearGradient(x, y + r, x, y - r, col(theme::kIconLo), col(theme::kIconHi)));
        fill();
        beginPath();
        circle(x, y - 1.0f, r - 3.0f);
        fillPaint(linearGradient(x, y + r, x, y - r, col(theme::kCard), col(theme::kCapTop)));
        fill();

        // pointer
        beginPath();
        moveTo(x + std::sin(ang) * r * 0.25f, y - std::cos(ang) * r * 0.25f);
        lineTo(x + std::sin(ang) * r * 0.72f, y - std::cos(ang) * r * 0.72f);
        strokeColor(col(theme::kText));
        strokeWidth(2.4f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);
    }

    // small chevron at (x, y) pointing left (dir -1) or right (1), like ng_arrow
    void drawArrow(float x, float y, float dir)
    {
        beginPath();
        moveTo(x - dir * 1.5f, y - 3.0f);
        lineTo(x + dir * 1.5f, y);
        lineTo(x - dir * 1.5f, y + 3.0f);
        strokeColor(col(theme::kMuted));
        strokeWidth(1.0f);
        stroke();
    }

    // ---- plots (same mapping as ng_plot / ng_freq_x / ng_lin_y) -------------------------------

    float fPX = 0, fPY = 0, fPW = 0, fPH = 0;

    void setPlot(float x, float y, float w, float h) { fPX = x; fPY = y; fPW = w; fPH = h; }
    float freqX(float f) const { return fPX + fPW * std::log(std::fmax(f, 20.0f) / 20.0f) / std::log(1000.0f); }
    float linX(float v, float lo, float hi) const { return fPX + fPW * (v - lo) / (hi - lo); }
    float linY(float v, float lo, float hi) const { return fPY + fPH * (1.0f - (clampf(v, lo, hi) - lo) / (hi - lo)); }

    // log-frequency grid (20 Hz to 20 kHz), dB lines at +-range/2 and 0 on a +-range*2/3 scale
    void gridFreq(float range)
    {
        strokeWidth(1.0f);
        for (int pass = 0; pass < 2; ++pass)
        {
            beginPath();
            for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
                for (int m = 1; m <= 9; ++m)
                {
                    const float f = float(m) * d;
                    if (f < 20.0f || f > 20000.0f || (m == 1) != (pass == 1))
                        continue;
                    const float x = std::round(freqX(f)) + 0.5f;
                    moveTo(x, fPY);
                    lineTo(x, fPY + fPH);
                }
            strokeColor(col(theme::kBorder, pass == 1 ? 1.0f : 0.5f));
            stroke();
        }
        for (int i = -1; i <= 1; ++i)
        {
            const float db = i * range * 0.5f;
            const float y = std::round(linY(db, -range * 2.0f / 3.0f, range * 2.0f / 3.0f)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(i == 0 ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
            char buf[16];
            std::snprintf(buf, sizeof(buf), db > 0 ? "+%g" : "%g", double(db));
            label(fPX - 6.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const float capY = fPY + fPH + 11.0f;
        label(freqX(100.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), capY, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");
    }

    // plain nx x ny grid with a brighter centre cross when centre is true
    void grid(int nx, int ny, bool centre)
    {
        strokeWidth(1.0f);
        for (int i = 0; i <= nx; ++i)
        {
            const float x = std::round(fPX + fPW * i / nx) + 0.5f;
            const bool c = centre && i * 2 == nx;
            beginPath();
            moveTo(x, fPY);
            lineTo(x, fPY + fPH);
            strokeColor(c ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
        }
        for (int i = 0; i <= ny; ++i)
        {
            const float y = std::round(fPY + fPH * i / ny) + 0.5f;
            const bool c = centre && i * 2 == ny;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(c ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
        }
    }

    // curve through n points (x, y in design units); fillToY >= 0 also shades down/up to that y
    void curve(const float* xs, const float* ys, int n, float fillToY = -1.0f)
    {
        if (n < 2)
            return;
        scissor(fPX, fPY, fPW, fPH);
        if (fillToY >= 0.0f)
        {
            beginPath();
            moveTo(xs[0], fillToY);
            for (int i = 0; i < n; ++i)
                lineTo(xs[i], ys[i]);
            lineTo(xs[n - 1], fillToY);
            closePath();
            fillColor(col(theme::kAccent, 0.10f));
            fill();
        }
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        resetScissor();
    }

    // dashed vertical marker at x
    void dashedV(float x, float alpha = 0.5f)
    {
        x = std::round(x) + 0.5f;
        beginPath();
        for (float y = fPY; y < fPY + fPH; y += 6.0f)
        {
            moveTo(x, y);
            lineTo(x, std::fmin(y + 3.0f, fPY + fPH));
        }
        strokeColor(col(theme::kAccent, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    // ---- meters (same as ng_meter / ng_gr_meter) ---------------------------------------------

    static float dbNorm(float db, float lo) { return clampf((db - lo) / -lo, 0.0f, 1.0f); }

    void meter(float x, float y, float w, float h, const char* name, float db, float peakDb, float lo)
    {
        label(x, y + h * 0.5f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, name, true);
        const float bx = x + 30.0f, bw = w - 30.0f;
        beginPath();
        rect(bx, y, bw, h);
        fillColor(col(theme::kBorder));
        fill();
        const float f = dbNorm(db, lo);
        if (f > 0.0f)
        {
            beginPath();
            rect(bx, y, bw * f, h);
            fillColor(col(theme::kAccent));
            fill();
        }
        const float p = dbNorm(peakDb, lo);
        if (p > 0.0f)
        {
            beginPath();
            rect(bx + bw * p - 1.0f, y, 1.0f, h);
            fillColor(col(theme::kText));
            fill();
        }
    }

    void grMeter(float x, float y, float w, float h, const char* name, float gr, float maxDb)
    {
        label(x, y + h * 0.5f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, name, true);
        const float bx = x + 30.0f, bw = w - 30.0f;
        beginPath();
        rect(bx, y, bw, h);
        fillColor(col(theme::kBorder));
        fill();
        const float f = clampf(gr / maxDb, 0.0f, 1.0f);
        if (f > 0.0f)
        {
            beginPath();
            rect(bx + bw * (1.0f - f), y, bw * f, h);
            fillColor(col(theme::kAccent));
            fill();
        }
    }

    // ---- scopes (same as ng_scope / ng_vector), read from the plugin's Taps ------------------

    void scope(float gain)
    {
        nulljsfx::Taps* t = taps();
        if (!t)
            return;
        const uint32_t len = nulljsfx::Taps::kScopeLen, pos = t->scopePos.load(std::memory_order_acquire);
        const int n = int(fPW);
        const uint32_t per = std::max<uint32_t>(1, len / uint32_t(n));
        beginPath();
        for (int i = 0; i < n; ++i)
        {
            float mn = 1.0f, mx = -1.0f;
            for (uint32_t j = 0; j < per; ++j)
            {
                const uint32_t k = (pos + uint32_t(i) * per + j) % len;
                const float v = (t->scopeL[k] + t->scopeR[k]) * 0.5f * gain;
                mn = std::fmin(mn, v);
                mx = std::fmax(mx, v);
            }
            const float x = fPX + float(i) + 0.5f;
            moveTo(x, fPY + fPH * (0.5f - std::fmin(1.0f, mx) * 0.5f));
            lineTo(x, fPY + fPH * (0.5f - std::fmax(-1.0f, mn) * 0.5f) + 0.5f);
        }
        strokeColor(col(theme::kAccent, 0.9f));
        strokeWidth(1.0f);
        stroke();
    }

    void vectorscope(float gain)
    {
        const float cx = fPX + fPW * 0.5f, cy = fPY + fPH * 0.5f, rad = std::fmin(fPW, fPH) * 0.5f;
        beginPath();
        moveTo(cx, cy - rad); lineTo(cx, cy + rad);
        moveTo(cx - rad, cy); lineTo(cx + rad, cy);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        nulljsfx::Taps* t = taps();
        if (!t)
            return;
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        beginPath();
        for (uint32_t k = 0; k < len; ++k)
        {
            const float l = t->scopeL[k] * gain, r = t->scopeR[k] * gain;
            const float m = (l + r) * 0.7071f, s = (r - l) * 0.7071f;
            rect(cx + clampf(s, -1.0f, 1.0f) * rad, cy - clampf(m, -1.0f, 1.0f) * rad, 1.0f, 1.0f);
        }
        fillColor(col(theme::kAccent, 0.35f));
        fill();
    }
};

// ------------------------------------------------------------------------------------------------
// One Slider layout: header, display card (14,54,332,110), slider card (14,172,332,54)

class NullOneSliderUI : public NullUI
{
public:
    struct Info {
        const char* title;    // header, e.g. "TILT"
        const char* label;    // slider card label, e.g. "AMOUNT"
        const char* left;     // end labels, e.g. "DARK" / "BRIGHT"
        const char* right;
        float min, max, def;
        bool bipolar;         // fill from the centre
        bool animated;        // repaint continuously (meters, scopes)
    };

    explicit NullOneSliderUI(const Info& info)
        : fInfo(info), fValue(info.def) {}

protected:
    // display area (design units): card interior is 24..336 x 62..156
    virtual void drawDisplay() = 0;
    // value text at the right of the slider card; default "+N" for bipolar, "N" otherwise
    virtual void formatValue(char* buf, size_t n, float v)
    {
        const int i = int(std::lround(v));
        std::snprintf(buf, n, fInfo.bipolar && i > 0 ? "+%d" : "%d", i);
    }
    // optional detail line in the middle of the slider card
    virtual void formatCaption(char* buf, size_t, float) { buf[0] = '\0'; }

    float value() const { return fValue; }

    void parameterChanged(uint32_t index, float v) override
    {
        if (index != 0)
            return;
        fValue = v;
        repaint();
    }

    void sampleRateChanged(double) override { repaint(); }

    void uiIdle() override
    {
        if (fInfo.animated)
            repaint();
    }

    void onNanoDisplay() override
    {
        const float s = uiScale();
        scale(s, s);

        beginPath();
        rect(0, 0, kWidth, kHeight);
        fillColor(col(theme::kBackground));
        fill();

        fontFace(NANOVG_DEJAVU_SANS_TTF);

        drawHeader(fInfo.title, "ONE SLIDER");
        card(14.0f, 54.0f, 332.0f, 110.0f);
        drawDisplay();
        drawSliderCard();
    }

    void drawSliderCard()
    {
        card(14.0f, 172.0f, 332.0f, 54.0f);
        label(28.0f, 187.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, fInfo.label, true);
        char buf[64];
        formatCaption(buf, sizeof(buf), fValue);
        label(180.0f, 187.0f, ALIGN_CENTER, 8.0f, 0.3f, theme::kMuted, buf);
        formatValue(buf, sizeof(buf), fValue);
        label(332.0f, 187.0f, ALIGN_RIGHT, 14.0f, 0.0f, theme::kText, buf, true);
        label(28.0f, kSliderY, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, fInfo.left, true);
        label(332.0f, kSliderY, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, fInfo.right, true);
        drawSlider(kSliderX, kSliderY, kSliderW, (fValue - fInfo.min) / (fInfo.max - fInfo.min), fInfo.bipolar,
                   fDragging || fHover);
    }

    // ---- interaction: drag sideways (Ctrl/Shift fine), double-click resets, wheel steps -----

    static constexpr float kSliderX = 74.0f, kSliderY = 210.0f, kSliderW = 212.0f;

    bool inSlider(float x, float y) const
    {
        return x > kSliderX - 10.0f && x < kSliderX + kSliderW + 10.0f && std::fabs(y - kSliderY) < 16.0f;
    }

    void setFromUI(float v)
    {
        v = clampf(v, fInfo.min, fInfo.max);
        if (v == fValue)
            return;
        fValue = v;
        setParameterValue(0, v);
        repaint();
    }

    void setGesture(float v)
    {
        editParameter(0, true);
        setFromUI(v);
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
            if (!inSlider(x, y))
                return false;
            if (fLastClickTime != 0 && ev.time - fLastClickTime < 350 &&
                std::fabs(x - fLastClickX) < 4.0f && std::fabs(y - fLastClickY) < 4.0f)
            {
                fLastClickTime = 0;
                setGesture(fInfo.def);
                return true;
            }
            fLastClickTime = ev.time;
            fLastClickX = x;
            fLastClickY = y;
            fDragging = true;
            fDragLastX = x;
            fDragValue = fValue;
            editParameter(0, true);
            return true;
        }
        if (fDragging)
        {
            fDragging = false;
            editParameter(0, false);
            repaint();
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        const bool hover = inSlider(x, y);
        if (hover != fHover)
        {
            fHover = hover;
            repaint();
        }
        if (!fDragging)
            return false;

        const float dx = x - fDragLastX;
        fDragLastX = x;
        const bool fine = (ev.mod & (kModifierControl | kModifierShift)) != 0;
        // the full range over the slider width normally, 10x finer with Ctrl/Shift
        fDragValue = clampf(fDragValue + dx * (fInfo.max - fInfo.min) / kSliderW * (fine ? 0.1f : 1.0f),
                            fInfo.min, fInfo.max);
        setFromUI(fine ? std::round(fDragValue * 10.0f) / 10.0f : std::round(fDragValue));
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        if (!inSlider(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s))
            return false;
        const float d = float(ev.delta.getY()) + float(ev.delta.getX());
        if (d == 0.0f)
            return false;
        setGesture(std::round(fValue) + (d > 0.0f ? 1.0f : -1.0f));
        return true;
    }

private:
    const Info fInfo;
    float fValue;
    bool fDragging = false, fHover = false;
    float fDragLastX = 0.0f, fDragValue = 0.0f;
    uint fLastClickTime = 0;
    float fLastClickX = 0.0f, fLastClickY = 0.0f;
};

// ------------------------------------------------------------------------------------------------
// Knob layout (Essentials, Advanced), the twin of ng_knobs_begin / ng_tabs / ng_row /
// ng_knob_ctl / ng_select_ctl: header, display card (14, 54, W-28, D), controls card below
// with an optional tab bar and rows of up to 6 centred cells of width (W-44)/6, 86 high:
// label at +10, knob (radius 14) at +44, value text at +77.

class NullKnobsUI : public NullUI
{
public:
    enum Kind { kKnob, kSelect };

    struct Control {
        uint32_t param;    // parameter index (= the JSFX slider's position)
        Kind kind;
        const char* label; // e.g. "TIME"
        int row;           // row in the controls card, from 0
        int page;          // tab the control is on, or -1 for every tab
        bool bipolar;      // knob arc from the centre
    };

    struct Layout {
        const char* title;    // e.g. "DELAY"
        const char* category; // e.g. "ESSENTIALS"
        uint width;           // design width, usually 480
        uint display;         // display card height D, usually 120
        const char* tabs;     // "A|B|C", or nullptr for no tab bar
        int rows;             // rows in the controls card (the most any tab shows)
        const SliderInfo* sliders;
        uint32_t numSliders;
        const Control* controls;
        uint32_t numControls;
        bool animated;        // repaint continuously (meters, scopes)
    };

    static uint heightFor(const Layout& l)
    {
        return 54 + l.display + 8 + 8 + (l.tabs ? 28 : 0) + uint(l.rows) * 86 + 4 + 14;
    }

    explicit NullKnobsUI(const Layout& l)
        : NullUI(l.width, heightFor(l)), fL(l)
    {
        for (uint32_t i = 0; i < l.numSliders && i < kMax; ++i)
            fValues[i] = l.sliders[i].def;
        fNumTabs = 0;
        if (l.tabs)
        {
            fNumTabs = 1;
            for (const char* c = l.tabs; *c; ++c)
                fNumTabs += *c == '|';
        }
    }

protected:
    static constexpr uint32_t kMax = 64;

    // the display area (design units) is fVX, fVY, fVW, fVH
    virtual void drawDisplay() = 0;

    // value text under a knob; default: the value with the slider's unit
    virtual void formatValue(uint32_t param, float v, char* buf, size_t n)
    {
        const SliderInfo& in = fL.sliders[param];
        const char* sep = (in.unit[0] && std::strcmp(in.unit, "%") != 0) ? " " : "";
        if (in.step >= 1.0f)
            std::snprintf(buf, n, "%d%s%s", int(std::lround(v)), sep, in.unit);
        else
            std::snprintf(buf, n, "%.2f%s%s", v, sep, in.unit);
    }

    float value(uint32_t param) const { return param < kMax ? fValues[param] : 0.0f; }
    int page() const { return fPage; }

    float fVX = 0, fVY = 0, fVW = 0, fVH = 0;

    void parameterChanged(uint32_t index, float v) override
    {
        if (index >= kMax)
            return;
        fValues[index] = v;
        repaint();
    }

    void sampleRateChanged(double) override { repaint(); }

    void uiIdle() override
    {
        if (fL.animated)
            repaint();
    }

    // ---- geometry -----------------------------------------------------------------------------

    float cardY() const { return 54.0f + float(fL.display) + 8.0f; }
    float cellW() const { return (float(kWidth) - 44.0f) / 6.0f; }

    bool visible(const Control& c) const { return c.page < 0 || c.page == fPage; }

    // centre x and row top of control i
    void cell(uint32_t i, float& cx, float& ry) const
    {
        const Control& c = fL.controls[i];
        int n = 0, k = 0;
        for (uint32_t j = 0; j < fL.numControls; ++j)
        {
            const Control& o = fL.controls[j];
            if (!visible(o) || o.row != c.row)
                continue;
            if (j < i)
                ++k;
            ++n;
        }
        const float cw = cellW();
        cx = 22.0f + float(6 - n) * cw * 0.5f + (float(k) + 0.5f) * cw;
        ry = cardY() + 8.0f + (fL.tabs ? 28.0f : 0.0f) + float(c.row) * 86.0f;
    }

    bool hit(uint32_t i, float x, float y) const
    {
        float cx, ry;
        cell(i, cx, ry);
        const Control& c = fL.controls[i];
        if (c.kind == kKnob)
            return std::hypot(x - cx, y - (ry + 44.0f)) < 14.0f + 10.0f + 6.0f;
        const float pw = cellW() - 10.0f;
        return x >= cx - pw * 0.5f && x < cx + pw * 0.5f && std::fabs(y - (ry + 44.0f)) < 14.0f;
    }

    float tabW() const { return (float(kWidth) - 44.0f - float(fNumTabs - 1) * 4.0f) / float(fNumTabs); }

    int tabAt(float x, float y) const
    {
        if (!fL.tabs)
            return -1;
        const float ty = cardY() + 8.0f, tw = tabW();
        if (y < ty || y >= ty + 20.0f)
            return -1;
        for (int i = 0; i < fNumTabs; ++i)
        {
            const float tx = 22.0f + float(i) * (tw + 4.0f);
            if (x >= tx && x < tx + tw)
                return i;
        }
        return -1;
    }

    // ---- drawing ------------------------------------------------------------------------------

    void onNanoDisplay() override
    {
        const float s = uiScale();
        scale(s, s);

        beginPath();
        rect(0, 0, kWidth, kHeight);
        fillColor(col(theme::kBackground));
        fill();

        fontFace(NANOVG_DEJAVU_SANS_TTF);

        drawHeader(fL.title, fL.category);
        card(14.0f, 54.0f, float(kWidth) - 28.0f, float(fL.display));
        fVX = 24.0f; fVY = 62.0f; fVW = float(kWidth) - 48.0f; fVH = float(fL.display) - 16.0f;
        drawDisplay();

        const float cy = cardY();
        card(14.0f, cy, float(kWidth) - 28.0f, float(kHeight) - 14.0f - cy);
        if (fL.tabs)
            drawTabs(cy + 8.0f);
        char buf[64];
        for (uint32_t i = 0; i < fL.numControls; ++i)
        {
            const Control& c = fL.controls[i];
            if (!visible(c))
                continue;
            float cx, ry;
            cell(i, cx, ry);
            const SliderInfo& in = fL.sliders[c.param];
            const float v = fValues[c.param];
            const bool lit = int(i) == fHover || int(i) == fDrag;
            label(cx, ry + 10.0f, ALIGN_CENTER, 7.5f, 1.5f, theme::kMuted, c.label, true);
            if (c.kind == kKnob)
            {
                drawKnob(cx, ry + 44.0f, 14.0f, (v - in.min) / (in.max - in.min), c.bipolar, lit);
                formatValue(c.param, v, buf, sizeof(buf));
                label(cx, ry + 77.0f, ALIGN_CENTER, 8.5f, 0.0f, theme::kText, buf);
            }
            else
            {
                const float pw = cellW() - 10.0f;
                beginPath();
                roundedRect(cx - pw * 0.5f + 0.5f, ry + 33.5f, pw - 1.0f, 21.0f, 4.0f);
                fillColor(col(theme::kBackground));
                fill();
                strokeColor(col(lit ? theme::kTrackHover : theme::kBorder));
                strokeWidth(1.0f);
                stroke();
                const int n = int(in.max - in.min + 0.5f) + 1;
                const int k = std::max(0, std::min(n - 1, int(std::lround(v - in.min))));
                label(cx, ry + 44.0f, ALIGN_CENTER, 8.0f, 0.5f, theme::kText, in.options ? in.options[k] : "", true);
                drawArrow(cx - pw * 0.5f + 7.0f, ry + 44.0f, -1.0f);
                drawArrow(cx + pw * 0.5f - 7.0f, ry + 44.0f, 1.0f);
            }
        }
    }

    void drawTabs(float ty)
    {
        const float tw = tabW();
        const char* p = fL.tabs;
        for (int i = 0; i < fNumTabs; ++i)
        {
            const char* q = std::strchr(p, '|');
            const size_t len = q ? size_t(q - p) : std::strlen(p);
            char name[32];
            std::snprintf(name, sizeof(name), "%.*s", int(len), p);
            const float tx = 22.0f + float(i) * (tw + 4.0f);
            beginPath();
            roundedRect(tx + 0.5f, ty + 0.5f, tw - 1.0f, 19.0f, 4.0f);
            fillColor(col(i == fPage ? 0x2d2b28 : theme::kBackground));
            fill();
            strokeColor(col(i == fPage || i == fHoverTab ? theme::kTrackHover : theme::kBorder));
            strokeWidth(1.0f);
            stroke();
            label(tx + tw * 0.5f, ty + 10.0f, ALIGN_CENTER, 7.5f, 1.5f, i == fPage ? theme::kText : theme::kMuted, name, true);
            p = q ? q + 1 : p + len;
        }
    }

    // ---- interaction --------------------------------------------------------------------------

    int controlAt(float x, float y) const
    {
        for (uint32_t i = 0; i < fL.numControls; ++i)
            if (visible(fL.controls[i]) && hit(i, x, y))
                return int(i);
        return -1;
    }

    float snap(const SliderInfo& in, float v) const
    {
        v = clampf(v, in.min, in.max);
        if (in.step > 0.0f)
            v = clampf(in.min + std::round((v - in.min) / in.step) * in.step, in.min, in.max);
        return v;
    }

    void setFromUI(uint32_t param, float v)
    {
        if (v == fValues[param])
            return;
        fValues[param] = v;
        setParameterValue(param, v);
        repaint();
    }

    void setGesture(uint32_t param, float v)
    {
        editParameter(param, true);
        setFromUI(param, v);
        editParameter(param, false);
    }

    void step(int i, int dir, bool wrap)
    {
        const Control& c = fL.controls[i];
        const SliderInfo& in = fL.sliders[c.param];
        const float st = in.step > 0.0f ? in.step : (in.max - in.min) / 100.0f;
        float v = fValues[c.param] + float(dir) * st;
        if (wrap && v > in.max + st * 0.5f)
            v = in.min;
        else if (wrap && v < in.min - st * 0.5f)
            v = in.max;
        setGesture(c.param, snap(in, v));
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;

        if (ev.press)
        {
            const int t = tabAt(x, y);
            if (t >= 0)
            {
                fPage = t;
                repaint();
                return true;
            }
            const int i = controlAt(x, y);
            if (i < 0)
                return false;
            const Control& c = fL.controls[i];
            if (c.kind == kSelect)
            {
                float cx, ry;
                cell(uint32_t(i), cx, ry);
                step(i, x < cx ? -1 : 1, true);
                return true;
            }
            if (fLastClickTime != 0 && ev.time - fLastClickTime < 350 && fLastClick == i)
            {
                fLastClickTime = 0;
                setGesture(c.param, fL.sliders[c.param].def);
                return true;
            }
            fLastClickTime = ev.time;
            fLastClick = i;
            fDrag = i;
            fDragX = x;
            fDragY = y;
            fDragValue = fValues[c.param];
            editParameter(c.param, true);
            return true;
        }
        if (fDrag >= 0)
        {
            editParameter(fL.controls[fDrag].param, false);
            fDrag = -1;
            repaint();
            return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        const int h = controlAt(x, y), ht = tabAt(x, y);
        if (h != fHover || ht != fHoverTab)
        {
            fHover = h;
            fHoverTab = ht;
            repaint();
        }
        if (fDrag < 0)
            return false;
        const Control& c = fL.controls[fDrag];
        const SliderInfo& in = fL.sliders[c.param];
        const bool fine = (ev.mod & (kModifierControl | kModifierShift)) != 0;
        // up or right raises the value: the full range over 200 design units, 10x finer with Ctrl/Shift
        const float mv = (x - fDragX) + (fDragY - y);
        fDragX = x;
        fDragY = y;
        fDragValue = clampf(fDragValue + mv * (in.max - in.min) / 200.0f * (fine ? 0.1f : 1.0f), in.min, in.max);
        setFromUI(c.param, snap(in, fDragValue));
        return true;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        const int i = controlAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (i < 0)
            return false;
        const float d = float(ev.delta.getY()) + float(ev.delta.getX());
        if (d == 0.0f)
            return false;
        step(i, d > 0.0f ? 1 : -1, false);
        return true;
    }

private:
    const Layout fL;
    float fValues[kMax] = {};
    int fNumTabs = 0, fPage = 0;
    int fHover = -1, fHoverTab = -1, fDrag = -1, fLastClick = -1;
    float fDragX = 0.0f, fDragY = 0.0f, fDragValue = 0.0f;
    uint fLastClickTime = 0;
};

END_NAMESPACE_DISTRHO
