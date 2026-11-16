/*
 * NULL JSFX - Volume Shaper (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "VolumeShaperDSP.hpp"
#include "VolumeShaperSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SHAPE", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "RATE", 0, -1, false },
    { 6, NullKnobsUI::kSelect, "TRIGGER", 0, -1, false },
    { 7, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 8, NullKnobsUI::kSelect, "CUSTOM", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DEPTH", 1, -1, false },
    { 3, NullKnobsUI::kKnob, "SMOOTH", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "PHASE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIVolumeShaper : public NullKnobsUI
{
public:
    UIVolumeShaper()
        : NullKnobsUI({ "VOLUME SHAPER", "ESSENTIALS", 480, 120, nullptr, 2, nullvolshaper::kSliders,
                        nullvolshaper::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in volume_shaper_gfx.jsfx: the gain over one cycle (0 to 100%), the playhead dashed
    // with a dot at the current gain; below, the cycle length (and trigger) and the gain in dB.
    // With Custom on, a dot on each of the 16 points; click and drag in the display draws them.
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(8, 4, false);
        label(fPX - 6.0f, std::floor(fPY) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "100%");
        label(fPX - 6.0f, std::floor(fPY + fPH * 0.5f) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "50%");
        label(fPX - 6.0f, std::floor(fPY + fPH) + 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");

        const nullvolshaper::Params p = nullvolshaper::paramsFrom([this](uint32_t i) { return value(i); });
        char buf[48], note[16];
        const int rate = std::max(0, std::min(6, p.rate));
        if (!p.custom && p.shape == nullvolshaper::kHalfTime)
            std::snprintf(note, sizeof(note), "2 x %s", nullvolshaper::kRateOptions[rate]);
        else
            std::snprintf(note, sizeof(note), "%s", nullvolshaper::kRateOptions[rate]);
        if (p.trigger == nullvolshaper::kMidi)
            std::snprintf(buf, sizeof(buf), "ONE SHOT = %s ON MIDI NOTES", note);
        else if (p.trigger == nullvolshaper::kSidechain)
            std::snprintf(buf, sizeof(buf), "ONE SHOT = %s ON KEY > %d dB", note, int(p.thresholdDb));
        else
            std::snprintf(buf, sizeof(buf), "ONE CYCLE = %s", note);
        label(fPX, fPY + fPH + 11.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);

        float pos = 0.0f, g = 1.0f;
        if (nulljsfx::Taps* t = taps())
        {
            pos = clampf(t->extra.load(std::memory_order_relaxed), 0.0f, 1.0f);
            g = t->gr.load(std::memory_order_relaxed);
        }
        const double mix = p.mixPct / 100.0;
        const double db = 20.0 * std::log10(std::fmax(double(g) * mix + 1.0 - mix, 0.0000001));
        if (db < -60.0)
            std::snprintf(buf, sizeof(buf), "GAIN -INF dB");
        else
            std::snprintf(buf, sizeof(buf), "GAIN %.1f dB", db);
        label(fPX + fPW, fPY + fPH + 11.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        // the curve: Depth and Phase applied, two points per design unit so the gates stay sharp
        const nullvolshaper::Shape custom = nullvolshaper::customShape(p);
        const nullvolshaper::Shape& sh = nullvolshaper::playedShape(p, custom);
        const double depth = p.depthPct / 100.0, phase = p.phasePct / 100.0;
        // a one-shot (Trigger MIDI or Sidechain) stays at full level after its end
        const double osEnd = p.trigger != nullvolshaper::kTempo ? nullvolshaper::oneShotEnd(sh) : 2.0;
        constexpr int kMaxN = 1024;
        const int n = std::min(kMaxN, int(fPW * 2.0f) + 1);
        float xs[kMaxN], ys[kMaxN];
        for (int i = 0; i < n; ++i)
        {
            const double x = double(i) / double(n - 1) * 0.999999;
            double q = x + phase;
            if (q >= 1.0)
                q -= 1.0;
            const double gain = x >= osEnd ? 1.0 : 1.0 - depth * (1.0 - nullvolshaper::Shapes::evalShape(sh, q));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(gain), 0.0f, 1.0f);
        }
        curve(xs, ys, n, fPY + fPH);

        if (p.custom)
        {
            for (int k = 0; k < nullvolshaper::kPoints; ++k)
            {
                double q = double(k) / 16.0 - phase;
                if (q < 0.0)
                    q += 1.0;
                beginPath();
                circle(fPX + float(q) * fPW, linY(float(1.0 - depth * (1.0 - p.points[k] / 100.0)), 0.0f, 1.0f), 2.5f);
                fillColor(col(theme::kText));
                fill();
            }
        }

        // playhead and the current (smoothed) gain
        const float x = std::floor(fPX + pos * fPW) + 0.5f;
        dashedV(x);
        beginPath();
        circle(x, linY(g, 0.0f, 1.0f), 3.0f);
        fillColor(col(theme::kText));
        fill();
    }

    bool inPlot(float x, float y) const
    {
        return value(8) >= 0.5f && x >= fPX && x <= fPX + fPW && y >= fPY && y <= fPY + fPH;
    }

    // the point nearest to x (Phase taken into account) gets the level under y (Depth undone)
    void drawPoint(float x, float y)
    {
        const double depth = value(2) / 100.0;
        double q = std::max(0.0, std::min(0.999999, double(x - fPX) / double(fPW))) + value(4) / 100.0;
        if (q >= 1.0)
            q -= 1.0;
        const int k = int(std::floor(q * 16.0 + 0.5)) % 16;
        double lv = 1.0 - std::max(0.0, std::min(1.0, double(y - fPY) / double(fPH)));
        if (depth > 0.0)
            lv = 1.0 - (1.0 - lv) / depth;
        const float v = float(std::floor(std::max(0.0, std::min(1.0, lv)) * 100.0 + 0.5));
        if (v != value(uint32_t(9 + k)))
            setGesture(uint32_t(9 + k), v);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == kMouseButtonLeft && ev.press && inPlot(x, y))
        {
            fPointDrag = true;
            drawPoint(x, y);
            return true;
        }
        if (ev.button == kMouseButtonLeft && !ev.press && fPointDrag)
        {
            fPointDrag = false;
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fPointDrag)
        {
            const float s = uiScale();
            drawPoint(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
            return true;
        }
        return NullKnobsUI::onMotion(ev);
    }

private:
    bool fPointDrag = false;
};

UI* createUI()
{
    return new UIVolumeShaper();
}

END_NAMESPACE_DISTRHO
