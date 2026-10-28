/*
 * NULL JSFX - Downmix Check (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DownmixCheckPlugin.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "BED", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "MONITOR", 0, -1, false },
    { 6, NullKnobsUI::kSelect, "SOLO", 0, -1, false },
    { 7, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "CENTER MIX", 1, -1, false },
    { 3, NullKnobsUI::kKnob, "SURROUND MIX", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "HEIGHT MIX", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "LFE MIX", 1, -1, false },
};

class UIDownmixCheck : public NullKnobsUI
{
public:
    UIDownmixCheck()
        : NullKnobsUI({ "DOWNMIX CHECK", "ADVANCED", 480, 200, nullptr, 2, nulldmx::kSliders, nulldmx::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    DownmixCheckBase* plugin() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        return static_cast<DownmixCheckBase*>(static_cast<Plugin*>(getPluginInstancePointer()));
#else
        return nullptr;
#endif
    }

    void box(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb));
        fill();
    }

    // a meter row: the pin's name, then its peak on a -60..0 dB bar (silver, white over 0 dB)
    void row(float x, float y, float w, const char* name, float pk, bool on)
    {
        label(x, y + 4.5f, ALIGN_LEFT, 6.0f, 0.5f, on ? theme::kMuted : theme::kBorder, name, true);
        box(x + 26.0f, y, w - 26.0f, 9.0f, theme::kCard);
        if (on)
        {
            const float f = dbNorm(pk > 1e-7f ? 20.0f * std::log10(pk) : -140.0f, -60.0f);
            box(x + 26.0f, y, (w - 26.0f) * f, 9.0f, pk > 1.0f ? theme::kText : theme::kAccent);
        }
    }

    void parameterChanged(uint32_t index, float v) override
    {
        fHold = 0.0f;
        NullKnobsUI::parameterChanged(index, v);
    }

    // as in downmix_check_gfx.jsfx: left, the bed coming in (after Solo); right, what the
    // Monitor plays, and the fold's highest peak since the last change
    void drawDisplay() override
    {
        static const char* const names[2][12] = {
            { "L", "R", "C", "LFE", "LS", "RS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" },
            { "L", "R", "C", "LFE", "LSS", "RSS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" },
        };
        DownmixCheckBase* p = plugin();
        const int bed = int(std::lround(value(0))), mon = int(std::lround(value(1)));
        box(24.0f, 62.0f, 208.0f, 180.0f, theme::kBackground);
        box(244.0f, 62.0f, 212.0f, 180.0f, theme::kBackground);
        label(32.0f, 70.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, bed == 0 ? "BED 5.1" : bed == 1 ? "BED 7.1" : "BED 7.1.4", true);
        label(252.0f, 70.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted,
              mon == 0 ? "MONITOR BED" : mon == 1 ? "MONITOR 5.1" : mon == 2 ? "MONITOR LO/RO" : "MONITOR MONO", true);
        for (int k = 0; k < 12; ++k)
        {
            if (p)
            {
                fIn[k] = std::max(fIn[k], p->inPeak[k].exchange(0.0f, std::memory_order_relaxed));
                fOut[k] = std::max(fOut[k], p->outPeak[k].exchange(0.0f, std::memory_order_relaxed));
            }
            bool on = k < 6 || (k < 8 && bed >= 1) || bed == 2;
            const char* n = names[bed ? 1 : 0][k];
            row(32.0f, 82.0f + float(k) * 13.0f, 192.0f, n, fIn[k], on);
            on = mon == 0 ? on : mon == 1 ? k < 6 : k < 2;
            if (mon >= 2 && k < 2)
                n = mon == 3 ? "M" : k ? "RO" : "LO";
            row(252.0f, 82.0f + float(k) * 13.0f, 196.0f, n, fOut[k], on);
            fIn[k] *= 0.85f;
            fOut[k] *= 0.85f;
        }
        if (p)
            fHold = std::max(fHold, p->foldPeak.exchange(0.0f, std::memory_order_relaxed));
        char buf[48];
        if (fHold > 1e-7f)
            std::snprintf(buf, sizeof(buf), "FOLD PEAK %+.1f%s", 20.0 * std::log10(double(fHold)), fHold > 1.0f ? " CLIPS" : "");
        else
            std::snprintf(buf, sizeof(buf), "FOLD PEAK -INF");
        label(448.0f, 70.0f, ALIGN_RIGHT, 7.0f, 1.0f, fHold > 1.0f ? theme::kText : theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 5 && v <= -60.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 5 || param == 7)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param >= 2 && param <= 4)
            std::snprintf(buf, n, "%.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    float fIn[12] {}, fOut[12] {}, fHold = 0.0f;
};

UI* createUI()
{
    return new UIDownmixCheck();
}

END_NAMESPACE_DISTRHO
