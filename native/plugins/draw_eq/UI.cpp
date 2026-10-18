/*
 * NULL JSFX - Draw EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DrawEqDSP.hpp"
#include "DrawEqPlugin.hpp"

#include <chrono>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { nulldraweq::kSmoothness, NullKnobsUI::kKnob, "SMOOTH", 0, -1, false },
    { nulldraweq::kOutput, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
    { nulldraweq::kMix, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

static const char* const kPointNames[] = { "31 Hz", "63 Hz", "125 Hz", "250 Hz", "500 Hz",
                                           "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz" };

class UIDrawEq : public NullKnobsUI
{
public:
    UIDrawEq()
        : NullKnobsUI({ "DRAW EQ", "ESSENTIALS", 480, 220, nullptr, 1, nulldraweq::kSliders, nulldraweq::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the plot, as in draw_eq_gfx.jsfx: (50, 66, 402, 184), 20 Hz .. 20 kHz, -24..+24 dB
    static constexpr float kPX = 50.0f, kPY = 66.0f, kPW = 402.0f, kPH = 184.0f;

    double sampleRate() const
    {
        const double sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    double gain(int k) const { return nulldraweq::sliderValue(uint32_t(k), value(uint32_t(k))); }
    static float pointX(int k) { return kPX + kPW * float(std::log(nulldraweq::pointFreq(k) / 20.0) / std::log(1000.0)); }
    static float dbAt(float y)
    {
        return clampf(std::floor((24.0f - 48.0f * (y - kPY) / kPH) * 10.0f + 0.5f) / 10.0f, -18.0f, 18.0f);
    }
    static int nearest(float x)
    {
        int k = 0;
        float best = 10000.0f;
        for (int i = 0; i < nulldraweq::kPoints; ++i)
        {
            const float d = std::fabs(x - pointX(i));
            if (d < best)
            {
                best = d;
                k = i;
            }
        }
        return k;
    }
    static bool insidePlot(float x, float y)
    {
        return x >= kPX - 6.0f && x <= kPX + kPW + 6.0f && y >= kPY - 6.0f && y <= kPY + kPH + 6.0f;
    }

    const nulleqan::Tap* tap() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<DrawEqBase*>(p)->analyzer : nullptr;
#else
        return nullptr;
#endif
    }

    // the output spectrum, as dg_spectrum in the GFX JSFX
    void drawSpectrum()
    {
        const nulleqan::Tap* t = tap();
        if (!t)
            return;
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        fAn.update(*t, true, now);
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

    void drawDisplay() override
    {
        setPlot(kPX, kPY, kPW, kPH);
        drawSpectrum();
        gridFreq(36.0f);

        nulldraweq::Params p;
        for (int i = 0; i < nulldraweq::kPoints; ++i)
            p.gain[i] = gain(i);
        p.smooth = nulldraweq::sliderValue(nulldraweq::kSmoothness, value(nulldraweq::kSmoothness));
        const double sr = sampleRate();
        fBank.compute(p, sr);
        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(fBank.responseDb(f)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));

        for (int k = 0; k < nulldraweq::kPoints; ++k)
        {
            const float x = pointX(k), y = linY(float(gain(k)), -24.0f, 24.0f);
            const bool on = nulldraweq::pointFreq(k) < sr * 0.45;
            beginPath();
            circle(x, y, 4.0f);
            fillColor(col(fDrag && nearest(fMX) == k ? theme::kAccent : theme::kCard));
            fill();
            strokeColor(col(fHover == k ? theme::kText : on ? theme::kAccent : theme::kMuted));
            strokeWidth(1.0f);
            stroke();
        }

        if (fHover >= 0)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%s  %+.1f dB", kPointNames[fHover], gain(fHover));
            label(kPX + 6.0f, kPY + 8.0f, ALIGN_LEFT, 7.0f, 0.5f, theme::kText, buf, true);
        }
        label(kPX + kPW - 6.0f, kPY + 8.0f, ALIGN_RIGHT, 7.0f, 0.0f, theme::kMuted, "Drag to draw. Double-click: 0 dB.");
    }

    void setPoint(int k, float db)
    {
        if (std::fabs(value(uint32_t(k)) - db) > 1e-6f)
            setGesture(uint32_t(k), db);
    }

    // every point between the last and this mouse position takes the height on the line between
    void drawTo(float x, float y)
    {
        if (std::fabs(x - fPMX) > 0.001f)
            for (int k = 0; k < nulldraweq::kPoints; ++k)
            {
                const float px = pointX(k);
                if (px >= std::fmin(fPMX, x) && px <= std::fmax(fPMX, x))
                    setPoint(k, dbAt(fPMY + (y - fPMY) * (px - fPMX) / (x - fPMX)));
            }
        setPoint(nearest(x), dbAt(y));
        fPMX = x;
        fPMY = y;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && insidePlot(x, y))
        {
            const int k = nearest(x);
            if (fLastClick != 0 && ev.time - fLastClick < 300 && fLastHit == k)
            {
                fLastClick = 0;
                fDrag = false;
                setPoint(k, 0.0f);
                repaint();
                return true;
            }
            fLastClick = ev.time;
            fLastHit = k;
            fDrag = true;
            fMX = fPMX = x;
            fPMY = y;
            drawTo(x, y);
            repaint();
            return true;
        }
        if (ev.button == 1 && !ev.press && fDrag)
        {
            fDrag = false;
            repaint();
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        fMX = x;
        if (fDrag)
        {
            drawTo(x, y);
            fHover = nearest(x);
            repaint();
            return true;
        }
        const int h = insidePlot(x, y) && !presetOpen() ? nearest(x) : -1;
        if (h != fHover)
        {
            fHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nulldraweq::sliderValue(param, v);
        if (param == nulldraweq::kOutput)
        {
            if (std::fabs(x) < 0.05)
                std::snprintf(buf, n, "0.0 dB");
            else
                std::snprintf(buf, n, "%+.1f dB", x);
        }
        else if (param == nulldraweq::kSmoothness || param == nulldraweq::kMix)
            std::snprintf(buf, n, "%d%%", int(std::lround(x)));
        else
            std::snprintf(buf, n, "%+.1f dB", x);
    }

private:
    nulldraweq::Bank fBank;
    nulleqan::Analyzer fAn;
    bool fDrag = false;
    int fHover = -1, fLastHit = -2;
    float fMX = 0.0f, fPMX = 0.0f, fPMY = 0.0f;
    uint fLastClick = 0;
};

UI* createUI()
{
    return new UIDrawEq();
}

END_NAMESPACE_DISTRHO
