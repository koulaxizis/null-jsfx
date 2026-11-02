/*
 * NULL JSFX - Modal Piano (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As modal_piano_gfx.jsfx: the display holds the partials of C4 struck at velocity 100, the
 * decay of the fundamental from A0 to C8, the voices in use and the pedals, and a keyboard from
 * C2 to C7 with the keys sounding. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "ModalPianoDSP.hpp"
#include "ModalPianoSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "HARDNESS", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "UNISON", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "INHARMONIC", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "HAMMER", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "VELOCITY", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "DAMPER", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "BODY", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "WIDTH", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

class UIModalPiano : public NullKnobsUI
{
public:
    UIModalPiano()
        : NullKnobsUI({ "MODAL PIANO", "INSTRUMENTS", 560, 156, nullptr, 2, nullpiano::kSliders, nullpiano::kNumParams, kControls, kNumControls, true })
    {
    }

protected:
    static bool white(int pc) { return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11; }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    void drawDisplay() override
    {
        double vals[nullpiano::kNumParams];
        for (uint32_t i = 0; i < nullpiano::kNumParams; ++i)
            vals[i] = jsfxValue(nullpiano::kSliders[i], value(i));
        const double hard = std::pow(2.0, (vals[0] - 50.0) / 40.0);
        const double dec = vals[1] / 100.0, inh = vals[3] / 100.0, velo = vals[5] / 100.0;

        // the partials of C4 at velocity 100, -60 to 0 dB over 20 Hz to 20 kHz
        setPlot(24.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        const double vh = std::max(0.05, std::min(1.0, 0.65 + (100.0 / 127.0 - 0.65) * velo));
        const nullpiano::Key c4 = nullpiano::Key::of(60, inh, dec);
        const double fh = (900.0 + 7000.0 * vh * vh) * hard;
        double m = 0.0;
        for (int k = 1; k <= nullpiano::kNP; ++k)
        {
            const double fk = k * c4.f0 * std::sqrt(1.0 + c4.B * k * k);
            const double a = std::fabs(std::sin(M_PI * k / 8.0)) / std::pow(double(k), 0.9) * std::exp(-fk / fh);
            if (k == 1)
                m = a;
            if (a > 0.000001 && fk < 20000.0)
            {
                const float x = fPX + fPW * float(std::log(std::max(fk, 20.0) / 20.0) / std::log(1000.0));
                const float y = linY(float(20.0 * std::log10(a / m)), -60.0f, 0.0f);
                box(x - 1.0f, y, 2.0f, fPY + fPH - y, theme::kAccent, k <= 6 ? 1.0f : 0.6f);
            }
        }
        label(30.0f, 72.0f, ALIGN_LEFT, 6.5f, 0.5f, theme::kMuted, "PARTIALS, C4", true);

        // the decay of the fundamental from A0 to C8, 0.1 to 100 s (log)
        setPlot(284.0f, 66.0f, 252.0f, 70.0f);
        grid(4, 3, false);
        float xs[88], ys[88];
        for (int j = 0; j < 88; ++j)
        {
            const double t = 22.0 * std::pow(2.0, -j / 17.0) * dec;
            xs[j] = fPX + fPW * float(j) / 87.0f;
            ys[j] = linY(float(std::log10(t)), -1.0f, 2.0f);
        }
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int j = 1; j < 88; ++j)
            lineTo(xs[j], ys[j]);
        strokeColor(col(theme::kAccent, 1.0f));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        label(530.0f, 72.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, "DECAY, A0 TO C8", true);

        // the voices in use and the pedals
        bool on[128] = {};
        int voices = 0, pedals = 0;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
                on[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            pedals = int(std::lround(t->aux[8].load(std::memory_order_relaxed)));
            voices = int(std::lround(t->extra.load(std::memory_order_relaxed)));
        }
        char info[32];
        std::snprintf(info, sizeof(info), "VOICES %d", voices);
        label(24.0f, 146.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        const char* ped = pedals == 3 ? "SUSTAIN  SOFT" : pedals == 1 ? "SUSTAIN" : pedals == 2 ? "SOFT" : "";
        label(536.0f, 146.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, ped, true);

        // the keyboard from C2 to C7
        const float ky = 156.0f, kh = 44.0f, ww = 512.0f / 36.0f, bw = ww * 0.62f, bh = kh * 0.6f;
        int i = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (!white(n % 12))
                continue;
            const float x = 24.0f + float(i) * ww;
            box(x + 0.5f, ky, ww - 1.0f, kh, on[n] ? theme::kAccent : 0x2d2b28);
            if (n % 12 == 0)
            {
                char b[8];
                std::snprintf(b, sizeof(b), "C%d", n / 12 - 1);
                label(x + ww * 0.5f, ky + kh - 5.0f, ALIGN_CENTER, 6.0f, 0.0f, on[n] ? theme::kBackground : theme::kMuted, b, true);
            }
            ++i;
        }
        i = 0;
        for (int n = 36; n <= 96; ++n)
        {
            if (white(n % 12))
            {
                ++i;
                continue;
            }
            const float x = 24.0f + float(i) * ww - bw * 0.5f;
            box(x - 1.0f, ky, bw + 2.0f, bh + 1.0f, theme::kCard);
            box(x, ky, bw, bh, on[n] ? theme::kAccent : 0x131211);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 9)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 2)
            std::snprintf(buf, n, "%.1f ct", v);
        else if (param == 6)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f s", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIModalPiano();
}

END_NAMESPACE_DISTRHO
