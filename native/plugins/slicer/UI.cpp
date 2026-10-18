/*
 * NULL JSFX - Slicer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SlicerDSP.hpp"
#include "SlicerSliders.hpp"

START_NAMESPACE_DISTRHO

// as slicer_gfx.jsfx: pattern, variation, seed, slices / reverse, repeat, gate, mix
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "PATTERN", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "VARIATION", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SEED", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "SLICES", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "REVERSE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "REPEAT", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "GATE", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UISlicer : public NullKnobsUI
{
public:
    UISlicer()
        : NullKnobsUI({ "SLICER", "ADVANCED", 480, 120, nullptr, 2, nullslicer::kUiSliders, nullslicer::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    void box(float x, float y, float w, float h, uint32_t rgb, float alpha)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    // as in slicer_gfx.jsfx: the slices of the bar in playing order, each with the number of
    // the slice of the previous bar it plays (brighter when changed) and its marks (< reversed,
    // x2 repeated, << backspin); the filled part is the Gate; the playhead runs across the bar
    void drawDisplay() override
    {
        const nullslicer::Params prm = nullslicer::paramsFrom([this](uint32_t i) { return value(i); });
        const nullslicer::Pattern pat = nullslicer::makePattern(prm);
        float pos = 0.0f;
        bool full = false;
        if (nulljsfx::Taps* t = taps())
        {
            pos = clampf(t->extra.load(std::memory_order_relaxed), 0.0f, 1.0f);
            full = t->aux[0].load(std::memory_order_relaxed) >= 0.5f;
        }
        const int ns = pat.ns;
        const float cw = 420.0f / ns;
        const int cur = std::min(ns - 1, int(std::floor(pos * ns)));
        static const char* const kPat[] = { "STRAIGHT", "SHUFFLE", "REVERSE PAIRS", "STUTTER", "BACKSPIN", "RANDOM" };
        char line[96];
        if (prm.pattern == 0)
            std::snprintf(line, sizeof(line), "%s  SEED %d", kPat[0], int(prm.seed));
        else
            std::snprintf(line, sizeof(line), "%s %d%%  SEED %d", kPat[prm.pattern % 6], int(prm.variation), int(prm.seed));
        label(30.0f, 70.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, line);
        std::snprintf(line, sizeof(line), full ? "PREVIOUS BAR  %d SLICES" : "RECORDING FIRST BAR  %d SLICES", ns);
        label(450.0f, 70.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);

        const float gate = float(prm.gatePct / 100.0);
        for (int j = 0; j < ns; ++j)
        {
            const float x = 30.0f + j * cw;
            box(x + 1.0f, 80.0f, cw - 2.0f, 50.0f, j == cur ? theme::kBorder : theme::kBackground, 1.0f);
            box(x + 1.0f, 80.0f, (cw - 2.0f) * gate, 50.0f, theme::kAccent, j == cur ? 0.34f : 0.16f);
            const bool ch = pat.src[j] != j || pat.rv[j] != 0.0 || pat.rp[j] != 0.0 || pat.bs[j] != 0.0;
            char num[16];
            if (pat.bs[j] != 0.0)
                std::snprintf(num, sizeof(num), "-");
            else
                std::snprintf(num, sizeof(num), "%d", int(pat.src[j]) + 1);
            label(x + cw * 0.5f, 98.0f, ALIGN_CENTER, ns > 16 ? 7.0f : 8.0f, 0.0f, ch ? theme::kText : theme::kMuted, num, ch);
            if (pat.bs[j] != 0.0 || pat.rv[j] != 0.0)
                label(x + cw * 0.5f, 113.0f, ALIGN_CENTER, 7.0f, 0.0f, ch ? theme::kText : theme::kMuted,
                      pat.bs[j] != 0.0 ? "<<" : "<");
            if (pat.rp[j] != 0.0)
                label(x + cw * 0.5f, 123.0f, ALIGN_CENTER, 7.0f, 0.0f, ch ? theme::kText : theme::kMuted, "x2");
        }
        box(std::floor(30.0f + 420.0f * pos), 78.0f, 1.0f, 54.0f, theme::kText, 1.0f);

        char b[16];
        for (int k = 0; k < 4; ++k)
        {
            std::snprintf(b, sizeof(b), "%d", k + 1);
            label(31.0f + k * 105.0f, 141.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, b);
        }
        label(30.0f, 156.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "<  REVERSED    x2  REPEATED    <<  BACKSPIN");
        std::snprintf(line, sizeof(line), "GATE %d%%", int(prm.gatePct));
        label(450.0f, 156.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);
    }
};

UI* createUI()
{
    return new UISlicer();
}

END_NAMESPACE_DISTRHO
