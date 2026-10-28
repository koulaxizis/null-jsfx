/*
 * NULL JSFX - Bass Management (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BassManagementPlugin.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "BED", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "CROSSOVER", 0, -1, false, true },
    { 2, NullKnobsUI::kSelect, "MAINS", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "LFE GAIN", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SUB LEVEL", 1, -1, true },
    { 5, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UIBassManagement : public NullKnobsUI
{
public:
    UIBassManagement()
        : NullKnobsUI({ "BASS MANAGEMENT", "ADVANCED", 480, 200, nullptr, 2, nullbm::kSliders, nullbm::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    BassManagementBase* plugin() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        return static_cast<BassManagementBase*>(static_cast<Plugin*>(getPluginInstancePointer()));
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

    // as in bass_management_gfx.jsfx: left, the crossover (the mains' high-pass, the sub's
    // low-pass with Sub Level, and their sum); right, a meter for each output pin
    void drawDisplay() override
    {
        static const char* const names[2][12] = {
            { "L", "R", "C", "SUB", "LS", "RS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" },
            { "L", "R", "C", "SUB", "LSS", "RSS", "LRS", "RRS", "LTF", "RTF", "LTR", "RTR" },
        };
        BassManagementBase* p = plugin();
        const int bed = int(std::lround(value(0))), listen = int(std::lround(value(5)));
        const float fc = value(1), subg = std::pow(10.0f, value(4) / 20.0f);
        const bool full = value(2) >= 0.5f;
        setPlot(50.0f, 84.0f, 180.0f, 140.0f);
        gridFreq(24.0f);
        for (int c = 0; c < 3; ++c)
        {
            beginPath();
            for (int i = 0; i <= 196; ++i)
            {
                const float f = 20.0f * std::pow(1000.0f, float(i) / 196.0f), r = std::pow(f / fc, 4.0f);
                const float lp = 1.0f / (1.0f + r) * subg, hp = full ? 1.0f : r / (1.0f + r);
                const float v = c == 0 ? hp : c == 1 ? lp : std::sqrt(hp * hp + lp * lp);
                const float y = linY(v > 1e-7f ? 20.0f * std::log10(v) : -140.0f, -16.0f, 16.0f);
                if (i == 0) moveTo(freqX(f), y); else lineTo(freqX(f), y);
            }
            strokeColor(c == 2 ? col(theme::kText, 0.6f) : col(c == 1 ? theme::kMuted : theme::kAccent));
            strokeWidth(1.0f);
            stroke();
        }
        char buf[32];
        label(50.0f, 70.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kAccent, "MAINS", true);
        label(86.0f, 70.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kMuted, "SUB", true);
        std::snprintf(buf, sizeof(buf), "%d HZ", int(std::lround(fc)));
        label(230.0f, 70.0f, ALIGN_RIGHT, 6.5f, 1.0f, theme::kMuted, buf, true);

        box(244.0f, 62.0f, 212.0f, 180.0f, theme::kBackground);
        label(252.0f, 70.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted,
              listen == 1 ? "LISTEN SUB" : listen == 2 ? "LISTEN MAINS" : "OUTPUT", true);
        for (int k = 0; k < 12; ++k)
        {
            if (p)
                fPeak[k] = std::max(fPeak[k], p->peak[k].exchange(0.0f, std::memory_order_relaxed));
            const bool on = k < 6 || (k < 8 && bed >= 1) || bed == 2;
            const float y = 82.0f + float(k) * 13.0f;
            label(252.0f, y + 4.5f, ALIGN_LEFT, 6.0f, 0.5f, on ? theme::kMuted : theme::kBorder, names[bed ? 1 : 0][k], true);
            box(278.0f, y, 170.0f, 9.0f, theme::kCard);
            if (on)
            {
                const float f = dbNorm(fPeak[k] > 1e-7f ? 20.0f * std::log10(fPeak[k]) : -140.0f, -60.0f);
                box(278.0f, y, 170.0f * f, 9.0f, fPeak[k] > 1.0f ? theme::kText : theme::kAccent);
            }
            fPeak[k] *= 0.85f;
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4 || param == 6)
            std::snprintf(buf, n, "%+.1f dB", v);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    float fPeak[12] {};
};

UI* createUI()
{
    return new UIBassManagement();
}

END_NAMESPACE_DISTRHO
