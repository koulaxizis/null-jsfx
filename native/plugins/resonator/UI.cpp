/*
 * NULL JSFX - Resonator (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ResonatorDSP.hpp"
#include "ResonatorSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "ROOT", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "OCTAVE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "CHORD", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DECAY", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "BRIGHT", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "NOTES", 1, -1, false },
};

class UIResonator : public NullKnobsUI
{
public:
    UIResonator()
        : NullKnobsUI({ "RESONATOR", "ADVANCED", 480, 120, nullptr, 2, nullres::kSliders, nullres::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in resonator_gfx.jsfx: the wet gain over frequency, with the resonant peaks (both
    // channels, so Spread shows as pairs) as lines at their exact height, and the note names
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(48.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        double s[nullres::kNumSliders];
        for (uint32_t i = 0; i < nullres::kNumSliders; ++i)
            s[i] = nullres::sliderValue(i, value(i));
        // Notes MIDI: the notes the plugin is tuned to now
        int notes[4] = {}, nNotes = 0;
        if (nulljsfx::Taps* t = taps())
        {
            nNotes = std::max(0, std::min(4, int(std::lround(t->aux[0].load(std::memory_order_relaxed)))));
            for (int k = 0; k < nNotes; ++k)
                notes[k] = int(std::lround(t->aux[1 + k].load(std::memory_order_relaxed)));
        }
        const nullres::Params p = nullres::computeParams(sr, s, notes, nNotes);

        const float by = fPY + fPH;
        const int n = std::min(512, int(fPW));
        float y1[512], y2[512];
        for (int i = 0; i < n; ++i)
        {
            double mn = 1000.0, mx = -1000.0;
            for (int j = 0; j <= 4; ++j)
            {
                const double d = nullres::responseDb(p, 20.0 * std::pow(1000.0, (i + j / 4.0) / double(fPW)), sr);
                mn = std::min(mn, d);
                mx = std::max(mx, d);
            }
            y1[i] = linY(float(mx), -32.0f, 32.0f);
            y2[i] = linY(float(mn), -32.0f, 32.0f);
        }
        beginPath();
        for (int i = 0; i < n; ++i)
            rect(fPX + float(i), y1[i], 1.0f, by - y1[i]);
        fillColor(col(theme::kAccent, 0.08f));
        fill();
        beginPath();
        for (int i = 0; i < n; ++i)
            rect(fPX + float(i), y1[i], 1.0f, std::max(1.0f, y2[i] - y1[i]));
        fillColor(col(theme::kAccent, 0.45f));
        fill();

        // stems while neighbouring harmonics are at least 2.5 units apart
        beginPath();
        for (int i = 0; i < p.nv * 2; ++i)
        {
            const double f0 = sr / p.loops[i].len;
            for (int h = 1; h * f0 <= 20000.0 && std::log(1.0 + 1.0 / h) * fPW / std::log(1000.0) >= 2.5; ++h)
            {
                const float x = std::floor(freqX(float(h * f0)));
                const float y = linY(float(nullres::peakDb(p, i, h)), -32.0f, 32.0f);
                rect(x, y, 1.0f, by - y);
            }
        }
        fillColor(col(theme::kAccent, 0.9f));
        fill();

        static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int v = 0; v < p.nv; ++v)
        {
            const nullres::Loop& l = p.loops[v * 2];
            const float x = freqX(float(sr / l.len));
            dashedV(x);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%s%d", kNames[l.note % 12], l.note / 12 - 1);
            label(x + 3.0f, fPY + 6.0f + float(v % 2) * 11.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf, true);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 3)
            std::snprintf(buf, n, "%.2f s", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIResonator();
}

END_NAMESPACE_DISTRHO
