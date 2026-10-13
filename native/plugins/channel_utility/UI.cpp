/*
 * NULL JSFX - Channel Utility (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ChannelUtilitySliders.hpp"

START_NAMESPACE_DISTRHO

// two tabs of two rows, as in channel_utility_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "PAD", 0, 0, false },
    { 1, NullKnobsUI::kKnob, "GAIN", 0, 0, false },
    { 2, NullKnobsUI::kSelect, "INVERT L", 0, 0, false },
    { 3, NullKnobsUI::kSelect, "INVERT R", 0, 0, false },
    { 4, NullKnobsUI::kKnob, "DC FREQ", 1, 0, false },
    { 5, NullKnobsUI::kKnob, "DC MIX", 1, 0, false },
    { 6, NullKnobsUI::kKnob, "BAL L", 0, 1, true },
    { 7, NullKnobsUI::kKnob, "BAL R", 0, 1, true },
    { 8, NullKnobsUI::kSelect, "SOLO L", 0, 1, false },
    { 9, NullKnobsUI::kSelect, "SOLO R", 0, 1, false },
    { 10, NullKnobsUI::kSelect, "MONO", 1, 1, false },
    { 11, NullKnobsUI::kKnob, "OUTPUT", 1, 1, true },
    { 12, NullKnobsUI::kSelect, "LIMIT", 1, 1, false },
};

class UIChannelUtility : public NullKnobsUI
{
public:
    UIChannelUtility()
        : NullKnobsUI({ "CHANNEL UTILITY", "ESSENTIALS", 480, 120, "INPUT|OUTPUT", 2, nullchannelutility::kSliders,
                        nullchannelutility::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    bool on(uint32_t p) const { return value(p) >= 0.5f; }

    // one stage of the signal chain (cu_stage in the GFX): box i of 9, lit when it changes the signal
    void stage(int i, const char* name, const char* text, bool lit)
    {
        const float w = (432.0f - 8.0f * 6.0f) / 9.0f, x = 24.0f + float(i) * (w + 6.0f);
        beginPath();
        roundedRect(x + 0.5f, 66.5f, w - 1.0f, 31.0f, 4.0f);
        fillColor(col(lit ? 0x2d2b28 : theme::kBackground));
        fill();
        strokeColor(col(lit ? theme::kTrackHover : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x + w * 0.5f, 76.0f, ALIGN_CENTER, 6.5f, 1.0f, theme::kMuted, name, true);
        label(x + w * 0.5f, 89.0f, ALIGN_CENTER, 7.5f, 0.0f, lit ? theme::kText : theme::kMuted, text);
        if (i < 8)
            drawArrow(x + w + 3.0f, 82.0f, 1.0f);
    }

    // as in channel_utility_gfx.jsfx: the signal chain in processing order, the stages that
    // change the signal lit, then input and output level meters per channel
    void drawDisplay() override
    {
        using nullchannelutility::tenth;
        char b[32];
        const int pad = int(std::lround(value(0)));
        std::snprintf(b, sizeof(b), "%d dB", pad == 1 ? -6 : (pad == 2 ? -12 : 0));
        stage(0, "PAD", b, pad > 0);
        const double g = tenth(value(1));
        std::snprintf(b, sizeof(b), "%+.1f dB", g);
        stage(1, "GAIN", b, g != 0.0);
        stage(2, "PHASE", on(2) && on(3) ? "L+R" : (on(2) ? "L" : (on(3) ? "R" : "OFF")), on(2) || on(3));
        const long mix = std::lround(value(5));
        if (mix > 0)
            std::snprintf(b, sizeof(b), "%d Hz", int(std::lround(value(4))));
        else
            std::snprintf(b, sizeof(b), "OFF");
        stage(3, "DC", b, mix > 0);
        const double bl = tenth(value(6)), br = tenth(value(7));
        std::snprintf(b, sizeof(b), "%.1f/%.1f", bl, br);
        stage(4, "BAL", b, bl != 0.0 || br != 0.0);
        stage(5, "SOLO", on(8) ? "L" : (on(9) ? "R" : "OFF"), on(8) || on(9));
        stage(6, "MONO", on(10) ? "ON" : "OFF", on(10));
        const double o = tenth(value(11));
        std::snprintf(b, sizeof(b), "%+.1f dB", o);
        stage(7, "OUT", b, o != 0.0);
        stage(8, "LIMIT", on(12) ? "ON" : "OFF", on(12));

        float lv[4] = {};
        if (nulljsfx::Taps* t = taps())
            for (int k = 0; k < 4; ++k)
                lv[k] = t->scopeL[k];
        meter(24.0f, 112.0f, 208.0f, 8.0f, "IN L", nulljsfx::toDb(lv[0]), -140.0f, -60.0f);
        meter(24.0f, 128.0f, 208.0f, 8.0f, "IN R", nulljsfx::toDb(lv[1]), -140.0f, -60.0f);
        meter(248.0f, 112.0f, 208.0f, 8.0f, "OUT L", nulljsfx::toDb(lv[2]), -140.0f, -60.0f);
        meter(248.0f, 128.0f, 208.0f, 8.0f, "OUT R", nulljsfx::toDb(lv[3]), -140.0f, -60.0f);
        for (int c = 0; c < 2; ++c)
        {
            const float x0 = 24.0f + 224.0f * float(c);
            label(x0 + 30.0f, 149.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "-60");
            label(x0 + 119.0f, 149.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "-30");
            label(x0 + 208.0f, 149.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0 dB");
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1 || param == 6 || param == 7 || param == 11)
            std::snprintf(buf, n, "%+.1f dB", nullchannelutility::tenth(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIChannelUtility();
}

END_NAMESPACE_DISTRHO
