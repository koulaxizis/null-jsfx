/*
 * NULL JSFX - Octaver (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "OctaverSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "DRY", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "OCT -1", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "OCT -2", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "UP", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "TONE", 0, -1, false, true },
    { 5, NullKnobsUI::kKnob, "TRACKING", 0, -1, false, true },
};

class UIOctaver : public NullKnobsUI
{
public:
    UIOctaver()
        : NullKnobsUI({ "OCTAVER", "ESSENTIALS", 480, 120, nullptr, 1, nulloctaver::kSliders, nulloctaver::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nulloctaver::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<OctaverBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    // as the @gfx of octaver_gfx.jsfx: the output waveform and the tracked pitch on the left,
    // levels per voice on the right
    void drawDisplay() override
    {
        setPlot(28.0f, 66.0f, 226.0f, 74.0f);
        grid(8, 2, true);
        scope(1.0f);
        label(28.0f, 155.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, "TRACKING", true);

        const nulloctaver::View* v = view();
        auto get = [&](const std::atomic<float>& a) { return v ? a.load(std::memory_order_relaxed) : 0.0f; };
        const float hz = v ? get(v->hz) : 0.0f;
        char buf[48];
        if (hz > 0.0f)
        {
            static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            const int n = int(std::floor(12.0 * std::log(double(hz) / 440.0) / std::log(2.0) + 69.5));
            const int k = n - 12 * int(std::floor(double(n) / 12.0));
            std::snprintf(buf, sizeof(buf), "%.1f Hz   %s%d", double(hz), kNames[k], int(std::floor(double(n) / 12.0)) - 1);
        }
        else
            std::snprintf(buf, sizeof(buf), "--");
        label(92.0f, 155.0f, ALIGN_LEFT, 8.5f, 0.0f, theme::kText, buf);

        using nulljsfx::toDb;
        meter(278.0f, 70.0f, 174.0f, 8.0f, "IN", v ? toDb(get(v->in)) : -140.0f, -140.0f, -60.0f);
        meter(278.0f, 89.0f, 174.0f, 8.0f, "-1", v ? toDb(get(v->oct1)) : -140.0f, -140.0f, -60.0f);
        meter(278.0f, 108.0f, 174.0f, 8.0f, "-2", v ? toDb(get(v->oct2)) : -140.0f, -140.0f, -60.0f);
        meter(278.0f, 127.0f, 174.0f, 8.0f, "UP", v ? toDb(get(v->up)) : -140.0f, -140.0f, -60.0f);
        meter(278.0f, 146.0f, 174.0f, 8.0f, "OUT", v ? toDb(get(v->out)) : -140.0f, -140.0f, -60.0f);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4 && v >= 1000.0f)
            std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIOctaver();
}

END_NAMESPACE_DISTRHO
