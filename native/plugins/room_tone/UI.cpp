/*
 * NULL JSFX - Room Tone (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "RoomToneSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LEARN", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "FILL LEVEL", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "FADE", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "HOLD", 1, -1, false },
    { 6, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
};

class UIRoomTone : public NullKnobsUI
{
public:
    UIRoomTone()
        : NullKnobsUI({ "ROOM TONE", "ESSENTIALS", 480, 150, nullptr, 2, nullrt::kSliders, nullrt::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kCols = 200;

    const nullrt::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<RoomToneBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    // as in room_tone_gfx.jsfx: the learnt room tone (the mean power of both channels per bin,
    // silver area) on a log-frequency axis, 0 dB = a full-scale sine, down to -120 dB; the status
    // on the left and the fill lamp on the right, above the plot
    void drawDisplay() override
    {
        setPlot(fVX + 30.0f, fVY + 14.0f, fVW - 34.0f, fVH - 30.0f);
        grid(1, 4, false);
        strokeWidth(1.0f);
        for (float f = 100.0f; f <= 10000.0f; f *= 10.0f)
        {
            const float x = std::floor(freqX(f)) + 0.5f;
            beginPath();
            moveTo(x, fPY);
            lineTo(x, fPY + fPH);
            strokeColor(col(theme::kBorder, 0.6f));
            stroke();
        }
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-60");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-120");
        label(freqX(100.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");

        const nullrt::View* v = view();
        const bool learning = v && v->learning.load(std::memory_order_relaxed) != 0;
        const float learnt = v ? v->learnt.load(std::memory_order_relaxed) : 0.0f;
        const float fillG = v ? v->fill.load(std::memory_order_relaxed) : 0.0f;

        char buf[48];
        if (learning)
            std::snprintf(buf, sizeof(buf), "LEARNING %.1f S", double(learnt));
        else if (learnt > 0.0f)
            std::snprintf(buf, sizeof(buf), "PROFILE %.1f S", double(learnt));
        else
            std::snprintf(buf, sizeof(buf), "NO PROFILE: LEARN ON ROOM TONE");
        const float ty = fVY + 4.0f;
        label(fPX, ty, ALIGN_LEFT, 8.0f, 0.0f, learning ? theme::kText : theme::kMuted, buf);
        std::snprintf(buf, sizeof(buf), "FILL %d%%", int(std::floor(fillG * 100.0f + 0.5f)));
        label(fPX + fPW, ty, ALIGN_RIGHT, 8.0f, 0.0f, fillG > 0.05f ? theme::kAccent : theme::kMuted, buf);
        if (!v || !(learnt > 0.0f))
            return;

        // the profile over 201 columns: the loudest bin under each column
        const double sr = v->sampleRate.load(std::memory_order_relaxed);
        const int nb = nullrt::kNB, n = nullrt::kN;
        static float xs[kCols + 1], ys[kCols + 1];
        for (int j = 0; j <= kCols; ++j)
        {
            const int b0 = std::min(nb - 1, int(std::floor(20.0 * std::pow(1000.0, j / 200.0) * n / sr)));
            const int b1 = std::max(b0, std::min(nb - 1, int(std::floor(20.0 * std::pow(1000.0, (j + 1) / 200.0) * n / sr))));
            float m = v->profile[b0];
            for (int b = b0 + 1; b <= b1; ++b)
                m = std::fmax(m, v->profile[b]);
            xs[j] = fPX + fPW * float(j) / 200.0f;
            ys[j] = linY(std::fmax(-120.0f, std::fmin(0.0f, m)), -120.0f, 0.0f);
        }
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(xs[0], fPY + fPH);
        for (int j = 0; j <= kCols; ++j)
            lineTo(xs[j], ys[j]);
        lineTo(xs[kCols], fPY + fPH);
        closePath();
        fillColor(col(theme::kAccent, 0.14f));
        fill();
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int j = 1; j <= kCols; ++j)
            lineTo(xs[j], ys[j]);
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        lineJoin(ROUND);
        stroke();
        resetScissor();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIRoomTone();
}

END_NAMESPACE_DISTRHO
