/*
 * NULL JSFX - Surround Panner (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SurroundPannerPlugin.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "FORMAT", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "INPUT", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "AZIMUTH", 0, -1, true },
    { 3, NullKnobsUI::kKnob, "ELEVATION", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SPREAD", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "CENTER", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "LFE", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

// each pin's speaker: azimuth in 5.1 and in 7.1 (the sides move), on the top ring or not
static const float kAz51[12] = { -30, 30, 0, 0, -110, 110, -150, 150, -45, 45, -135, 135 };
static const float kAz71[12] = { -30, 30, 0, 0, -90, 90, -150, 150, -45, 45, -135, 135 };
static const char* const kMeterNames[2][12] = {
    { "L", "R", "C", "LFE", "LS", "RS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" },
    { "L", "R", "C", "LFE", "LSS", "RSS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" },
};

class UISurroundPanner : public NullKnobsUI
{
public:
    UISurroundPanner()
        : NullKnobsUI({ "SURROUND PANNER", "ADVANCED", 480, 200, nullptr, 2, nullpan::kSliders, nullpan::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    SurroundPannerBase* plugin() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        return static_cast<SurroundPannerBase*>(static_cast<Plugin*>(getPluginInstancePointer()));
#else
        return nullptr;
#endif
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float a = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, a));
        fill();
    }

    // as in surround_panner_gfx.jsfx: left, the room from above (front up, the tops at half the
    // radius) with each speaker of the Format lit by its gain and the source's points; right, a
    // meter for each output pin
    void drawDisplay() override
    {
        SurroundPannerBase* p = plugin();
        const int fmt = int(std::lround(value(0)));
        const bool stereo = value(1) >= 0.5f;
        const float az = value(2), elev = value(3), spread = value(4);
        const float cx = 128.0f, cy = 152.0f, r = 78.0f;
        box(24.0f, 62.0f, 208.0f, 180.0f, theme::kBackground);
        beginPath();
        circle(cx, cy, r);
        if (fmt == 2)
            circle(cx, cy, r * 0.5f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        for (int k = 0; k < 12; ++k)
        {
            const bool on = k != 3 && (k < 6 || (k < 8 && fmt >= 1) || fmt == 2);
            if (!on)
                continue;
            const float a = (fmt ? kAz71[k] : kAz51[k]) * float(M_PI) / 180.0f, rr = k >= 8 ? r * 0.5f : r;
            const float x = cx + rr * std::sin(a), y = cy - rr * std::cos(a);
            const float g = p ? p->power[k].load(std::memory_order_relaxed) : 0.0f;
            box(x - 5.0f, y - 5.0f, 10.0f, 10.0f, theme::kCard);
            box(x - 5.0f, y - 5.0f, 10.0f, 10.0f, theme::kAccent, std::min(1.0f, g));
            beginPath();
            rect(x - 5.0f, y - 5.0f, 10.0f, 10.0f);
            strokeColor(col(theme::kBorder));
            stroke();
        }
        const float e = fmt == 2 ? std::cos(elev * float(M_PI) / 180.0f) : 1.0f;
        const int n = stereo ? 2 : spread > 0.0f ? 7 : 1;
        for (int i = 0; i < n; ++i)
        {
            const float a = (stereo ? az + spread * (float(i) - 0.5f) : n > 1 ? az + spread * (float(i) / 6.0f - 0.5f) : az)
                            * float(M_PI) / 180.0f;
            beginPath();
            circle(cx + r * e * 0.85f * std::sin(a), cy - r * e * 0.85f * std::cos(a), n == 7 && i != 3 ? 1.5f : 3.0f);
            fillColor(col(theme::kText));
            fill();
        }
        label(30.0f, 66.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, "FRONT UP", true);
        label(30.0f, 230.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, fmt == 0 ? "5.1" : fmt == 1 ? "7.1" : "7.1.4", true);

        box(244.0f, 62.0f, 212.0f, 180.0f, theme::kBackground);
        for (int k = 0; k < 12; ++k)
        {
            const bool on = k < 6 || (k < 8 && fmt >= 1) || fmt == 2;
            const float x = 250.0f + float(k) * 17.2f;
            if (p)
                fMeter[k] = std::max(fMeter[k], p->peak[k].exchange(0.0f, std::memory_order_relaxed));
            const float f = dbNorm(fMeter[k] > 1e-7f ? 20.0f * std::log10(fMeter[k]) : -140.0f, -60.0f);
            box(x, 68.0f, 11.0f, 150.0f, theme::kCard);
            if (on)
                box(x, 68.0f + 150.0f * (1.0f - f), 11.0f, 150.0f * f, fMeter[k] > 1.0f ? theme::kText : theme::kAccent);
            label(x + 5.5f, 226.0f, ALIGN_CENTER, 6.0f, 0.5f, on ? theme::kMuted : theme::kBorder, kMeterNames[fmt ? 1 : 0][k], true);
            fMeter[k] *= 0.85f;
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 6 && v <= -60.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 6)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (param == 7)
            std::snprintf(buf, n, "%+.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    float fMeter[12] {};
};

UI* createUI()
{
    return new UISurroundPanner();
}

END_NAMESPACE_DISTRHO
