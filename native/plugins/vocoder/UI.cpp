/*
 * NULL JSFX - Vocoder (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "VocoderDSP.hpp"
#include "VocoderSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "CARRIER", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "PITCH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "FORMANT", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "ATTACK", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "SIBILANCE", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
};

class UIVocoder : public NullKnobsUI
{
public:
    UIVocoder()
        : NullKnobsUI({ "VOCODER", "ADVANCED", 480, 120, nullptr, 2, nullvocoder::kSliders, nullvocoder::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in vocoder_gfx.jsfx: the voice's level per band (0 to -60 dB) at the band centres,
    // ticks at the bottom for the carrier bands (moved by Formant Shift)
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(1, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-30");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-60");
        label(freqX(100.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(8000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "8k");

        nulljsfx::Taps* t = taps();
        const float bw = 6.0f, shift = std::pow(2.0f, value(2) / 12.0f), bottom = fPY + fPH;
        for (int i = 0; i < nullvocoder::kBands; ++i)
        {
            const float f = float(nullvocoder::bandFreq(i));
            const float env = t ? t->scopeL[i] : 0.0f;
            const float y = linY(20.0f * std::log10(std::fmax(env, 0.000001f)), -60.0f, 0.0f);
            if (y < bottom - 1.0f)
            {
                beginPath();
                rect(freqX(f) - bw * 0.5f, y, bw, bottom - y);
                fillColor(col(theme::kAccent));
                fill();
            }
            const float cx = std::round(freqX(f * shift)) + 0.5f;
            beginPath();
            moveTo(cx, bottom - 1.0f);
            lineTo(cx, bottom - 7.0f);
            strokeColor(col(theme::kMuted));
            strokeWidth(1.0f);
            stroke();
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1 || param == 2 || param == 3 || param == 7)
            std::snprintf(buf, n, "%.1f %s", double(v), kSlidersUnit(param));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    static const char* kSlidersUnit(uint32_t p) { return nullvocoder::kSliders[p].unit; }
};

UI* createUI()
{
    return new UIVocoder();
}

END_NAMESPACE_DISTRHO
