/*
 * NULL JSFX - Harmonizer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "HarmonizerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "VOICE 1", 0, -1, true },
    { 1, NullKnobsUI::kKnob, "VOICE 2", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "LEVEL 1", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "LEVEL 2", 0, -1, false },
    { 8, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 9, NullKnobsUI::kSelect, "KEY", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DETUNE", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "DELAY", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 10, NullKnobsUI::kSelect, "SCALE", 1, -1, false },
    { 11, NullKnobsUI::kSelect, "FORMANT", 1, -1, false },
    { 12, NullKnobsUI::kSelect, "VOICES", 2, -1, false },
    { 13, NullKnobsUI::kKnob, "VOICE 3", 2, -1, true },
    { 14, NullKnobsUI::kKnob, "VOICE 4", 2, -1, true },
    { 15, NullKnobsUI::kKnob, "LEVEL 3", 2, -1, false },
    { 16, NullKnobsUI::kKnob, "LEVEL 4", 2, -1, false },
};

class UIHarmonizer : public NullKnobsUI
{
public:
    UIHarmonizer()
        : NullKnobsUI({ "HARMONIZER", "ADVANCED", 480, 120, nullptr, 3, nullharmonizer::kSliders, nullharmonizer::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    void dot(float x, float y, float r, uint32_t rgb, float alpha)
    {
        beginPath();
        circle(x, y, r);
        fillColor(col(rgb, alpha));
        fill();
    }

    // a voice's number, faded with its level (label() at an alpha)
    void labelA(float x, float y, const char* str, float alpha)
    {
        fontSize(8.0f * kNotoLineToEm);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(col(theme::kText, alpha));
        text(x, y, str, nullptr);
    }

    // name of MIDI note k with its octave (60 = C4)
    static void noteName(int k, char* buf, size_t n)
    {
        const int p = k - 12 * int(std::floor(k / 12.0));
        std::snprintf(buf, n, "%s%d", nullharmonizer::kKeyOptions[p], int(std::floor(k / 12.0)) - 1);
    }

    // as in harmonizer_gfx.jsfx: the voices' pitch (-12 to +12 semitones, detune included)
    // against stereo position; dot size follows the level, brightness the dry/wet Mix. Modes
    // Scale and MIDI show the intervals they sing now, the key or the held notes, and the note
    // sung, and Formant Preserve
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(4, 4, true);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "+12");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-12");
        label(fPX, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "L");
        label(fPX + fPW * 0.5f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "C");
        label(fPX + fPW, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "R");

        const float sp = value(6) / 100.0f, det = value(4) / 100.0f, mix = value(7) / 100.0f;
        const float cx = fPX + fPW * 0.5f, cy = linY(0.0f, -12.0f, 12.0f);
        const float x1 = linX((1.0f - sp) * 0.5f, 0.0f, 1.0f), x2 = linX(1.0f - (1.0f - sp) * 0.5f, 0.0f, 1.0f);
        float i1 = value(0) + det, i2 = value(1) - det, a1 = 1.0f, a2 = 1.0f, cur = 0.0f, n1 = 0.0f, n2 = 0.0f;
        float i3 = value(13) + det * 0.5f, i4 = value(14) - det * 0.5f, a3 = 1.0f, a4 = 1.0f, n3 = 0.0f, n4 = 0.0f;
        const int nv = int(std::lround(value(12))) + 2;
        bool pitched = false;
        nulljsfx::Taps* t = taps();
        if (t && t->aux[8].load(std::memory_order_relaxed) > 0.5f)
        {
            i1 = t->aux[0].load(std::memory_order_relaxed);
            i2 = t->aux[1].load(std::memory_order_relaxed);
            a1 = t->aux[2].load(std::memory_order_relaxed);
            a2 = t->aux[3].load(std::memory_order_relaxed);
            i3 = t->aux[9].load(std::memory_order_relaxed);
            i4 = t->aux[10].load(std::memory_order_relaxed);
            a3 = t->aux[11].load(std::memory_order_relaxed);
            a4 = t->aux[12].load(std::memory_order_relaxed);
        }
        if (t)
        {
            cur = t->aux[4].load(std::memory_order_relaxed);
            pitched = t->aux[5].load(std::memory_order_relaxed) > 0.5f;
            n1 = t->aux[6].load(std::memory_order_relaxed);
            n2 = t->aux[7].load(std::memory_order_relaxed);
            n3 = t->aux[13].load(std::memory_order_relaxed);
            n4 = t->aux[14].load(std::memory_order_relaxed);
        }
        const float y1 = linY(clampf(i1, -12.0f, 12.0f), -12.0f, 12.0f), y2 = linY(clampf(i2, -12.0f, 12.0f), -12.0f, 12.0f);
        const float r1 = 3.0f + 5.0f * (value(2) + 48.0f) / 48.0f, r2 = 3.0f + 5.0f * (value(3) + 48.0f) / 48.0f;
        // voices 3 and 4 (Voices 3 or 4) sit halfway out
        const float x3 = linX((1.0f - sp * 0.5f) * 0.5f, 0.0f, 1.0f), x4 = linX(1.0f - (1.0f - sp * 0.5f) * 0.5f, 0.0f, 1.0f);
        const float y3 = linY(clampf(i3, -12.0f, 12.0f), -12.0f, 12.0f), y4 = linY(clampf(i4, -12.0f, 12.0f), -12.0f, 12.0f);
        const float r3 = 3.0f + 5.0f * (value(15) + 48.0f) / 48.0f, r4 = 3.0f + 5.0f * (value(16) + 48.0f) / 48.0f;

        // the mode, bottom left under the dots: the key and the note sung (Scale), the held
        // notes (MIDI); Formant Preserve
        static const char* const kScaleNames[] = { "MAJOR", "MINOR", "DORIAN", "MIXOLYDIAN", "HARMONIC MINOR",
                                                   "PENTATONIC MAJOR", "PENTATONIC MINOR" };
        const int mode = int(std::lround(value(8)));
        char buf[64], a[16], b[16];
        if (mode == 1)
        {
            const int key = std::max(0, std::min(11, int(std::lround(value(9)))));
            const int sc = std::max(0, std::min(6, int(std::lround(value(10)))));
            if (pitched && cur > 0.5f)
            {
                noteName(int(std::lround(cur)) - 1, a, sizeof(a));
                std::snprintf(buf, sizeof(buf), "%s %s  SUNG %s", nullharmonizer::kKeyOptions[key], kScaleNames[sc], a);
            }
            else
                std::snprintf(buf, sizeof(buf), "%s %s  SUNG --", nullharmonizer::kKeyOptions[key], kScaleNames[sc]);
        }
        else if (mode == 2)
        {
            if (n1 > 0.5f)
            {
                noteName(int(std::lround(n1)) - 1, a, sizeof(a));
                if (n2 > 0.5f)
                {
                    noteName(int(std::lround(n2)) - 1, b, sizeof(b));
                    std::snprintf(buf, sizeof(buf), "MIDI  %s  %s", a, b);
                }
                else
                    std::snprintf(buf, sizeof(buf), "MIDI  %s", a);
                const float nn[2] = { n3, n4 };
                for (int v = 0; v < 2 && v + 3 <= nv; ++v)
                {
                    if (nn[v] > 0.5f)
                    {
                        char m[64];
                        noteName(int(std::lround(nn[v])) - 1, b, sizeof(b));
                        std::snprintf(m, sizeof(m), "%s  %s", buf, b);
                        std::snprintf(buf, sizeof(buf), "%s", m);
                    }
                }
            }
            else
                std::snprintf(buf, sizeof(buf), "MIDI  HOLD NOTES");
        }
        else
            buf[0] = '\0';
        if (value(11) >= 0.5f)
        {
            char m[64];
            std::snprintf(m, sizeof(m), mode > 0 ? "%s  FORMANT" : "%sFORMANT", buf);
            std::snprintf(buf, sizeof(buf), "%s", m);
        }
        label(fPX + 6.0f, fPY + fPH - 8.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kText, buf);

        beginPath();
        moveTo(cx, cy);
        lineTo(x1, y1);
        moveTo(cx, cy);
        lineTo(x2, y2);
        if (nv > 2)
        {
            moveTo(cx, cy);
            lineTo(x3, y3);
        }
        if (nv > 3)
        {
            moveTo(cx, cy);
            lineTo(x4, y4);
        }
        strokeColor(col(theme::kAccent, 0.35f));
        strokeWidth(1.0f);
        stroke();
        dot(cx, cy, 4.0f, theme::kMuted, 0.35f + 0.65f * std::cos(mix * float(M_PI) / 2.0f));
        const float wet = 0.35f + 0.65f * std::sin(mix * float(M_PI) / 2.0f);
        dot(x1, y1, r1, theme::kAccent, wet * a1);
        dot(x2, y2, r2, theme::kAccent, wet * a2);
        labelA(x1 + r1 + 4.0f, y1, "1", a1);
        labelA(x2 + r2 + 4.0f, y2, "2", a2);
        if (nv > 2)
        {
            dot(x3, y3, r3, theme::kAccent, wet * a3);
            labelA(x3 + r3 + 4.0f, y3, "3", a3);
        }
        if (nv > 3)
        {
            dot(x4, y4, r4, theme::kAccent, wet * a4);
            labelA(x4 + r4 + 4.0f, y4, "4", a4);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param <= 1 || param == 13 || param == 14)
        {
            const int mode = int(std::lround(value(8)));
            if (mode == 2)
                std::snprintf(buf, n, "MIDI");
            else
                std::snprintf(buf, n, mode == 1 ? "%+d deg" : "%+d st", int(std::lround(v)));
        }
        else if (param <= 3 || param == 15 || param == 16)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 4)
            std::snprintf(buf, n, "%.1f ct", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIHarmonizer();
}

END_NAMESPACE_DISTRHO
