/*
 * NULL JSFX - Music Box (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As music_box_gfx.jsfx: the display holds the comb from C4 to C7 (one tine per key, shorter
 * as the pitch rises) with the tines ringing lit, the tines ringing, the spring and the
 * mechanism. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "MusicBoxSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "BRIGHT", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "BODY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "BOX SIZE", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "VELOCITY", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "MECHANISM", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "WIND DOWN", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

class UIMusicBox : public NullKnobsUI
{
public:
    UIMusicBox()
        : NullKnobsUI({ "MUSIC BOX", "INSTRUMENTS", 560, 156, nullptr, 2, nullmusicbox::kSliders, nullmusicbox::kNumParams,
                        kControls, kNumControls, true })
    {
    }

protected:
    void box(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb));
        fill();
    }

    void drawDisplay() override
    {
        bool on[128] = {};
        int tines = 0;
        float spring = 1.0f;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            tines = int(std::lround(t->extra.load(std::memory_order_relaxed)));
            spring = t->gr.load(std::memory_order_relaxed);
        }

        // the comb: its spine and one tine per key from C4 to C7
        box(24.0f, 68.0f, 512.0f, 8.0f, theme::kBorder);
        const float w = 512.0f / 37.0f;
        for (int i = 0; i < 37; ++i)
        {
            const float x = 24.0f + float(i) * w, h = 104.0f - 60.0f * float(i) / 36.0f;
            box(x + 1.5f, 76.0f, w - 3.0f, h, on[60 + i] ? theme::kAccent : 0x2d2b28);
            if (i % 12 == 0)
            {
                char b[8];
                std::snprintf(b, sizeof(b), "C%d", 4 + i / 12);
                label(x + w * 0.5f, 76.0f + h + 8.0f, ALIGN_CENTER, 6.0f, 0.0f, theme::kMuted, b, true);
            }
        }

        // the tines ringing, the spring and the mechanism
        char info[64], sp[32];
        std::snprintf(info, sizeof(info), "TINES %d / 32", tines);
        label(24.0f, 200.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        if (value(5) > 0.0f)
        {
            if (spring < 0.0f)
                std::snprintf(sp, sizeof(sp), "SPRING RUN DOWN");
            else
                std::snprintf(sp, sizeof(sp), "SPRING %d%%", int(std::floor(100.0f * spring + 0.5f)));
        }
        else
            std::snprintf(sp, sizeof(sp), "SPRING WOUND");
        std::snprintf(info, sizeof(info), "%s   MECHANISM %d%%", sp, int(value(4)));
        label(536.0f, 200.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, info, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 8)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 5 && v <= 0.0f)
            std::snprintf(buf, n, "OFF");
        else
            std::snprintf(buf, n, "%d%%", int(v));
    }
};

UI* createUI()
{
    return new UIMusicBox();
}

END_NAMESPACE_DISTRHO
