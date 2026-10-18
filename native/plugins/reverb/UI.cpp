/*
 * NULL JSFX - Reverb (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ReverbDSP.hpp"
#include "ReverbSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "SIZE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DAMPING", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "PRE-DELAY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "BYPASS", 0, -1, false },
    { 11, NullKnobsUI::kSelect, "CHARACTER", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "LOW CUT", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "HIGH CUT", 1, -1, false, true },
    { 8, NullKnobsUI::kKnob, "WIDTH", 1, -1, true },
    { 9, NullKnobsUI::kKnob, "MODULATION", 1, -1, false },
    { 10, NullKnobsUI::kSelect, "FREEZE", 1, -1, false },
};

class UIReverb : public NullKnobsUI
{
public:
    UIReverb()
        : NullKnobsUI({ "REVERB", "ESSENTIALS", 480, 120, nullptr, 2, nullreverb::kUiSliders, nullreverb::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), false }) {}

protected:
    // as in reverb_gfx.jsfx: the tail's decay over 3 s, level in dB (0 to -60), starting after the
    // pre-delay: lows (the comb feedback) in the accent, highs at 5 kHz (feedback times the damping
    // low-pass) muted; frozen, the tail holds at 0 dB
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(6, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-30");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-60");
        char buf[96];
        for (int i = 1; i <= 5; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%.1f s", i * 0.5);
            label(fPX + fPW * float(i) / 6.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
        }

        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        const int chr = int(std::lround(value(11)));
        const bool frz = value(10) > 0.0f;
        const nullreverb::Params p = nullreverb::paramsFor(value(0), value(1), value(2), value(3), sr, chr, frz);
        const double cf = nullreverb::combScale(chr);
        const double w = 2.0 * M_PI * 5000.0 / sr;
        const double lp = p.dampA / std::sqrt(1.0 - 2.0 * p.damp * std::cos(w) + p.damp * p.damp);
        const double lo = frz ? 99.0 : rt60(p.fb, p.room * cf), hi = frz ? 99.0 : rt60(p.fb * lp, p.room * cf);
        const bool bypass = value(4) > 0.0f;
        const float a = bypass ? 0.35f : 1.0f;
        const int pdMs = int(std::lround(value(5)));
        const float pd = pdMs / 1000.0f;
        const float x0 = linX(pd, 0.0f, 3.0f);

        // the pre-delay gap: a dashed line where the tail starts
        if (pdMs > 0)
        {
            const float xd = std::floor(x0) + 0.5f;
            beginPath();
            for (float y = fPY; y < fPY + fPH; y += 6.0f)
            {
                moveTo(xd, y);
                lineTo(xd, std::fmin(y + 3.0f, fPY + fPH));
            }
            strokeColor(col(theme::kMuted, 0.6f * a));
            strokeWidth(1.0f);
            stroke();
            std::snprintf(buf, sizeof(buf), "PRE %d ms", pdMs);
            label(x0 + 5.0f, fPY + fPH - 8.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        // highs
        float te = float(std::min(hi, 3.0 - pd));
        beginPath();
        moveTo(x0, fPY);
        lineTo(linX(pd + te, 0.0f, 3.0f), linY(-60.0f * te / float(hi), -60.0f, 0.0f));
        strokeColor(col(theme::kMuted, a));
        strokeWidth(2.0f);
        stroke();
        // lows, shaded down to -60
        te = float(std::min(lo, 3.0 - pd));
        const float x2 = linX(pd + te, 0.0f, 3.0f), y2 = linY(-60.0f * te / float(lo), -60.0f, 0.0f);
        beginPath();
        moveTo(x0, fPY);
        lineTo(x2, y2);
        lineTo(x2, fPY + fPH);
        lineTo(x0, fPY + fPH);
        closePath();
        fillColor(col(theme::kAccent, 0.10f * a));
        fill();
        beginPath();
        moveTo(x0, fPY);
        lineTo(x2, y2);
        strokeColor(col(theme::kAccent, a));
        strokeWidth(2.0f);
        stroke();

        const char* c = chr == 2 ? "HALL   " : (chr == 1 ? "PLATE   " : "");
        char m[24] = "";
        const int mod = int(std::lround(value(9)));
        if (mod > 0)
            std::snprintf(m, sizeof(m), "MOD %d%%   ", mod);
        if (bypass)
            std::snprintf(buf, sizeof(buf), "BYPASSED");
        else if (frz)
            std::snprintf(buf, sizeof(buf), "%s%sFROZEN", c, m);
        else
            std::snprintf(buf, sizeof(buf), "%s%sRT60 %.2f s   5 kHz %.2f s", c, m, lo, hi);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 6 && v <= 20.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 7 && v >= 20000.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 7)
            std::snprintf(buf, n, "%.1f kHz", std::lround(v) / 1000.0);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    // seconds for the tail to fall 60 dB with comb loop gain g (rv_rt60 in the GFX JSFX; room
    // already times the Character's comb scale)
    static double rt60(double g, double room)
    {
        return g > 0.0 && g < 1.0 ? -60.0 / (20.0 * std::log10(g)) * 1588.0 * room / 44100.0 : 99.0;
    }
};

UI* createUI()
{
    return new UIReverb();
}

END_NAMESPACE_DISTRHO
