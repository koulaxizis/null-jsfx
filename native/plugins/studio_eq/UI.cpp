/*
 * NULL JSFX - Studio EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StudioEqDSP.hpp"
#include "StudioEqPlugin.hpp"

#include <chrono>

START_NAMESPACE_DISTRHO

#define NULL_SEQ_CONTROLS(b)                                              \
    { b * 6 + 0, NullKnobsUI::kSelect, "TYPE", 0, b, false },             \
    { b * 6 + 1, NullKnobsUI::kKnob, "FREQ", 0, b, false, true },         \
    { b * 6 + 2, NullKnobsUI::kKnob, "GAIN", 0, b, true },                \
    { b * 6 + 3, NullKnobsUI::kKnob, "Q", 0, b, false, true },            \
    { b * 6 + 4, NullKnobsUI::kSelect, "SLOPE", 0, b, false },            \
    { b * 6 + 5, NullKnobsUI::kSelect, "CHANNELS", 0, b, false }

// one tab per band, then Output, Auto Gain and Phase on every tab, as in studio_eq_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    NULL_SEQ_CONTROLS(0), NULL_SEQ_CONTROLS(1), NULL_SEQ_CONTROLS(2), NULL_SEQ_CONTROLS(3),
    NULL_SEQ_CONTROLS(4), NULL_SEQ_CONTROLS(5), NULL_SEQ_CONTROLS(6), NULL_SEQ_CONTROLS(7),
    { nullseq::kOutput, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
    { nullseq::kAutoGain, NullKnobsUI::kSelect, "AUTO GAIN", 1, -1, false },
    { nullseq::kPhase, NullKnobsUI::kSelect, "PHASE", 1, -1, false },
};

#undef NULL_SEQ_CONTROLS

class UIStudioEq : public NullKnobsUI
{
public:
    UIStudioEq()
        : NullKnobsUI({ "STUDIO EQ", "ESSENTIALS", 480, 180, "BAND 1|BAND 2|BAND 3|BAND 4|BAND 5|BAND 6|BAND 7|BAND 8", 2,
                        nullseq::kSliders, nullseq::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nulleqan::Tap* tap() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<StudioEqBase*>(p)->analyzer : nullptr;
#else
        return nullptr;
#endif
    }

    double sampleRate() const
    {
        const double sr = getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    // the bands as the audio has them, from the interface's values
    nullseq::Params params() const
    {
        return nullseq::params([this](uint32_t i) { return nullseq::sliderValue(i, value(i)); });
    }

    // the spectrum behind the curve (POST or PRE), as sq_spectrum in the GFX JSFX
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

    // a band's node: at its gain for a bell or shelf, else on the curve (sq_node_y)
    float nodeX(int b) const { return freqX(std::min(20000.0f, value(uint32_t(b * 6 + 1)))); }
    float nodeY(const nullseq::Bands& bands, int b) const
    {
        const float tp = value(uint32_t(b * 6));
        if (tp >= 1.0f && tp <= 3.0f)
            return linY(value(uint32_t(b * 6 + 2)), -24.0f, 24.0f);
        const double f = std::min(20000.0, std::max(20.0, double(value(uint32_t(b * 6 + 1)))));
        return linY(float(bands.responseDb(f, sampleRate())), -24.0f, 24.0f);
    }

    // drag a node: across for the frequency, up and down for the gain of a bell or shelf
    void dragTo(float x, float y)
    {
        const int b = fDrag - 1;
        const float f = std::floor(std::min(30000.0f, std::max(10.0f, 20.0f * std::pow(1000.0f, (x - fPX) / fPW))) + 0.5f);
        setFromUI(uint32_t(b * 6 + 1), f);
        const float tp = value(uint32_t(b * 6));
        if (tp >= 1.0f && tp <= 3.0f)
            setFromUI(uint32_t(b * 6 + 2),
                      std::floor(std::min(24.0f, std::max(-24.0f, 24.0f - 48.0f * (y - fPY) / fPH)) * 10.0f + 0.5f) / 10.0f);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && !presetOpen())
        {
            if (inAnBox(x, y))
            {
                fAnMode = (fAnMode + 1) % 3;
                repaint();
                return true;
            }
            nullseq::Bands bands;
            bands.compute(params(), sampleRate());
            float best = 8.0f;
            int hit = 0;
            for (int b = 0; b < nullseq::kBands; ++b)
            {
                const float d = std::hypot(x - nodeX(b), y - nodeY(bands, b));
                if (d < best)
                {
                    best = d;
                    hit = b + 1;
                }
            }
            if (hit)
            {
                fDrag = hit;
                setPage(hit - 1);
                editParameter(uint32_t((hit - 1) * 6 + 1), true);
                editParameter(uint32_t((hit - 1) * 6 + 2), true);
                return true;
            }
        }
        if (ev.button == 1 && !ev.press && fDrag)
        {
            editParameter(uint32_t((fDrag - 1) * 6 + 1), false);
            editParameter(uint32_t((fDrag - 1) * 6 + 2), false);
            fDrag = 0;
            repaint();
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (fDrag)
        {
            dragTo(x, y);
            return true;
        }
        const bool h = !presetOpen() && inAnBox(x, y);
        if (h != fAnHover)
        {
            fAnHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    // as in studio_eq_gfx.jsfx: the bands' response (-24..+24 dB) over the spectrum, the selected
    // band dashed, a node per band (filled = on, hollow = off; M, S, L or R above a band that is
    // not stereo), the phase mode and auto gain at the top left
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        drawSpectrum();
        gridFreq(36.0f);

        const double sr = sampleRate();
        const nullseq::Params p = params();
        nullseq::Bands bands;
        bands.compute(p, sr);

        const int sel = page();
        dashedV(std::floor(freqX(value(uint32_t(sel * 6 + 1)))) + 0.5f);

        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(bands.responseDb(f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));

        static const char* const kTags[] = { "", "M", "S", "L", "R" };
        for (int b = 0; b < nullseq::kBands; ++b)
        {
            const float x = nodeX(b), y = nodeY(bands, b);
            const bool on = bands.ns[b] > 0;
            const uint32_t c = on ? theme::kAccent : theme::kMuted;
            beginPath();
            circle(x, y, b == sel ? 4.0f : 3.0f);
            if (b == sel || on)
            {
                fillColor(col(c));
                fill();
            }
            else
            {
                strokeColor(col(c));
                strokeWidth(1.0f);
                stroke();
            }
            const int h = bands.ch[b];
            if (on && h > 0 && h < 5)
                label(x, y - 9.0f, ALIGN_CENTER, 6.5f, 0.0f, c, kTags[h], true);
        }

        char st[48];
        const char* mode = p.linear ? "LINEAR PHASE" : "ZERO LATENCY";
        if (p.autoGain)
            std::snprintf(st, sizeof(st), "%s   AUTO %+.1f dB", mode, bands.autoGain(p, sr));
        else
            std::snprintf(st, sizeof(st), "%s", mode);
        label(fVX + 30.0f, fVY + 14.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kMuted, st, true);

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

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param < 48 && param % 6 == 1)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if ((param < 48 && param % 6 == 2) || param == nullseq::kOutput)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else if (param < 48 && param % 6 == 3)
            std::snprintf(buf, n, "%.2f", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    nulleqan::Analyzer fAn;
    int fAnMode = 0; // 0 = post, 1 = pre, 2 = off
    bool fAnHover = false;
    int fDrag = 0;   // the dragged band + 1
};

UI* createUI()
{
    return new UIStudioEq();
}

END_NAMESPACE_DISTRHO
