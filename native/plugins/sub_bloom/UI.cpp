/*
 * NULL JSFX - Sub Bloom (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SubBloomDSP.hpp"
#include "SubBloomSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SOURCE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SUB", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "OCTAVE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "ORIGINAL", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "OUTPUT", 0, -1, true },
};

class UISubBloom : public NullKnobsUI
{
public:
    UISubBloom()
        : NullKnobsUI({ "SUB BLOOM", "ADVANCED", 480, 120, nullptr, 1, nullsubbloom::kSliders, nullsubbloom::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // the generated sub at phase t (in periods of the source note), as sb_sub in the GFX
    float sub(float t) const
    {
        const float s1 = std::sin(float(M_PI) * t), s2 = std::sin(float(M_PI) * t * 0.5f);
        const int m = int(std::lround(value(2)));
        return m == 0 ? s1 : m == 1 ? s2 : (s1 + s2) * 0.7071f;
    }

    // as in sub_bloom_gfx.jsfx: four periods of a note at Source (thin, at Original Mix) and
    // the sub under it (at Sub Level), with Output
    void drawDisplay() override
    {
        setPlot(fVX + 4.0f, fVY + 16.0f, fVW - 8.0f, fVH - 20.0f);
        grid(8, 2, true);
        const float src = value(0);
        const int m = int(std::lround(value(2)));
        char buf[48];
        std::snprintf(buf, sizeof(buf), "SOURCE %d Hz", int(std::lround(src)));
        label(fPX, fPY - 8.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        if (m == 0)
            std::snprintf(buf, sizeof(buf), "SUB %.1f Hz", src / 2.0);
        else if (m == 1)
            std::snprintf(buf, sizeof(buf), "SUB %.1f Hz", src / 4.0);
        else
            std::snprintf(buf, sizeof(buf), "SUB %.1f + %.1f Hz", src / 2.0, src / 4.0);
        label(fPX + fPW, fPY - 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kAccent, buf);

        const float g = std::pow(10.0f, value(4) / 20.0f) * 0.8f;
        const float orig = value(3) / 100.0f, lvl = value(1) / 100.0f;
        constexpr int n = 425;
        float xs[n], ys[n];
        beginPath();
        for (int i = 0; i < n; ++i)
        {
            const float t = float(i) / float(n - 1) * 4.0f;
            const float x = fPX + fPW * float(i) / float(n - 1);
            const float y = linY(std::sin(2.0f * float(M_PI) * t) * orig * g, -1.0f, 1.0f);
            if (i == 0)
                moveTo(x, y);
            else
                lineTo(x, y);
            xs[i] = x;
            ys[i] = linY(sub(t) * lvl * g, -1.0f, 1.0f);
        }
        strokeColor(col(theme::kMuted, 0.8f));
        strokeWidth(1.0f);
        stroke();
        curve(xs, ys, n);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4)
            std::snprintf(buf, n, "%+d dB", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISubBloom();
}

END_NAMESPACE_DISTRHO
