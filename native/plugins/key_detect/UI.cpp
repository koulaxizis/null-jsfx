/*
 * NULL JSFX - Key Detect (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "KeyDetectDSP.hpp"
#include "KeyDetectSliders.hpp"

START_NAMESPACE_DISTRHO

// one row, as in key_detect_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "WINDOW", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "PROFILE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "HOLD", 0, -1, false },
};

class UIKeyDetect : public NullKnobsUI
{
public:
    UIKeyDetect()
        : NullKnobsUI({ "KEY DETECT", "ADVANCED", 480, 200, nullptr, 1, nullkeydetect::kUiSliders,
                        nullkeydetect::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in key_detect_gfx.jsfx: the key, its confidence, the runner-up, the relative key and the
    // settings for Scale and Pitch Correction; the chroma as 12 bars, the key's scale lit
    void drawDisplay() override
    {
        float c[12] = {};
        int key = -1, key2 = -1;
        float conf = 0.0f, conf2 = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            for (int i = 0; i < 12; ++i)
                c[i] = t->aux[i].load(std::memory_order_relaxed);
            key = int(t->aux[12].load(std::memory_order_relaxed));
            conf = t->aux[13].load(std::memory_order_relaxed);
            key2 = int(t->aux[14].load(std::memory_order_relaxed));
            conf2 = t->aux[15].load(std::memory_order_relaxed);
        }
        const bool hold = nullkeydetect::sliderValue(3, value(3)) >= 0.5;
        const float x0 = fVX + 8.0f;
        char buf[96];
        label(x0, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, hold ? "KEY  (HELD)" : "KEY", true);
        if (key >= 0)
        {
            const bool minor = key >= 12;
            std::snprintf(buf, sizeof(buf), "%s %s", nullkeydetect::keyNote(key), minor ? "MINOR" : "MAJOR");
            label(x0, fVY + 38.0f, ALIGN_LEFT, 22.0f, 1.0f, theme::kText, buf, true);
            std::snprintf(buf, sizeof(buf), "CONFIDENCE %d%%", int(std::lround(std::fmax(0.0f, conf) * 100.0f)));
            label(x0, fVY + 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
            if (key2 >= 0)
            {
                std::snprintf(buf, sizeof(buf), "OR %s %s  %d%%", nullkeydetect::keyNote(key2), key2 >= 12 ? "MINOR" : "MAJOR",
                              int(std::lround(std::fmax(0.0f, conf2) * 100.0f)));
                label(x0, fVY + 82.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
            }
            const int rel = minor ? (key + 3) % 12 : (key + 9) % 12 + 12;
            std::snprintf(buf, sizeof(buf), "RELATIVE %s %s", nullkeydetect::keyNote(rel), minor ? "MAJOR" : "MINOR");
            label(x0, fVY + 98.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
            label(x0, fVY + 126.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kSlogan, "IN SCALE AND PITCH CORRECTION", true);
            std::snprintf(buf, sizeof(buf), "KEY %s   SCALE %s", nullkeydetect::keyNote(key), minor ? "MINOR" : "MAJOR");
            label(x0, fVY + 140.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kText, buf, true);
        }
        else
        {
            label(x0, fVY + 38.0f, ALIGN_LEFT, 22.0f, 1.0f, theme::kSlogan, "LISTENING", true);
            label(x0, fVY + 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kSlogan, "PLAY A FEW BARS OF MUSIC", true);
        }

        // the chroma: 12 bars from C, the notes of the key's scale lit
        static const int kMajor[] = { 0, 2, 4, 5, 7, 9, 11 }, kMinor[] = { 0, 2, 3, 5, 7, 8, 10 };
        float mx = 0.0f;
        for (int i = 0; i < 12; ++i)
            mx = std::fmax(mx, c[i]);
        const float bx = fVX + 216.0f, bw = fVX + fVW - 8.0f - bx, by = fVY + 20.0f, bh = fVH - 40.0f;
        static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
        {
            bool in = false;
            if (key >= 0)
                for (int j = 0; j < 7; ++j)
                    in = in || (key % 12 + (key >= 12 ? kMinor[j] : kMajor[j])) % 12 == i;
            const float w = bw / 12.0f, x = bx + i * w + 2.0f, v = mx > 0.0f ? c[i] / mx : 0.0f;
            beginPath();
            rect(x, by, w - 4.0f, bh);
            fillColor(col(theme::kBorder, 0.35f));
            fill();
            beginPath();
            rect(x, by + bh * (1.0f - v), w - 4.0f, bh * v);
            fillColor(in ? col(theme::kAccent, key >= 0 && i == key % 12 ? 1.0f : 0.7f) : col(theme::kMuted, 0.35f));
            fill();
            label(x + (w - 4.0f) * 0.5f, by + bh + 10.0f, ALIGN_CENTER, 7.0f, 0.0f,
                  key >= 0 && i == key % 12 ? theme::kText : theme::kMuted, kNames[i]);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullkeydetect::sliderValue(param, v);
        if (param == 0)
            std::snprintf(buf, n, "%d s", int(x));
        else if (param == 1)
            std::snprintf(buf, n, "%d dB", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIKeyDetect();
}

END_NAMESPACE_DISTRHO
