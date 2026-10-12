/*
 * NULL JSFX - Mid/Side Matrix (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "MidSideMatrixDSP.hpp"
#include "MidSideMatrixSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "MID", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "SIDE", 0, -1, true },
    { 2, NullKnobsUI::kSelect, "MODE", 0, -1, false },
};

class UIMidSideMatrix : public NullKnobsUI
{
public:
    UIMidSideMatrix()
        : NullKnobsUI({ "MID/SIDE MATRIX", "ESSENTIALS", 480, 120, nullptr, 1, nullmidside::kSliders,
                        nullmidside::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in mid_side_matrix_gfx.jsfx: left, vectorscope of the input; right, the matrix with
    // the current gains and the levels of the two outputs
    void drawDisplay() override
    {
        setPlot(32.0f, 66.0f, 96.0f, 96.0f);
        vectorscope(2.0f);
        label(80.0f, 158.0f, ALIGN_CENTER, 8.0f, 1.0f, theme::kMuted, "INPUT");

        const bool decode = std::lround(value(2)) != 0;
        const double mg = nullmidside::gainFor(nullmidside::tenth(value(0)));
        const double sg = nullmidside::gainFor(nullmidside::tenth(value(1)));
        label(160.0f, 74.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted, decode ? "DECODE  M/S TO L/R" : "ENCODE  L/R TO M/S", true);
        char f1[64], f2[64];
        if (decode)
        {
            std::snprintf(f1, sizeof(f1), "OUT L  =  0.707 x (M x %.2f + S x %.2f)", mg, sg);
            std::snprintf(f2, sizeof(f2), "OUT R  =  0.707 x (M x %.2f - S x %.2f)", mg, sg);
        }
        else
        {
            std::snprintf(f1, sizeof(f1), "OUT L  =  0.707 x (L + R) x %.2f", mg);
            std::snprintf(f2, sizeof(f2), "OUT R  =  0.707 x (L - R) x %.2f", sg);
        }
        label(160.0f, 94.0f, ALIGN_LEFT, 9.0f, 0.0f, theme::kText, f1);
        label(160.0f, 110.0f, ALIGN_LEFT, 9.0f, 0.0f, theme::kText, f2);

        nulljsfx::Taps* t = taps();
        const float l = t ? t->inLevel.load(std::memory_order_relaxed) : 0.0f;
        const float r = t ? t->outLevel.load(std::memory_order_relaxed) : 0.0f;
        meter(160.0f, 128.0f, 286.0f, 8.0f, decode ? "L" : "MID", nulljsfx::toDb(l), -140.0f, -60.0f);
        meter(160.0f, 144.0f, 286.0f, 8.0f, decode ? "R" : "SIDE", nulljsfx::toDb(r), -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double d = nullmidside::tenth(v) + 0.0; // + 0.0 turns -0.0 into 0.0
        std::snprintf(buf, n, d > 0.0 ? "+%.1f dB" : "%.1f dB", d);
        (void)param;
    }
};

UI* createUI()
{
    return new UIMidSideMatrix();
}

END_NAMESPACE_DISTRHO
