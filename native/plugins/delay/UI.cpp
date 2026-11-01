/*
 * NULL JSFX - Delay (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DelayDSP.hpp"
#include "DelaySliders.hpp"

START_NAMESPACE_DISTRHO

// as delay_gfx.jsfx: tab ECHO (time, feedback, mix, mode / sync, note, freeze) and tab CHARACTER
// (low cut, high cut, drive / modulation, diffusion, ducking)
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIME", 0, 0, false },
    { 1, NullKnobsUI::kKnob, "FEEDBACK", 0, 0, false },
    { 2, NullKnobsUI::kKnob, "MIX", 0, 0, false },
    { 5, NullKnobsUI::kSelect, "MODE", 0, 0, false },
    { 3, NullKnobsUI::kSelect, "SYNC", 1, 0, false },
    { 4, NullKnobsUI::kSelect, "NOTE", 1, 0, false },
    { 12, NullKnobsUI::kSelect, "FREEZE", 1, 0, false },
    { 13, NullKnobsUI::kSelect, "REVERSE", 1, 0, false },
    { 14, NullKnobsUI::kKnob, "PITCH", 1, 0, true },
    { 6, NullKnobsUI::kKnob, "LOW CUT", 0, 1, false, true },
    { 7, NullKnobsUI::kKnob, "HIGH CUT", 0, 1, false, true },
    { 8, NullKnobsUI::kKnob, "DRIVE", 0, 1, false },
    { 9, NullKnobsUI::kKnob, "MODULATION", 1, 1, false },
    { 10, NullKnobsUI::kKnob, "DIFFUSION", 1, 1, false },
    { 11, NullKnobsUI::kKnob, "DUCKING", 1, 1, false },
};

class UIDelay : public NullKnobsUI
{
public:
    UIDelay()
        : NullKnobsUI({ "DELAY", "ESSENTIALS", 480, 120, "ECHO|CHARACTER", 2, nulldelay::kUiSliders, nulldelay::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the delay time actually used: the host's tempo when synced (from the plugin), else Time
    float delayMs() const
    {
        if (value(3) >= 0.5f)
            if (nulljsfx::Taps* t = taps())
                return t->extra.load(std::memory_order_relaxed);
        return float(nulldelay::delayMs(value(0), value(3) >= 0.5f, int(std::lround(value(4))), 120.0));
    }

    // as in delay_gfx.jsfx: the dry signal at 0 and the echoes over 2.5 s (7.5 s for longer times),
    // level in dB (0 to -48); in Ping-Pong the right-channel echoes are dimmer, Diffusion smears
    // each echo, Freeze holds them level, and what colours the loop is named
    void drawDisplay() override
    {
        const float ms = delayMs();
        const float win = ms > 2000.0f ? 7500.0f : 2500.0f;
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(5, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-48");
        char buf[16];
        for (int i = 1; i <= 4; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%.1f s", i * double(win) / 5000.0);
            label(fPX + fPW * float(i) / 5.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
        const bool ping = value(5) >= 0.5f, frz = value(12) >= 0.5f;
        const int lc = int(std::lround(value(6))), hc = int(std::lround(value(7))), drv = int(std::lround(value(8)));
        const int mod = int(std::lround(value(9))), dif = int(std::lround(value(10))), duck = int(std::lround(value(11)));
        char b[24] = "", c[24] = "", d[24] = "", line[96];
        if (lc > 20)
            std::snprintf(b, sizeof(b), "  LC %d Hz", lc);
        if (hc < 20000)
        {
            if (hc >= 1000)
                std::snprintf(c, sizeof(c), "  HC %.1f kHz", hc / 1000.0);
            else
                std::snprintf(c, sizeof(c), "  HC %d Hz", hc);
        }
        if (drv > 0)
            std::snprintf(d, sizeof(d), "  DRIVE %d%%", drv);
        std::snprintf(line, sizeof(line), "%s%s%s%s",
                      frz ? (ping ? "FREEZE  PING-PONG" : "FREEZE") : (ping ? "PING-PONG" : ""), b, c, d);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);
        b[0] = c[0] = d[0] = 0;
        if (mod > 0)
            std::snprintf(b, sizeof(b), "MOD %d%%", mod);
        if (dif > 0)
            std::snprintf(c, sizeof(c), "  DIFF %d%%", dif);
        if (duck > 0)
            std::snprintf(d, sizeof(d), "  DUCK %d%%", duck);
        std::snprintf(line, sizeof(line), "%s%s%s", b, c, d);
        label(fPX + fPW - 6.0f, fPY + 21.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);

        const float bw = 3.0f, mix = value(2) / 100.0f;
        if (mix < 1.0f)
        {
            const float y = linY(20.0f * std::log10(1.0f - mix), -48.0f, 0.0f);
            beginPath();
            rect(fPX, y, bw, fPY + fPH - y);
            fillColor(col(theme::kMuted));
            fill();
        }
        const float fb = frz ? 1.0f : value(1) / 100.0f * 0.95f, tw = dif / 100.0f * 36.0f;
        if (mix > 0.0f && ms > 0.0f)
        {
            float g = mix;
            int k = 0;
            for (float t = ms; t <= win && g > 0.004f; t += ms, g *= fb, ++k)
            {
                const float x = linX(t, 0.0f, win), y = linY(20.0f * std::log10(g), -48.0f, 0.0f);
                if (dif > 0)
                {
                    // the smear: six fading steps after the echo
                    for (int j = 0; j < 6; ++j)
                    {
                        if (x + tw * float(j + 1) / 6.0f > fPX + fPW)
                            break;
                        const float h = (fPY + fPH - y) * 0.7f * (1.0f - j / 6.0f);
                        beginPath();
                        rect(x + tw * float(j) / 6.0f, fPY + fPH - h, tw / 6.0f, h);
                        fillColor(col(theme::kAccent, 0.24f * (1.0f - j / 6.0f)));
                        fill();
                    }
                }
                beginPath();
                rect(x - bw * 0.5f, y, bw, fPY + fPH - y);
                fillColor(col(theme::kAccent, ping && (k % 2) ? 0.45f : 1.0f));
                fill();
            }
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%d ms", int(std::lround(value(3) >= 0.5f ? delayMs() : v)));
        else if (param == 6 && v <= 20.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 7 && v >= 20000.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 7 && v >= 1000.0f)
            std::snprintf(buf, n, "%.1f kHz", std::lround(v) / 1000.0);
        else if (param == 14)
            std::snprintf(buf, n, "%+d st", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDelay();
}

END_NAMESPACE_DISTRHO
