/*
 * NULL JSFX - Half Speed (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "HalfSpeedSliders.hpp"

START_NAMESPACE_DISTRHO

// as half_speed_gfx.jsfx: one row (length, speed, pitch keep, tone, mix, offset)
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LENGTH", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SPEED", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "PITCH KEEP", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "TONE", 0, -1, false, true },
    { 4, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "OFFSET", 0, -1, false },
};

class UIHalfSpeed : public NullKnobsUI
{
public:
    UIHalfSpeed()
        : NullKnobsUI({ "HALF SPEED", "ADVANCED", 480, 120, nullptr, 1, nullhalfspeed::kUiSliders,
                        nullhalfspeed::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nullhalfspeed::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<HalfSpeedBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    // as in half_speed_gfx.jsfx: the captured Length as a waveform (dimmer where this cycle has
    // not written yet), the write head (solid) and the slowed read head (dashed); below, the
    // cycle (Length / Speed) with the part already played filled and a tick where the capture ends
    void drawDisplay() override
    {
        const int len = int(std::lround(value(0))), spd = int(std::lround(value(1)));
        const bool keep = value(2) >= 0.5f;
        const int tone = int(std::lround(value(3))), off = int(std::lround(value(5)));
        static const char* const kLen[] = { "1/4 NOTE", "1/2 NOTE", "1 BAR", "2 BARS" };
        static const char* const kSpd[] = { "1/2 SPEED", "1/4 SPEED", "2/3 SPEED" };
        char line[96], c[24] = "", d[24] = "";
        std::snprintf(line, sizeof(line), "CAPTURE %s", kLen[len & 3]);
        label(30.0f, 70.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, line);
        if (tone < 20000)
            std::snprintf(c, sizeof(c), "  TONE %.1f kHz", tone / 1000.0);
        if (off > 0)
            std::snprintf(d, sizeof(d), "  OFFSET %d", off);
        std::snprintf(line, sizeof(line), "%s%s%s%s", kSpd[spd < 3 ? spd : 0], keep ? "  PITCH KEPT" : "", c, d);
        label(450.0f, 70.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);

        float w = 0.0f, r = 0.0f, cyc = 0.0f, cap = 0.5f;
        const nullhalfspeed::View* v = view();
        if (v)
        {
            w = v->write.load(std::memory_order_relaxed);
            r = v->read.load(std::memory_order_relaxed);
            cyc = v->cycle.load(std::memory_order_relaxed);
            cap = v->capture.load(std::memory_order_relaxed);
        }
        else
            cap = float(nullhalfspeed::speedRatio(spd));

        // waveform
        box(30.0f, 78.0f, 420.0f, 50.0f, theme::kBackground, 1.0f);
        constexpr int nb = nullhalfspeed::HalfSpeedProcessor::kBins;
        const float bw = 420.0f / nb;
        for (int k = 0; k < nb; ++k)
        {
            const float pk = v ? v->peaks[k].load(std::memory_order_relaxed) : 0.0f;
            const float h = std::fmax(0.5f, std::fmin(1.0f, pk) * 24.0f);
            box(30.0f + k * bw, 103.0f - h, std::fmax(1.0f, bw - 1.0f), 2.0f * h, theme::kAccent,
                float(k + 1) / nb <= w ? 0.6f : 0.25f);
        }
        box(std::floor(30.0f + 420.0f * w), 78.0f, 1.0f, 50.0f, theme::kAccent, 1.0f);
        for (float y = 78.0f; y < 128.0f; y += 6.0f)
            box(std::floor(30.0f + 420.0f * r), y, 1.0f, std::fmin(y + 3.0f, 128.0f) - y, theme::kText, 0.8f);

        // cycle
        box(30.0f, 136.0f, 420.0f, 6.0f, theme::kBackground, 1.0f);
        box(30.0f, 136.0f, 420.0f * cyc, 6.0f, theme::kAccent, 0.35f);
        box(std::floor(30.0f + 420.0f * cap), 134.0f, 1.0f, 10.0f, theme::kMuted, 1.0f);

        // legend
        box(30.0f, 154.0f, 10.0f, 2.0f, theme::kAccent, 1.0f);
        label(44.0f, 155.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "WRITE");
        box(90.0f, 154.0f, 3.0f, 2.0f, theme::kText, 0.8f);
        box(96.0f, 154.0f, 3.0f, 2.0f, theme::kText, 0.8f);
        label(104.0f, 155.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, "READ");
        const double cb = nullhalfspeed::lengthBeats(len) / nullhalfspeed::speedRatio(spd);
        if (cb == std::floor(cb))
            std::snprintf(line, sizeof(line), "CYCLE %d BEATS", int(cb));
        else
            std::snprintf(line, sizeof(line), "CYCLE %.1f BEATS", cb);
        label(450.0f, 155.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 3 && v >= 20000.0f)
            std::snprintf(buf, n, "OFF");
        else if (param == 3)
            std::snprintf(buf, n, "%.1f kHz", std::lround(v) / 1000.0);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIHalfSpeed();
}

END_NAMESPACE_DISTRHO
