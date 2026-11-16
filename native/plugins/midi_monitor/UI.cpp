/*
 * NULL JSFX - MIDI Monitor (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "MidiMonitorPlugin.hpp"

START_NAMESPACE_DISTRHO

// one row, as in midi_monitor_gfx.jsfx
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "CHANNEL", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SHOW", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "FREEZE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "MIDDLE C", 0, -1, false },
};

class UIMidiMonitor : public NullKnobsUI
{
public:
    UIMidiMonitor()
        : NullKnobsUI({ "MIDI MONITOR", "MIDI", 480, 260, nullptr, 1, nullmidimonitor::kUiSliders,
                        nullmidimonitor::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nullmidimonitor::Monitor* mon() const
    {
        if (auto* p = static_cast<NullPluginBase*>(getPluginInstancePointer()))
            return &static_cast<PluginMidiMonitor*>(p)->monitor();
        return nullptr;
    }

    // as in midi_monitor_gfx.jsfx: the messages that passed, a keyboard of the held notes, every
    // channel's notes, bend and pressure, and the latest messages Channel and Show let through
    void drawDisplay() override
    {
        const nullmidimonitor::Monitor* m = mon();
        const nullmidimonitor::Params p = nullmidimonitor::paramsFrom([this](uint32_t i) { return nullmidimonitor::sliderValue(i, value(i)); });
        const float x0 = fVX + 8.0f, w = fVW - 16.0f;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "MESSAGES %u", m ? m->seen() : 0u);
        label(x0, fVY + 8.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kAccent, buf, true);
        int held = 0;
        if (m)
            for (int ch = 0; ch < 16; ++ch)
                if (p.channel == 0 || ch == p.channel - 1)
                    for (int n = 0; n < 128; ++n)
                        held += m->held(ch, n) > 0;
        std::snprintf(buf, sizeof(buf), p.freeze ? "FROZEN   HELD %d" : "HELD %d", held);
        label(x0 + w, fVY + 8.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, buf, true);

        // the keyboard: 128 keys, the held ones lit by their velocity
        const float ky = fVY + 20.0f, kh = 28.0f, kw = w / 128.0f;
        beginPath();
        rect(x0, ky, w, kh);
        fillColor(col(theme::kMuted, 0.35f));
        fill();
        for (int n = 0; n < 128; ++n)
        {
            const int pc = n % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            int v = 0;
            if (m)
                for (int ch = 0; ch < 16; ++ch)
                    if (p.channel == 0 || ch == p.channel - 1)
                        v = std::max(v, m->held(ch, n));
            // a white key's edge: under the middle of a black key, or between E and F, B and C
            if (black || pc == 5 || pc == 0)
            {
                beginPath();
                rect(x0 + (n + (black ? 0.5f : 0.0f)) * kw, ky, 0.5f, kh);
                fillColor(col(theme::kBackground));
                fill();
            }
            if (v > 0 || black)
            {
                beginPath();
                rect(x0 + n * kw, ky, std::fmax(1.0f, kw - 0.5f), black ? kh * 0.62f : kh);
                fillColor(v > 0 ? col(theme::kAccent, 0.35f + 0.65f * float(v) / 127.0f) : col(theme::kBackground));
                fill();
            }
            if (pc == 0)
            {
                nullmidimonitor::noteName(n, p.middleC, buf, sizeof(buf));
                label(x0 + n * kw, ky + kh + 6.0f, ALIGN_LEFT, 6.0f, 0.0f, theme::kSlogan, buf);
            }
        }

        // the channels: held notes (the frame), pitch bend (from the middle) and pressure (below)
        const float cy = ky + kh + 16.0f, ch = 22.0f, cw = w / 16.0f;
        for (int c = 0; c < 16; ++c)
        {
            const float cx = x0 + c * cw;
            bool any = false;
            if (m)
                for (int n = 0; n < 128 && !any; ++n)
                    any = m->held(c, n) > 0;
            const bool dim = p.channel > 0 && c != p.channel - 1;
            beginPath();
            rect(cx + 1.0f, cy, cw - 2.0f, ch);
            fillColor(col(any ? theme::kAccent : theme::kBorder, dim ? 0.15f : (any ? 0.3f : 0.5f)));
            fill();
            std::snprintf(buf, sizeof(buf), "%d", c + 1);
            label(cx + cw * 0.5f, cy + 6.0f, ALIGN_CENTER, 6.0f, 0.0f, dim ? theme::kSlogan : theme::kMuted, buf);
            const float b = m ? m->bend(c) : 0.0f, pr = m ? m->pressure(c) : 0.0f;
            const float mid = cx + cw * 0.5f, half = (cw - 6.0f) * 0.5f;
            beginPath();
            rect(b < 0.0f ? mid + b * half : mid, cy + 13.0f, std::fmax(1.0f, std::fabs(b) * half), 3.0f);
            fillColor(col(theme::kText, dim ? 0.3f : 0.9f));
            fill();
            if (pr > 0.0f)
            {
                beginPath();
                rect(cx + 3.0f, cy + 18.0f, (cw - 6.0f) * pr, 2.0f);
                fillColor(col(theme::kMuted, dim ? 0.3f : 0.9f));
                fill();
            }
        }

        // the latest messages, newest first
        const float ly = cy + ch + 12.0f, lh = 12.0f;
        const int rows = int((fVY + fVH - ly) / lh);
        int shown = 0;
        if (m)
        {
            const uint32_t n = m->logged();
            const uint32_t first = n > nullmidimonitor::Monitor::kLog ? n - nullmidimonitor::Monitor::kLog : 0;
            for (uint32_t i = n; i > first && shown < rows; --i)
            {
                const nullmidimonitor::Monitor::Entry e = m->entry(i - 1);
                if (!nullmidimonitor::shown(p, e.a))
                    continue;
                char kind[24], data[40];
                nullmidimonitor::describe(e.a, e.b, e.c, p.middleC, kind, sizeof(kind), data, sizeof(data));
                const float y = ly + shown * lh;
                const uint32_t tc = shown == 0 ? theme::kText : theme::kMuted;
                std::snprintf(buf, sizeof(buf), "%.3f", e.t);
                label(x0 + 52.0f, y, ALIGN_RIGHT, 7.5f, 0.0f, theme::kSlogan, buf);
                std::snprintf(buf, sizeof(buf), "CH %d", (e.a & 15) + 1);
                label(x0 + 62.0f, y, ALIGN_LEFT, 7.5f, 0.0f, tc, buf);
                label(x0 + 104.0f, y, ALIGN_LEFT, 7.5f, 0.0f, tc, kind);
                label(x0 + 196.0f, y, ALIGN_LEFT, 7.5f, 0.0f, tc, data);
                ++shown;
            }
        }
        if (!shown)
            label(x0 + w * 0.5f, ly + 30.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kSlogan, "NO MIDI YET", true);
    }
};

UI* createUI()
{
    return new UIMidiMonitor();
}

END_NAMESPACE_DISTRHO
