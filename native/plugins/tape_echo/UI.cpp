/*
 * NULL JSFX - Tape Echo (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "TapeEchoDSP.hpp"
#include "TapeEchoSliders.hpp"

START_NAMESPACE_DISTRHO

// as tape_echo_gfx.jsfx: time, intensity, mix, heads, sync, freeze / saturation, bass, treble,
// wow/flutter, spring
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "TIME", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "INTENSITY", 0, -1, false },
    { 9, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "HEADS", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SYNC", 0, -1, false },
    { 10, NullKnobsUI::kSelect, "FREEZE", 0, -1, false },
    { 7, NullKnobsUI::kKnob, "SATURATION", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "BASS", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "TREBLE", 1, -1, true },
    { 6, NullKnobsUI::kKnob, "WOW/FLUTTER", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "SPRING", 1, -1, false },
};

class UITapeEcho : public NullKnobsUI
{
public:
    UITapeEcho()
        : NullKnobsUI({ "TAPE ECHO", "ESSENTIALS", 480, 120, nullptr, 2, nulltapeecho::kUiSliders,
                        nulltapeecho::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the repeat time in use: the host's tempo when synced (from the plugin), else Repeat Time
    float repeatMs() const
    {
        if (value(1) >= 0.5f)
            if (nulljsfx::Taps* t = taps())
                return t->extra.load(std::memory_order_relaxed);
        return float(nulltapeecho::repeatMs(value(0), std::lround(value(1)), 120.0));
    }

    // as in tape_echo_gfx.jsfx: the tape above (record head at 0, playback heads 1-3 at 1x, 2x,
    // 3x the repeat time, lit when active; the mode and what colours it named on its right) and
    // the repeats of an impulse below (level in dB, 0 to -48), the dry signal at 0
    void drawDisplay() override
    {
        const float ms = repeatMs();
        const float win = 6.0f * ms <= 2500.0f ? 2500.0f : 6.0f * ms <= 5000.0f ? 5000.0f
                        : 6.0f * ms <= 10000.0f ? 10000.0f : 15000.0f;
        const int pos = int(std::lround(value(2)));
        const int hm = nulltapeecho::headMask(pos);
        const bool frz = value(10) >= 0.5f;
        const int inten = int(std::lround(value(3))), spring = int(std::lround(value(8)));
        setPlot(fVX + 26.0f, fVY + 22.0f, fVW - 30.0f, fVH - 38.0f);
        grid(5, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-24");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-48");
        char buf[64];
        for (int i = 1; i <= 4; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%.1f s", i * double(win) / 5000.0);
            label(fPX + fPW * float(i) / 5.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }
        std::snprintf(buf, sizeof(buf), "%s%s%s", nulltapeecho::kHeadsOptions[std::max(0, std::min(11, pos))],
                      frz ? "  FREEZE" : inten > 100 ? "  RUNAWAY" : "", "");
        if ((hm & 8) && spring > 0)
        {
            char s[24];
            std::snprintf(s, sizeof(s), "  SPRING %d%%", spring);
            std::strncat(buf, s, sizeof(buf) - std::strlen(buf) - 1);
        }
        label(fPX + fPW, fVY + 10.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        // the tape and its heads
        const float ty = fVY + 10.0f;
        beginPath();
        moveTo(fPX, ty);
        lineTo(fPX + fPW * 0.52f, ty);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        head(fPX + 9.0f, ty, "REC", true, 18.0f);
        for (int k = 1; k <= 3; ++k)
        {
            const float x = linX(float(k) * ms, 0.0f, win);
            if (x > fPX + fPW - 6.0f)
                continue;
            const bool on = (hm >> (k - 1)) & 1;
            if (on)
                dashedV(x, 0.35f);
            std::snprintf(buf, sizeof(buf), "%d", k);
            head(x, ty, buf, on, 12.0f);
        }

        // the repeats
        const float bw = 3.0f, mix = value(9) / 100.0f;
        if (mix < 1.0f)
        {
            const float y = linY(20.0f * std::log10(1.0f - mix), -48.0f, 0.0f);
            beginPath();
            rect(fPX, y, bw, fPY + fPH - y);
            fillColor(col(theme::kMuted));
            fill();
        }
        if (mix > 0.0f && ms > 0.0f)
        {
            double lv[64];
            nulltapeecho::echoLevels(hm, inten / 100.0, frz, 64, lv);
            for (int j = 0; j < 64 && float(j + 1) * ms <= win; ++j)
            {
                const float g = mix * float(lv[j]);
                if (g <= 0.004f)
                    continue;
                const float x = linX(float(j + 1) * ms, 0.0f, win), y = linY(20.0f * std::log10(g), -48.0f, 0.0f);
                beginPath();
                rect(x - bw * 0.5f, y, bw, fPY + fPH - y);
                fillColor(col(theme::kAccent));
                fill();
            }
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
            std::snprintf(buf, n, "%d ms", int(std::lround(value(1) >= 0.5f ? repeatMs() : v)));
        else if (param == 4 || param == 5)
            std::snprintf(buf, n, v > 0.0f ? "+%.1f dB" : "%.1f dB", std::lround(v * 10.0f) / 10.0);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    // a head on the tape: a small box with its name, filled when active
    void head(float x, float y, const char* name, bool on, float w)
    {
        beginPath();
        roundedRect(x - w * 0.5f, y - 5.5f, w, 11.0f, 2.0f);
        fillColor(col(on ? theme::kAccent : theme::kCard));
        fill();
        strokeColor(col(on ? theme::kAccent : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x, y, ALIGN_CENTER, 7.0f, 0.0f, on ? theme::kCard : theme::kMuted, name, true);
    }
};

UI* createUI()
{
    return new UITapeEcho();
}

END_NAMESPACE_DISTRHO
