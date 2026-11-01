/*
 * NULL JSFX - Phaser (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PhaserDSP.hpp"
#include "PhaserSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "RATE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "STAGES", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DEPTH", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "FEEDBACK", 1, -1, true },
    { 5, NullKnobsUI::kKnob, "CENTER", 1, -1, false, true },
    { 6, NullKnobsUI::kKnob, "STEREO", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "SYNC", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "BARBER", 1, -1, false },
};

class UIPhaser : public NullKnobsUI
{
public:
    UIPhaser()
        : NullKnobsUI({ "PHASER", "ESSENTIALS", 480, 120, nullptr, 2, nullphaser::kUiSliders, nullphaser::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in phaser_gfx.jsfx: the response (dry plus wet) at the current LFO position, the sweep
    // range shaded; with Stereo Phase the right channel's curve is dimmer
    void drawDisplay() override
    {
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        nullphaser::PhaserProcessor p; // the @slider state for the current values
        p.setSampleRate(sr);
        nullphaser::applyParams(p, [this](uint32_t i) { return value(i); });

        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(24.0f);
        const float xl = freqX(float(p.sweepLoHz())), xh = freqX(float(p.sweepHiHz()));
        beginPath();
        rect(xl, fPY, std::fmax(1.0f, xh - xl), fPH);
        fillColor(col(theme::kAccent, 0.07f));
        fill();

        char buf[40];
        const int stages = int(std::lround(value(1))), notches = stages / 2;
        if (stages < 2)
            std::snprintf(buf, sizeof(buf), "1 STAGE, NO NOTCH");
        else if (notches == 1)
            std::snprintf(buf, sizeof(buf), "%d STAGES, 1 NOTCH", stages);
        else
            std::snprintf(buf, sizeof(buf), "%d STAGES, %d NOTCHES", stages, notches);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        const double ph = tapExtra() * nullphaser::kTwoPi;
        if (p.barber())
        {
            // Barber Pole: the first voice's position on the ramp
            if (value(6) > 0.0f)
                response(p, sr, p.barberCoef(p.barberPos(ph, true)), 0.45f);
            response(p, sr, p.barberCoef(p.barberPos(ph, false)), 1.0f);
            return;
        }
        if (value(6) > 0.0f)
            response(p, sr, p.coefAt(ph + p.stereoOffset()), 0.45f);
        response(p, sr, p.coefAt(ph), 1.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (value(7) >= 0.5f)
                std::snprintf(buf, n, "SYNC");
            else
                std::snprintf(buf, n, "%.1f Hz", v);
            return;
        }
        if (param == 5)
        {
            const int hz = int(std::lround(v));
            if (hz >= 1000)
                std::snprintf(buf, n, "%.2f kHz", hz / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", hz);
            return;
        }
        if (param == 6)
        {
            std::snprintf(buf, n, "%d deg", int(std::lround(v)));
            return;
        }
        NullKnobsUI::formatValue(param, v, buf, n);
    }

    // the response curve for allpass coefficient c across the plot (20 Hz to 20 kHz)
    void response(const nullphaser::PhaserProcessor& p, double sr, double c, float alpha)
    {
        const int n = std::max(2, int(fPW) + 1);
        beginPath();
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(fPW));
            const float y = linY(float(nullphaser::responseDb(f, sr, c, p.stages(), p.feedback(), p.mixW())), -16.0f, 16.0f);
            if (i == 0)
                moveTo(fPX, y);
            else
                lineTo(fPX + float(i), y);
        }
        strokeColor(col(theme::kAccent, alpha));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
    }

    float tapExtra() const
    {
        nulljsfx::Taps* t = taps();
        return t ? t->extra.load(std::memory_order_relaxed) : 0.0f;
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIPhaser)
};

UI* createUI()
{
    return new UIPhaser();
}

END_NAMESPACE_DISTRHO
