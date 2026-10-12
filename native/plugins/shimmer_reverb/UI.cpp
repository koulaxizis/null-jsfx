/*
 * NULL JSFX - Shimmer Reverb (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ShimmerReverbDSP.hpp"
#include "ShimmerReverbSliders.hpp"

START_NAMESPACE_DISTRHO

// the selector shows the interval only (the host keeps the JSFX's full option names)
static const char* const kPitchShort[] = { "+12", "+7", "+19", "+24", "-12" };
static const SliderInfo kUiSliders[] = {
    nullshimmer::kSliders[0], nullshimmer::kSliders[1], nullshimmer::kSliders[2],
    { "Pitch", "pitch", 0.0f, 4.0f, 0.0f, "", 1.0f, kPitchShort },
    nullshimmer::kSliders[4], nullshimmer::kSliders[5],
};

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SIZE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SHIMMER", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "PITCH", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DAMPING", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 0, -1, false },
};

class UIShimmerReverb : public NullKnobsUI
{
public:
    UIShimmerReverb()
        : NullKnobsUI({ "SHIMMER REVERB", "ADVANCED", 480, 120, nullptr, 1, kUiSliders, nullshimmer::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // log-frequency lines (20 Hz to 20 kHz), lines at 0, -24 and -48 dB, and labels
    void freqGrid()
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
        beginPath();
        for (int k = 0; k <= 2; ++k)
        {
            const float y = std::round(fPY + fPH * float(k) * 0.5f) + 0.5f;
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
        }
        strokeColor(col(theme::kBorder, 0.6f));
        stroke();
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-48");
        label(freqX(100.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");
    }

    // as in shimmer_reverb_gfx.jsfx: a 440 Hz note and the partials the shimmer loop stacks on
    // it, each Shimmer lower and darkened by the loop's damping (dashed), 0 to -48 dB
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        freqGrid();
        dashedV(freqX(value(4)));
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
        const float bw = 3.0f;
        beginPath();
        rect(freqX(440.0f) - bw * 0.5f, fPY, bw, fPH);
        fillColor(col(theme::kMuted));
        fill();
        const double ratio = std::pow(2.0, nullshimmer::semitones(int(std::lround(value(3)))) / 12.0);
        const double d = std::exp(-2.0 * M_PI * value(4) / sr), sh = value(2) / 100.0;
        double f = 440.0, g = 1.0;
        beginPath();
        for (int k = 1; k <= 12; ++k)
        {
            f *= ratio;
            g *= sh * (1.0 - d) / std::sqrt(1.0 - 2.0 * d * std::cos(2.0 * M_PI * std::min(f, 0.5 * sr) / sr) + d * d);
            if (f < 20.0 || f > 20000.0 || g <= 0.004)
                break;
            const float y = linY(20.0f * float(std::log10(g)), -48.0f, 0.0f);
            rect(freqX(float(f)) - bw * 0.5f, y, bw, fPY + fPH - y);
        }
        fillColor(col(theme::kAccent));
        fill();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%.1f s", v);
        else if (param == 4)
            std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIShimmerReverb();
}

END_NAMESPACE_DISTRHO
