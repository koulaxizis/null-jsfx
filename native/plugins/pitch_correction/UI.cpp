/*
 * NULL JSFX - Pitch Correction (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PitchCorrectionSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "KEY", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SCALE", 0, -1, false },
    { 7, NullKnobsUI::kSelect, "TARGET", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RETUNE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "HUMANIZE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "TRANSPOSE", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "SENSITIVITY", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "VIB RATE", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "VIB DEPTH", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIPitchCorrection : public NullKnobsUI
{
public:
    UIPitchCorrection()
        : NullKnobsUI({ "PITCH CORRECTION", "ADVANCED", 480, 150, nullptr, 2, nullpitchcorrection::kSliders,
                        nullpitchcorrection::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kLen = 300; // history entries, one per design pixel (10 ms each)

    // name of MIDI note k with its octave (60 = C4)
    static void noteName(int k, char* buf, size_t n)
    {
        static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int p = k - 12 * int(std::floor(k / 12.0));
        std::snprintf(buf, n, "%s%d", kNames[p], int(std::floor(k / 12.0)) - 1);
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    // as in pitch_correction_gfx.jsfx: left, the last 3 s of detected (grey) and corrected
    // (silver) pitch on a 15-semitone note grid around the average corrected pitch, the scale's
    // notes shaded (the key brighter, the target note lit) and named; right, the target note,
    // the cents deviation of the detected pitch from it, the detected frequency and the shift
    void drawDisplay() override
    {
        setPlot(52.0f, 66.0f, 300.0f, 126.0f);

        nulljsfx::Taps* t = taps();
        const uint32_t len = nulljsfx::Taps::kScopeLen;
        const uint32_t pos = t ? t->scopePos.load(std::memory_order_acquire) : 0;
        float det[kLen], cor[kLen];
        double sum = 0.0;
        int cnt = 0;
        for (int i = 0; i < kLen; ++i)
        {
            det[i] = cor[i] = 0.0f;
            if (t)
            {
                const uint32_t k = (pos + len - uint32_t(kLen) + uint32_t(i)) % len;
                det[i] = t->scopeL[k];
                cor[i] = t->scopeR[k];
            }
            if (cor[i] > 0.0f)
            {
                sum += cor[i];
                ++cnt;
            }
        }
        bool voiced = false;
        float detNow = 0.0f, tgt = 60.0f, shift = 0.0f;
        bool midiNote = false;
        if (t)
        {
            voiced = t->extra.load(std::memory_order_relaxed) > 0.5f;
            detNow = t->inLevel.load(std::memory_order_relaxed);
            tgt = t->outLevel.load(std::memory_order_relaxed);
            shift = t->gr.load(std::memory_order_relaxed);
            midiNote = t->aux[0].load(std::memory_order_relaxed) > 0.5f;
        }

        const int c = cnt > 0 ? int(std::floor(sum / cnt + 0.5)) : 60;
        const float lo = float(c) - 7.5f, hi = float(c) + 7.5f;
        const int key = int(std::lround(value(0)));
        const int mask = nullpitchcorrection::kScaleMasks[std::max(0, std::min(8, int(std::lround(value(1)))))];
        char buf[32];
        for (int k = c - 7; k <= c + 7; ++k)
        {
            const int p = (k - key) - 12 * int(std::floor((k - key) / 12.0));
            const bool ins = ((mask >> p) & 1) != 0;
            const float y0 = linY(float(k) + 0.5f, lo, hi), y1 = linY(float(k) - 0.5f, lo, hi);
            if (ins)
                box(fPX, y0, fPW, y1 - y0, theme::kAccent, voiced && k == int(std::lround(tgt)) ? 0.16f : p == 0 ? 0.09f : 0.045f);
            hline(std::floor(y1) + 0.5f);
            // names: the scale's notes, or the natural notes when Chromatic
            const int pn = k - 12 * int(std::floor(k / 12.0));
            if (mask == 4095 ? ((2741 >> pn) & 1) != 0 : ins)
            {
                noteName(k, buf, sizeof(buf));
                label(fPX - 6.0f, (y0 + y1) * 0.5f, ALIGN_RIGHT, 7.0f, 0.0f, theme::kMuted, buf);
            }
        }
        hline(std::floor(fPY) + 0.5f);

        // detected pitch, thin
        beginPath();
        for (int i = 1; i < kLen; ++i)
            if (det[i] > 0.0f && det[i - 1] > 0.0f && std::fabs(det[i] - det[i - 1]) < 2.0f)
            {
                moveTo(fPX + float(i) - 0.5f, linY(det[i - 1], lo, hi));
                lineTo(fPX + float(i) + 0.5f, linY(det[i], lo, hi));
            }
        strokeColor(col(theme::kMuted, 0.9f));
        strokeWidth(1.0f);
        stroke();
        // corrected pitch, thick
        beginPath();
        for (int i = 1; i < kLen; ++i)
            if (cor[i] > 0.0f && cor[i - 1] > 0.0f && std::fabs(cor[i] - cor[i - 1]) < 2.0f)
            {
                moveTo(fPX + float(i) - 0.5f, linY(cor[i - 1], lo, hi));
                lineTo(fPX + float(i) + 0.5f, linY(cor[i], lo, hi));
            }
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);

        label(408.0f, 74.0f, ALIGN_CENTER, 7.0f, 1.0f, theme::kMuted, midiNote ? "MIDI NOTE" : "NOTE", true);
        if (voiced)
            noteName(int(std::lround(tgt)), buf, sizeof(buf));
        label(408.0f, 98.0f, ALIGN_CENTER, 22.0f, 0.0f, voiced ? theme::kText : theme::kMuted, voiced ? buf : "--", true);
        // cents bar, -50 to +50
        box(368.0f, 120.0f, 80.0f, 6.0f, theme::kBorder, 1.0f);
        if (voiced)
        {
            const float ct = clampf((detNow - tgt) * 100.0f, -50.0f, 50.0f);
            if (ct > 0.0f)
                box(408.0f, 120.0f, ct * 0.8f, 6.0f, theme::kAccent, 0.5f);
            else
                box(408.0f + ct * 0.8f, 120.0f, -ct * 0.8f, 6.0f, theme::kAccent, 0.5f);
            box(407.0f + ct * 0.8f, 117.0f, 2.0f, 12.0f, theme::kText, 1.0f);
        }
        box(407.5f, 117.0f, 1.0f, 12.0f, theme::kMuted, 1.0f);
        if (voiced)
            std::snprintf(buf, sizeof(buf), "%+d ct", int(std::floor((detNow - tgt) * 100.0f + 0.5f)));
        else
            std::snprintf(buf, sizeof(buf), "-- ct");
        label(408.0f, 140.0f, ALIGN_CENTER, 8.5f, 0.0f, theme::kText, buf);
        if (voiced)
            std::snprintf(buf, sizeof(buf), "%.1f Hz", 440.0 * std::pow(2.0, (detNow - 69.0) / 12.0));
        else
            std::snprintf(buf, sizeof(buf), "-- Hz");
        label(408.0f, 160.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, buf);
        std::snprintf(buf, sizeof(buf), "SHIFT %+.2f st", shift);
        label(408.0f, 178.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, buf);
    }

    void hline(float y)
    {
        beginPath();
        moveTo(fPX, y);
        lineTo(fPX + fPW, y);
        strokeColor(col(theme::kBorder, 0.6f));
        strokeWidth(1.0f);
        stroke();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2 && v < 0.5f)
            std::snprintf(buf, n, "HARD");
        else if (param == 4)
            std::snprintf(buf, n, "%+d st", int(std::lround(v)));
        else if (param == 8)
            std::snprintf(buf, n, "%.1f Hz", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIPitchCorrection();
}

END_NAMESPACE_DISTRHO
