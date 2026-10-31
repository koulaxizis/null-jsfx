/*
 * NULL JSFX - Drum Trigger (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "DrumTriggerDSP.hpp"
#include "DrumTriggerSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in drum_trigger_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "NOTE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RETRIGGER", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DYNAMICS", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "DET HPF", 1, -1, false, true },
    { 5, NullKnobsUI::kKnob, "DET LPF", 1, -1, false, true },
    { 6, NullKnobsUI::kKnob, "LENGTH", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "CHANNEL", 1, -1, false },
};

static const char* const kNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class UIDrumTrigger : public NullKnobsUI
{
public:
    UIDrumTrigger()
        : NullKnobsUI({ "DRUM TRIGGER", "MIDI", 480, 120, nullptr, 2, nulldrumtrigger::kUiSliders,
                        nulldrumtrigger::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in drum_trigger_gfx.jsfx: the detector level with Threshold marked, a pad that lights
    // while the note sounds, the note, the last velocity and the hits so far
    void drawDisplay() override
    {
        float lv = 0.0f, vel = 0.0f, hits = 0.0f, on = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            lv = t->inLevel.load(std::memory_order_relaxed);
            vel = t->gr.load(std::memory_order_relaxed);
            hits = t->extra.load(std::memory_order_relaxed);
            on = t->aux[0].load(std::memory_order_relaxed);
        }
        const float thr = float(nulldrumtrigger::sliderValue(1, value(1)));
        meter(40.0f, 82.0f, 300.0f, 12.0f, "DET", nulljsfx::toDb(lv), thr, -60.0f);
        beginPath();
        roundedRect(372.0f, 70.0f, 76.0f, 76.0f, 6.0f);
        fillColor(col(on > 0.5f ? theme::kAccent : theme::kBorder, on > 0.5f ? 0.25f + 0.75f * vel / 127.0f : 1.0f));
        fill();
        const int note = int(std::lround(nulldrumtrigger::sliderValue(0, value(0))));
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s%d", kNotes[note % 12], note / 12 - 1);
        label(410.0f, 108.0f, ALIGN_CENTER, 12.0f, 1.0f, theme::kText, buf, true);
        std::snprintf(buf, sizeof(buf), "NOTE %d   CH %d", note, int(std::lround(nulldrumtrigger::sliderValue(7, value(7)))) + 1);
        label(40.0f, 118.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        std::snprintf(buf, sizeof(buf), "VELOCITY %d   HITS %d", int(vel), int(hits));
        label(40.0f, 136.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nulldrumtrigger::sliderValue(param, v);
        if (param == 0)
        {
            const int k = int(std::lround(x));
            std::snprintf(buf, n, "%s%d", kNotes[k % 12], k / 12 - 1);
        }
        else if (param == 1)
            std::snprintf(buf, n, "%.1f dB", x);
        else if (param == 2 || param == 6)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param == 3)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 4 || param == 5)
            std::snprintf(buf, n, x >= 1000.0 ? "%.1f kHz" : "%.0f Hz", x >= 1000.0 ? x / 1000.0 : x);
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIDrumTrigger();
}

END_NAMESPACE_DISTRHO
