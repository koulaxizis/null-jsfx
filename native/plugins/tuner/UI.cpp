/*
 * NULL JSFX - Tuner (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TunerSliders.hpp"

#include <chrono>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "A4", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "DISPLAY", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "INPUT", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "OUTPUT", 0, -1, false },
};

class UITuner : public NullKnobsUI
{
public:
    UITuner()
        : NullKnobsUI({ "TUNER", "ADVANCED", 480, 160, nullptr, 1, nulltuner::kSliders, nulltuner::kNumSliders, kControls,
                        sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    float freq() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? static_cast<TunerBase*>(p)->view.freq.load(std::memory_order_relaxed) : 0.0f;
#else
        return 0.0f;
#endif
    }

    // tg_row: blocks 10 wide every 20, moved by phase ph (cycles), clipped to the bar
    void strobeRow(float y, double ph, const Color& c)
    {
        const float o = float((ph - std::floor(ph)) * 20.0);
        beginPath();
        for (float x = -20.0f + o; x < 400.0f; x += 20.0f)
        {
            const float a = std::fmax(0.0f, x), b = std::fmin(400.0f, x + 10.0f);
            if (b > a)
                rect(40.0f + a, y, b - a, 11.0f);
        }
        fillColor(c);
        fill();
    }

    // as the @gfx of tuner_gfx.jsfx: the note, cents and frequency, then the needle bar or the
    // strobe (0.05 / 0.1 / 0.2 cycles a second per cent off, right = sharp)
    void drawDisplay() override
    {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const double dt = fLast > 0.0 ? std::fmin(0.1, std::fmax(0.0, now - fLast)) : 0.0;
        fLast = now;
        const double f = freq(), a4 = std::lround(value(0));
        const bool on = f > 0.0;
        double c = 0.0;
        int pc = 0, oct = 0;
        if (on)
        {
            const double m = 69.0 + 12.0 * std::log(f / a4) / std::log(2.0);
            const double n = std::floor(m + 0.5);
            c = (m - n) * 100.0;
            pc = int(n - 12.0 * std::floor(n / 12.0));
            oct = int(std::floor(n / 12.0)) - 1;
            fS1 += c * dt * 0.05;
            fS2 += c * dt * 0.1;
            fS3 += c * dt * 0.2;
        }
        char buf[48];
        std::snprintf(buf, sizeof(buf), "A4 = %d HZ", int(a4));
        label(fVX + 10.0f, fVY + 12.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, buf, true);
        label(fVX + fVW - 10.0f, fVY + 12.0f, ALIGN_RIGHT, 7.0f, 1.2f, on && std::fabs(c) < 2.0 ? theme::kText : theme::kBorder,
              "IN TUNE", true);
        static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (on)
            std::snprintf(buf, sizeof(buf), "%s%d", kNames[pc], oct);
        else
            std::snprintf(buf, sizeof(buf), "-");
        label(240.0f, fVY + 42.0f, ALIGN_CENTER, 30.0f, 0.0f, on ? theme::kText : theme::kBorder, buf, true);
        if (on)
            std::snprintf(buf, sizeof(buf), "%+.1f CENTS   %.1f HZ", c, f);
        else
            std::snprintf(buf, sizeof(buf), "NO SIGNAL");
        label(240.0f, fVY + 74.0f, ALIGN_CENTER, 8.0f, 1.0f, theme::kMuted, buf, true);

        if (value(1) < 0.5f)
        {
            for (int i = -50; i <= 50; i += 10)
            {
                const float x = std::floor(40.0f + float(i + 50) * 4.0f) + 0.5f;
                beginPath();
                moveTo(x, i == 0 ? 152.0f : 166.0f);
                lineTo(x, i == 0 ? 192.0f : 178.0f);
                strokeColor(i == 0 ? col(theme::kText, 0.8f) : col(theme::kMuted, 0.6f));
                strokeWidth(1.0f);
                stroke();
            }
            beginPath();
            moveTo(40.0f, 172.5f);
            lineTo(440.0f, 172.5f);
            strokeColor(col(theme::kBorder));
            stroke();
            if (on)
            {
                const float x = 40.0f + float(std::fmin(50.0, std::fmax(-50.0, c)) + 50.0) * 4.0f;
                beginPath();
                rect(std::fmin(240.0f, x), 167.0f, std::fabs(x - 240.0f), 10.0f);
                fillColor(col(theme::kAccent, 0.25f));
                fill();
                beginPath();
                rect(x - 1.5f, 154.0f, 3.0f, 36.0f);
                fillColor(col(theme::kAccent));
                fill();
            }
        }
        else
        {
            const Color sc = on ? col(theme::kAccent, 0.9f) : col(theme::kBorder);
            strobeRow(152.0f, fS1, sc);
            strobeRow(166.0f, fS2, sc);
            strobeRow(180.0f, fS3, sc);
        }
        label(40.0f, 199.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, "FLAT");
        label(440.0f, 199.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, "SHARP");
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    double fLast = 0.0, fS1 = 0.0, fS2 = 0.0, fS3 = 0.0;
};

UI* createUI()
{
    return new UITuner();
}

END_NAMESPACE_DISTRHO
