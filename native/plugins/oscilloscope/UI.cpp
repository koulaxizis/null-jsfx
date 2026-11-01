/*
 * NULL JSFX - Oscilloscope (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "OscilloscopeSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in oscilloscope_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIME", 0, -1, false, true },
    { 1, NullKnobsUI::kSelect, "SYNC", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "TRIGGER", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "EDGE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "LEVEL", 0, -1, true },
    { 5, NullKnobsUI::kSelect, "VIEW", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "GAIN", 1, -1, true },
    { 7, NullKnobsUI::kSelect, "HOLD", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
};

class UIOscilloscope : public NullKnobsUI
{
public:
    UIOscilloscope()
        : NullKnobsUI({ "OSCILLOSCOPE", "ADVANCED", 560, 240, nullptr, 2, nullosc::kUiSliders, nullosc::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nullosc::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<OscilloscopeBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    // as in oscilloscope_gfx.jsfx: 10 x 8 divisions, the trigger level, trace A (and B in
    // Left/Right), the time per division, the view and the trigger's state
    void drawDisplay() override
    {
        const nullosc::Params p = nullosc::paramsFrom([this](uint32_t i) { return nullosc::sliderValue(i, value(i)); });
        setPlot(fVX + 4.0f, fVY + 18.0f, fVW - 8.0f, fVH - 22.0f);
        grid(10, 8, true);
        if (p.trigger != 2)
        {
            const float y = fPY + fPH * float(0.5 - p.level * 0.5);
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(col(theme::kMuted, 0.5f));
            strokeWidth(1.0f);
            stroke();
        }

        const nullosc::View* v = view();
        const auto yOf = [this](float s) { return fPY + fPH * (0.5f - clampf(s, -1.0f, 1.0f) * 0.5f); };
        if (v)
            for (int tr = 0; tr < (p.view == 1 ? 2 : 1); ++tr)
            {
                const float* mn = tr ? v->bn : v->an;
                const float* mx = tr ? v->bx : v->ax;
                beginPath();
                float lx = 0.0f, ly = 0.0f;
                for (int k = 0; k < nullosc::kCols; ++k)
                {
                    const float x = fPX + fPW * float(k) / float(nullosc::kCols - 1);
                    const float y1 = yOf(mx[k]), y2 = yOf(mn[k]), ym = (y1 + y2) * 0.5f;
                    if (k > 0)
                    {
                        moveTo(lx, ly);
                        lineTo(x, ym);
                    }
                    if (y2 - y1 >= 0.5f)
                    {
                        moveTo(x, y1);
                        lineTo(x, y2);
                    }
                    lx = x;
                    ly = ym;
                }
                strokeColor(tr ? col(theme::kText, 0.55f) : col(theme::kAccent));
                strokeWidth(1.0f);
                stroke();
            }

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const double win = v ? double(v->win.load(std::memory_order_relaxed)) : 960.0;
        const double div = win / sr * 100.0; // ms per division (10 divisions)
        char buf[32];
        if (div >= 100.0)
            std::snprintf(buf, sizeof(buf), "%.2f S/DIV", div / 1000.0);
        else if (div >= 1.0)
            std::snprintf(buf, sizeof(buf), "%.1f MS/DIV", div);
        else
            std::snprintf(buf, sizeof(buf), "%.2f MS/DIV", div);
        label(fVX + 6.0f, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        label(fVX + fVW * 0.5f, fVY + 8.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted,
              p.view == 1 ? "LEFT / RIGHT" : p.view == 2 ? "SIDE" : "MID", true);
        const bool cap = v && v->state.load(std::memory_order_relaxed) != 0;
        const double wait = v ? double(v->wait.load(std::memory_order_relaxed)) : 0.0;
        label(fVX + fVW - 6.0f, fVY + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted,
              p.hold ? "HOLD" : p.trigger == 2 ? "FREE" : cap ? "RUN" : p.trigger == 1 || wait < win ? "WAIT" : "AUTO", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullosc::sliderValue(param, v);
        if (param == 0)
        {
            if (x >= 100.0)
                std::snprintf(buf, n, "%d ms", int(x));
            else
                std::snprintf(buf, n, "%.1f ms", x);
        }
        else if (param == 4)
            std::snprintf(buf, n, "%+.2f", x);
        else if (param == 6)
            std::snprintf(buf, n, "%+.1f dB", x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIOscilloscope();
}

END_NAMESPACE_DISTRHO
