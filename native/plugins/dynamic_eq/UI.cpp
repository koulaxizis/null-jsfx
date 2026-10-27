/*
 * NULL JSFX - Dynamic EQ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DynamicEqDSP.hpp"
#include "DynamicEqSliders.hpp"

START_NAMESPACE_DISTRHO

#define NULL_DEQ_CONTROLS(b)                                          \
    { b * 4 + 0, NullKnobsUI::kKnob, "FREQ", 0, b, false },           \
    { b * 4 + 1, NullKnobsUI::kKnob, "GAIN", 0, b, true },            \
    { b * 4 + 2, NullKnobsUI::kKnob, "Q", 0, b, false },              \
    { b * 4 + 3, NullKnobsUI::kKnob, "THRESHOLD", 0, b, false }

// one tab per band, attack / release / makeup under every tab, as in dynamic_eq_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    NULL_DEQ_CONTROLS(0), NULL_DEQ_CONTROLS(1), NULL_DEQ_CONTROLS(2), NULL_DEQ_CONTROLS(3),
    { 16, NullKnobsUI::kKnob, "ATTACK", 1, -1, false },
    { 17, NullKnobsUI::kKnob, "RELEASE", 1, -1, false },
    { 18, NullKnobsUI::kKnob, "MAKEUP", 1, -1, false },
    { 19, NullKnobsUI::kSelect, "CHANNELS", 1, -1, false },
    { 20, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
};

#undef NULL_DEQ_CONTROLS

class UIDynamicEq : public NullKnobsUI
{
public:
    UIDynamicEq()
        : NullKnobsUI({ "DYNAMIC EQ", "ESSENTIALS", 480, 140, "BAND 1|BAND 2|BAND 3|BAND 4", 2,
                        nulldeq::kSliders, nulldeq::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in dynamic_eq_gfx.jsfx: the curve with every band fully engaged (muted) and the curve as
    // the band levels engage it now (accent), makeup included (-16..+16 dB); selected band dashed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(24.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nulldeq::Params p = nulldeq::params([this](uint32_t i) { return double(value(i)); });
        nulldeq::Band bands[nulldeq::kBands];
        nulldeq::setupBands(p, sr, bands);

        // the band envelopes from the plugin (inLevel, outLevel, gr, extra = band 1..4)
        double over[nulldeq::kBands] = {};
        if (nulljsfx::Taps* t = taps())
        {
            const float env[nulldeq::kBands] = { t->inLevel.load(std::memory_order_relaxed),
                                                 t->outLevel.load(std::memory_order_relaxed),
                                                 t->gr.load(std::memory_order_relaxed),
                                                 t->extra.load(std::memory_order_relaxed) };
            for (int b = 0; b < nulldeq::kBands; ++b)
                over[b] = bands[b].over(double(env[b]));
        }
        auto total = [&](double f, bool live) {
            const double w = 2.0 * M_PI * std::min(f, sr * 0.4999) / sr;
            double db = p.makeupDb;
            for (int b = 0; b < nulldeq::kBands; ++b)
                db += bands[b].magnitudeDb(w, live ? over[b] : 1.0);
            return float(db);
        };

        const int sel = page();
        dashedV(freqX(float(p.freq[sel])));

        constexpr int n = 404;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(total(f, false), -16.0f, 16.0f);
        }
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(col(theme::kMuted, 0.8f));
        strokeWidth(1.0f);
        stroke();
        resetScissor();

        for (int i = 0; i < n; ++i)
            ys[i] = linY(total(20.0 * std::pow(1000.0, double(i) / double(n - 1)), true), -16.0f, 16.0f);
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));

        beginPath();
        for (int b = 0; b < nulldeq::kBands; ++b)
            circle(freqX(float(p.freq[b])), linY(total(p.freq[b], true), -16.0f, 16.0f), b == sel ? 4.0f : 3.0f);
        fillColor(col(theme::kAccent));
        fill();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param < 16)
        {
            switch (param % 4)
            {
            case 0:
                if (v >= 1000.0f)
                    std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
                else
                    std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
                return;
            case 1: std::snprintf(buf, n, "%+.1f dB", double(v)); return;
            case 2: std::snprintf(buf, n, "%.1f", double(v)); return;
            }
        }
        else if (param == 16)
        {
            std::snprintf(buf, n, "%.1f ms", double(v));
            return;
        }
        NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDynamicEq();
}

END_NAMESPACE_DISTRHO
