/*
 * NULL JSFX - Spectral Ducker (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpectralDuckerPlugin.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "AMOUNT", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "ATTACK", 0, -1, false, true },
    { 3, NullKnobsUI::kKnob, "RELEASE", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "LOW CUT", 0, -1, false, true },
    { 5, NullKnobsUI::kKnob, "HIGH CUT", 0, -1, false, true },
    { 6, NullKnobsUI::kSelect, "CHANNELS", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
    { 9, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
};

class UISpectralDucker : public NullKnobsUI
{
public:
    UISpectralDucker()
        : NullKnobsUI({ "SPECTRAL DUCKER", "ADVANCED", 480, 120, nullptr, 2, nullsduck::kSliders, nullsduck::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    SpectralDuckerBase* plugin() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        return static_cast<SpectralDuckerBase*>(static_cast<Plugin*>(getPluginInstancePointer()));
#else
        return nullptr;
#endif
    }

    float bandX(float k) const { return fPX + fPW * (k + 0.5f) / 32.0f; }
    float freqBandX(float f) const { return bandX(31.0f * std::log(f / 30.0f) / std::log(600.0f)); }

    // as in spectral_ducker_gfx.jsfx: per band the key's level from the bottom (-60..0 dB,
    // Threshold dashed) and the cut of the track from the top (0..24 dB); bands outside
    // Low Cut .. High Cut dimmed
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        beginPath();
        rect(fPX, fPY, fPW, 1.0f);
        rect(fPX, fPY + fPH, fPW, 1.0f);
        fillColor(col(theme::kBorder));
        fill();

        SpectralDuckerBase* p = plugin();
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        float maxGr = 0.0f;
        for (int k = 0; k < nullsduck::kBands; ++k)
        {
            const double f = nullsduck::bandFreq(k);
            const bool on = f >= nullsduck::sliderValue(4, value(4)) && f <= nullsduck::sliderValue(5, value(5)) && f < sr * 0.45;
            const float x0 = std::floor(fPX + fPW * k / 32.0f) + 1.0f, x1 = std::floor(fPX + fPW * (k + 1) / 32.0f) - 1.0f;
            const float key = p ? p->bandKey[k].load(std::memory_order_relaxed) : 0.0f;
            const float gr = p ? p->bandGr[k].load(std::memory_order_relaxed) : 0.0f;
            maxGr = std::max(maxGr, gr);
            const float y = linY(20.0f * std::log10(std::max(key, 1e-9f)), -60.0f, 0.0f);
            if (y < fPY + fPH)
            {
                beginPath();
                rect(x0, y, std::max(1.0f, x1 - x0), fPY + fPH - y);
                fillColor(col(theme::kMuted, on ? 0.35f : 0.12f));
                fill();
            }
            const float h = fPH * std::min(1.0f, gr / 24.0f);
            if (h >= 1.0f)
            {
                beginPath();
                rect(x0, fPY, std::max(1.0f, x1 - x0), h);
                fillColor(col(theme::kAccent));
                fill();
            }
        }
        // Threshold on the key scale
        const float ty = std::floor(linY(value(1), -60.0f, 0.0f)) + 0.5f;
        beginPath();
        for (float x = fPX; x < fPX + fPW; x += 6.0f)
        {
            moveTo(x, ty);
            lineTo(std::min(x + 3.0f, fPX + fPW), ty);
        }
        strokeColor(col(theme::kAccent, 0.5f));
        strokeWidth(1.0f);
        stroke();

        label(freqBandX(100.0f), fPY + fPH + 8.0f, ALIGN_CENTER, 6.5f, 0.0f, theme::kMuted, "100");
        label(freqBandX(1000.0f), fPY + fPH + 8.0f, ALIGN_CENTER, 6.5f, 0.0f, theme::kMuted, "1k");
        label(freqBandX(10000.0f), fPY + fPH + 8.0f, ALIGN_CENTER, 6.5f, 0.0f, theme::kMuted, "10k");
        label(fPX - 4.0f, fPY + 5.0f, ALIGN_RIGHT, 6.5f, 0.0f, theme::kMuted, "CUT");
        label(fPX - 4.0f, fPY + fPH - 5.0f, ALIGN_RIGHT, 6.5f, 0.0f, theme::kMuted, "KEY");

        char st[48];
        const int ch = int(std::lround(value(6)));
        std::snprintf(st, sizeof(st), "%s   -%.1f dB", value(9) >= 0.5f ? "DELTA" : ch == 1 ? "MID" : ch == 2 ? "SIDE" : "STEREO",
                      double(maxGr));
        label(fVX + fVW - 4.0f, fVY + 14.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, st, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4 || param == 5)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 8)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISpectralDucker();
}

END_NAMESPACE_DISTRHO
