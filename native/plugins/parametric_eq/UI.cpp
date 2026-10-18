/*
 * NULL JSFX - Parametric EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ParametricEqDSP.hpp"
#include "ParametricEqPlugin.hpp"

#include <chrono>

START_NAMESPACE_DISTRHO

#define NULL_PEQ_CONTROLS(b)                                        \
    { b * 5 + 0, NullKnobsUI::kKnob, "FREQ", 0, b, false },         \
    { b * 5 + 1, NullKnobsUI::kKnob, "GAIN", 0, b, true },          \
    { b * 5 + 2, NullKnobsUI::kKnob, "Q", 0, b, false },            \
    { b * 5 + 3, NullKnobsUI::kSelect, "TYPE", 0, b, false },       \
    { b * 5 + 4, NullKnobsUI::kSelect, "MODE", 0, b, false }

// one tab per band, as in parametric_eq_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    NULL_PEQ_CONTROLS(0), NULL_PEQ_CONTROLS(1), NULL_PEQ_CONTROLS(2), NULL_PEQ_CONTROLS(3), NULL_PEQ_CONTROLS(4),
};

#undef NULL_PEQ_CONTROLS

class UIParametricEq : public NullKnobsUI
{
public:
    UIParametricEq()
        : NullKnobsUI({ "PARAMETRIC EQ", "ESSENTIALS", 480, 140, "LOW|LOW MID|MID HIGH|HIGH|ULTRA HIGH", 1,
                        nullpeq::kSliders, nullpeq::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nulleqan::Tap* tap() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<ParametricEqBase*>(p)->analyzer : nullptr;
#else
        return nullptr;
#endif
    }

    // the spectrum behind the curve (POST or PRE), as pe_spectrum in the GFX JSFX
    void drawSpectrum()
    {
        const nulleqan::Tap* t = tap();
        if (!t || fAnMode == 2)
            return;
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        fAn.update(*t, fAnMode == 0, now);
        constexpr int cols = 402;
        static float xs[cols], ys[cols];
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(fPX, fPY + fPH);
        for (int c = 0; c < cols; ++c)
        {
            const double f0 = 20.0 * std::pow(1000.0, double(c) / cols), f1 = 20.0 * std::pow(1000.0, double(c + 1) / cols);
            xs[c] = fPX + fPW * (float(c) + 0.5f) / float(cols);
            ys[c] = linY(float(fAn.bandDb(f0, f1)), -90.0f, 0.0f);
            lineTo(xs[c], ys[c]);
        }
        lineTo(fPX + fPW, fPY + fPH);
        closePath();
        fillColor(col(theme::kMuted, 0.13f));
        fill();
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int c = 1; c < cols; ++c)
            lineTo(xs[c], ys[c]);
        strokeColor(col(theme::kMuted, 0.45f));
        strokeWidth(1.0f);
        stroke();
        resetScissor();
    }

    // the spectrum selector at the top right of the display (an interface setting): POST, PRE, OFF
    float anX() const { return fVX + fVW - 62.0f; }
    float anY() const { return fVY + 7.0f; }
    bool inAnBox(float x, float y) const { return x >= anX() && x < anX() + 58.0f && y >= anY() && y < anY() + 14.0f; }

    void drawAnBox()
    {
        beginPath();
        roundedRect(anX() + 0.5f, anY() + 0.5f, 57.0f, 13.0f, 3.0f);
        fillColor(col(theme::kBackground, 0.85f));
        fill();
        strokeColor(col(fAnHover ? theme::kTrackHover : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(anX() + 29.0f, anY() + 7.0f, ALIGN_CENTER, 6.5f, 1.0f, fAnMode < 2 ? theme::kText : theme::kMuted,
              fAnMode == 0 ? "FFT POST" : fAnMode == 1 ? "FFT PRE" : "FFT OFF", true);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && !presetOpen() && inAnBox(x, y))
        {
            fAnMode = (fAnMode + 1) % 3;
            repaint();
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const bool h = !presetOpen() && inAnBox(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (h != fAnHover)
        {
            fAnHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    // as in parametric_eq_gfx.jsfx: the response of the bands the audio runs (-24..+24 dB), the
    // selected band dashed, a dot per band (filled = selected, hollow = bypassed or not soloed)
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        drawSpectrum();
        gridFreq(36.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nullpeq::BandParams p[nullpeq::kBands];
        nullpeq::bandParams([this](uint32_t i) { return double(value(i)); }, p);
        nullpeq::EqSetup eq;
        eq.compute(p, sr);

        const int sel = page();
        dashedV(freqX(float(p[sel].freq)));

        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(eq.responseDb(f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));

        for (int b = 0; b < nullpeq::kBands; ++b)
        {
            const double f = std::min(20000.0, p[b].freq);
            const float x = freqX(float(f)), y = linY(float(eq.responseDb(f, sr)), -24.0f, 24.0f);
            beginPath();
            if (b == sel)
            {
                circle(x, y, 4.0f);
                fillColor(col(theme::kAccent));
                fill();
            }
            else if (eq.active[b])
            {
                circle(x, y, 3.0f);
                fillColor(col(theme::kAccent));
                fill();
            }
            else
            {
                circle(x, y, 3.0f);
                strokeColor(col(theme::kMuted));
                strokeWidth(1.0f);
                stroke();
            }
        }
        drawAnBox();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        switch (param % 5)
        {
        case 0:
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
            break;
        case 1: std::snprintf(buf, n, "%+.1f dB", double(v)); break;
        case 2: std::snprintf(buf, n, "%.1f", double(v)); break;
        default: NullKnobsUI::formatValue(param, v, buf, n); break;
        }
    }

private:
    nulleqan::Analyzer fAn;
    int fAnMode = 0; // 0 = post, 1 = pre, 2 = off
    bool fAnHover = false;
};

UI* createUI()
{
    return new UIParametricEq();
}

END_NAMESPACE_DISTRHO
