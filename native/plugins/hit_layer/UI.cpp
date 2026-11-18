/*
 * NULL JSFX - Hit Layer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "HitLayerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "TONE", 0, -1, false, true },
    { 2, NullKnobsUI::kKnob, "TONE LVL", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "TONE DECAY", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "DROP", 0, -1, false },
    { 8, NullKnobsUI::kKnob, "FOLLOW", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "NOISE LVL", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "NOISE DECAY", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "COLOR", 1, -1, false, true },
    { 9, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "DRY", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIHitLayer : public NullKnobsUI
{
public:
    UIHitLayer()
        : NullKnobsUI({ "HIT LAYER", "ESSENTIALS", 480, 120, nullptr, 2, nullhitlayer::kSliders, nullhitlayer::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in hit_layer_gfx.jsfx: left, the layer's envelopes over the longer decay (tone bright,
    // noise dim); right, a lamp for each hit, the detector level with the threshold, the layer
    void drawDisplay() override
    {
        setPlot(56.0f, 66.0f, 260.0f, 78.0f);
        grid(4, 4, false);
        const float T = std::fmax(value(3), value(6)) / 1000.0f;
        char buf[24];
        const float ly = fPY + fPH + 10.0f;
        label(fPX, ly, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "0");
        if (T >= 1.0f)
            std::snprintf(buf, sizeof(buf), "%.1f s", T);
        else
            std::snprintf(buf, sizeof(buf), "%d ms", int(std::floor(T * 1000.0f + 0.5f)));
        label(fPX + fPW, ly, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        label(fPX + fPW * 0.5f, ly, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "NOISE");

        const int n = int(std::floor(fPW)) + 1;
        float xs[kMaxPts], yt[kMaxPts], yn[kMaxPts];
        const int m = n < kMaxPts ? n : kMaxPts;
        const float td = value(3) / 1000.0f, nd = value(6) / 1000.0f;
        for (int i = 0; i < m; ++i)
        {
            const float t = T * float(i) / fPW;
            xs[i] = fPX + float(i);
            yt[i] = linY(std::exp(-6.907755f * t / td) * value(2) / 100.0f, 0.0f, 1.0f);
            yn[i] = linY(std::exp(-6.907755f * t / nd) * value(5) / 100.0f, 0.0f, 1.0f);
        }
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(xs[0], yn[0]);
        for (int i = 1; i < m; ++i)
            lineTo(xs[i], yn[i]);
        strokeColor(col(theme::kAccent, 0.5f));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        resetScissor();
        curve(xs, yt, m);

        float in = 0.0f, layer = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            in = t->inLevel.load(std::memory_order_relaxed);
            layer = t->outLevel.load(std::memory_order_relaxed);
            if (t->extra.exchange(0.0f, std::memory_order_relaxed) > 0.5f)
                fFlash = 1.0f;
        }
        // the HIT lamp: lit on a hit, then falls off by 0.8 per frame (as hl_flash)
        fontFace("noto-bold");
        fontSize(7.0f * kNotoLineToEm);
        textLetterSpacing(1.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(col(theme::kAccent, fFlash > 0.05f ? fFlash : 0.25f));
        text(336.0f, 66.0f, "HIT", nullptr);
        textLetterSpacing(0.0f);
        fontFace("noto");
        fFlash *= 0.8f;
        meter(336.0f, 82.0f, 112.0f, 10.0f, "IN", nulljsfx::toDb(in), value(0), -60.0f);
        meter(336.0f, 106.0f, 112.0f, 10.0f, "LAYER", nulljsfx::toDb(layer), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 11)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 1)
            std::snprintf(buf, n, "%d Hz", int(v));
        else if (param == 3 || param == 6)
            std::snprintf(buf, n, "%d ms", int(v));
        else if (param == 4)
            std::snprintf(buf, n, "%.1f st", v);
        else if (param == 7)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d Hz", int(v));
        }
        else
            std::snprintf(buf, n, "%d%%", int(v));
    }

private:
    static constexpr int kMaxPts = 512;
    float fFlash = 0.0f;
};

UI* createUI()
{
    return new UIHitLayer();
}

END_NAMESPACE_DISTRHO
