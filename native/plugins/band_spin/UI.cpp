/*
 * NULL JSFX - Band Spin (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BandSpinSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "XOVER 1", 0, -1, false, true },
    { 1, NullKnobsUI::kKnob, "XOVER 2", 0, -1, false, true },
    { 2, NullKnobsUI::kKnob, "XOVER 3", 0, -1, false, true },
    { 7, NullKnobsUI::kKnob, "VOLUME", 0, -1, false },
    { 8, NullKnobsUI::kKnob, "PAN", 0, -1, false },
    { 9, NullKnobsUI::kKnob, "DOPPLER", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SPEED 1", 1, -1, false, true },
    { 4, NullKnobsUI::kKnob, "SPEED 2", 1, -1, false, true },
    { 5, NullKnobsUI::kKnob, "SPEED 3", 1, -1, false, true },
    { 6, NullKnobsUI::kKnob, "SPEED 4", 1, -1, false, true },
    { 10, NullKnobsUI::kKnob, "FOLLOW", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "MIX", 1, -1, false },
};

class UIBandSpin : public NullKnobsUI
{
public:
    UIBandSpin()
        : NullKnobsUI({ "BAND SPIN", "ADVANCED", 480, 120, nullptr, 2, nullbandspin::kSliders, nullbandspin::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // a rotor seen from above (as bs_rotor in band_spin_gfx.jsfx): the body and the rotor
    // pointing at angle ph, the band's name and its speed now
    void rotor(float cx, float cy, float r, float ph, const char* name, float hz)
    {
        beginPath();
        circle(cx, cy, r);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        const float a = 2.0f * float(M_PI) * ph, s = std::sin(a), c = std::cos(a);
        beginPath();
        moveTo(cx - s * r * 0.45f, cy + c * r * 0.45f);
        lineTo(cx + s * (r - 3.0f), cy - c * (r - 3.0f));
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineCap(ROUND);
        stroke();
        beginPath();
        circle(cx + s * (r - 3.0f), cy - c * (r - 3.0f), 2.5f);
        circle(cx, cy, 3.0f);
        fillColor(col(theme::kAccent));
        fill();
        label(cx, cy + r + 13.0f, ALIGN_CENTER, 7.5f, 1.5f, theme::kMuted, name, true);
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.2f Hz", double(hz));
        label(cx, cy + r + 25.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, buf);
    }

    // as in band_spin_gfx.jsfx: the four rotors at their current angle and speed
    void drawDisplay() override
    {
        static const char* const kNames[4] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        static const float kX[4] = { 98.0f, 193.0f, 288.0f, 383.0f };
        nulljsfx::Taps* t = taps();
        for (int b = 0; b < 4; ++b)
        {
            const float ph = t ? t->aux[b].load(std::memory_order_relaxed) : 0.25f * float(b);
            const float hz = t ? t->aux[4 + b].load(std::memory_order_relaxed) : value(uint32_t(3 + b));
            rotor(kX[b], 92.0f, 24.0f, ph, kNames[b], hz);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param <= 2)
        {
            if (param == 2 || (param == 1 && v >= 1000.0f))
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d Hz", int(v));
        }
        else if (param <= 6)
            std::snprintf(buf, n, "%.2f Hz", v);
        else
            std::snprintf(buf, n, "%d%%", int(v));
    }
};

UI* createUI()
{
    return new UIBandSpin();
}

END_NAMESPACE_DISTRHO
