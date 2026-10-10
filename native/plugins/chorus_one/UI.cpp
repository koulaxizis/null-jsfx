/*
 * NULL JSFX - Chorus One (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ChorusOneDSP.hpp"

START_NAMESPACE_DISTRHO

class UIChorusOne : public NullOneSliderUI
{
public:
    UIChorusOne()
        : NullOneSliderUI({ "CHORUS ONE", "AMOUNT", "SUBTLE", "LUSH", 0.0f, 100.0f, 50.0f, false, false }) {}

protected:
    // left (bright) and right (dim) delay times over 2 s of the triangle LFO
    // (as in chorus_one_gfx.jsfx)
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        grid(8, 4, false);
        for (int k = 0; k < 3; ++k)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", 20 - k * 10);
            label(fPX - 6.0f, fPY + fPH * k * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        static const char* const xs[3] = { "0.5 s", "1 s", "1.5 s" };
        for (int k = 1; k <= 3; ++k)
            label(linX(k / 4.0f, 0.0f, 1.0f), fPY + fPH + 11.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, xs[k - 1]);

        const nullchorusone::Params p = nullchorusone::paramsForAmount(value());
        const int n = int(fPW) + 1;
        scissor(fPX, fPY, fPW, fPH);
        for (int sgn = -1; sgn <= 1; sgn += 2)
        {
            beginPath();
            for (int i = 0; i < n; ++i)
            {
                const float y = linY(float(nullchorusone::viewDelayMs(p, 2.0 * i / fPW, sgn)), 0.0f, 20.0f);
                if (i == 0)
                    moveTo(fPX, y);
                else
                    lineTo(fPX + float(i), y);
            }
            strokeColor(col(theme::kAccent, sgn > 0 ? 1.0f : 0.45f));
            strokeWidth(2.0f);
            lineJoin(ROUND);
            stroke();
        }
        resetScissor();
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        const nullchorusone::Params p = nullchorusone::paramsForAmount(v);
        std::snprintf(buf, n, "RATE %.2f Hz  DEPTH %.1f ms", p.lfoRate, p.lfoDepth);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIChorusOne)
};

UI* createUI()
{
    return new UIChorusOne();
}

END_NAMESPACE_DISTRHO
