/*
 * NULL JSFX - Pitch to MIDI (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "PitchToMidiDSP.hpp"
#include "PitchToMidiSliders.hpp"

START_NAMESPACE_DISTRHO

// two rows, as in pitch_to_midi_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "LOW NOTE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "HIGH NOTE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "STABILITY", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "VELOCITY", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "FIXED VEL", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "TRANSPOSE", 1, -1, true },
    { 7, NullKnobsUI::kSelect, "BEND", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "CHANNEL", 1, -1, false },
};

static const char* const kNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class UIPitchToMidi : public NullKnobsUI
{
public:
    UIPitchToMidi()
        : NullKnobsUI({ "PITCH TO MIDI", "MIDI", 480, 120, nullptr, 2, nullpitchtomidi::kUiSliders,
                        nullpitchtomidi::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in pitch_to_midi_gfx.jsfx: the level the detector hears with Threshold marked, the pitch
    // it finds (note and cents, dim when unpitched), and the note it sends (lit while it sounds)
    void drawDisplay() override
    {
        float lv = -120.0f, f = -1.0f, sent = -1.0f;
        if (nulljsfx::Taps* t = taps())
        {
            lv = t->inLevel.load(std::memory_order_relaxed);
            f = t->gr.load(std::memory_order_relaxed);
            sent = t->extra.load(std::memory_order_relaxed);
        }
        const nullpitchtomidi::Params p
            = nullpitchtomidi::paramsFrom([this](uint32_t i) { return nullpitchtomidi::sliderValue(i, value(i)); });
        meter(40.0f, 82.0f, 260.0f, 12.0f, "IN", lv, float(p.threshold), -70.0f);
        char buf[64];
        if (f >= 0.0f)
        {
            const int n = int(std::floor(f + 0.5f));
            const int c = int(std::floor((f - float(n)) * 100.0f + 0.5f));
            std::snprintf(buf, sizeof(buf), "PITCH %s%d %+d CENTS", kNotes[n % 12], n / 12 - 1, c);
            label(40.0f, 118.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kText, buf, true);
        }
        else
            label(40.0f, 118.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, "PITCH --", true);
        std::snprintf(buf, sizeof(buf), "RANGE %d-%d   CH %d", int(std::fmin(p.lowNote, p.highNote)),
                      int(std::fmax(p.lowNote, p.highNote)), p.channel + 1);
        label(40.0f, 136.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, buf, true);
        const bool on = sent >= 0.0f;
        beginPath();
        roundedRect(340.5f, 70.5f, 107.0f, 75.0f, 6.0f);
        strokeColor(col(on ? theme::kAccent : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        if (on)
        {
            const int k = int(sent);
            std::snprintf(buf, sizeof(buf), "%s%d", kNotes[k % 12], k / 12 - 1);
        }
        else
            std::snprintf(buf, sizeof(buf), "--");
        label(394.0f, 102.0f, ALIGN_CENTER, 12.0f, 1.0f, on ? theme::kText : theme::kMuted, buf, true);
        label(394.0f, 132.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted, "NOTE OUT", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullpitchtomidi::sliderValue(param, v);
        if (param == 0)
            std::snprintf(buf, n, "%.1f dB", x);
        else if (param == 1 || param == 2 || param == 5)
            std::snprintf(buf, n, "%d", int(x));
        else if (param == 3)
            std::snprintf(buf, n, "%d ms", int(x));
        else if (param == 6)
            std::snprintf(buf, n, "%+d st", int(std::lround(x)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIPitchToMidi();
}

END_NAMESPACE_DISTRHO
