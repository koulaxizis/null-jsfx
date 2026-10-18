/*
 * NULL JSFX - Arpeggiator (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullKeyboardUI.hpp"
#include "ArpeggiatorSliders.hpp"

START_NAMESPACE_DISTRHO

// as arpeggiator_gfx.jsfx: rate, pattern, octaves, gate, swing, latch / steps, seed
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "RATE", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "PATTERN", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "OCTAVES", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "GATE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SWING", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "LATCH", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "STEPS", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "SEED", 1, -1, false },
};

class UIArpeggiator : public NullKeyboardUI
{
public:
    UIArpeggiator()
        : NullKeyboardUI({ "ARPEGGIATOR", "MIDI", 480, 170, nullptr, 2, nullarp::kUiSliders, nullarp::kNumSliders,
                           kControls, sizeof(kControls) / sizeof(kControls[0]), true })
    {
        for (int i = 0; i < 12; ++i)
            fInScale[i] = true;
    }

protected:
    // the velocity lane (design units), as in arpeggiator_gfx.jsfx
    static constexpr float kLaneX = 24.0f, kLaneY = 80.0f, kCellW = 27.0f, kLaneH = 64.0f;

    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    // as in arpeggiator_gfx.jsfx: the pattern, rate and octaves with the tempo and whether it
    // follows the host; the velocity lane (drag over the bars to draw it, wheel over a bar for
    // 5 % steps), the step playing lit, the steps past Steps dimmed; a keyboard C2..C7 with the
    // notes in the arp marked on top and the notes sounding silver
    void drawDisplay() override
    {
        static const char* const pats[] = { "UP", "DOWN", "UP-DOWN", "DOWN-UP", "PLAYED", "RANDOM", "CHORD" };
        int cur = -1;
        double bpm = 120.0;
        bool sync = false;
        if (nulljsfx::Taps* t = taps())
        {
            cur = int(std::lround(t->extra.load(std::memory_order_relaxed))) - 1;
            const float tp = t->gr.load(std::memory_order_relaxed);
            if (tp > 0.0f)
                bpm = tp;
            sync = t->inLevel.load(std::memory_order_relaxed) > 0.5f;
        }
        readNotes();
        const int n = std::max(1, std::min(16, iv(6)));
        const int pat = std::max(0, std::min(6, iv(1))), rate = std::max(0, std::min(9, iv(0)));

        char l[64], r[32];
        std::snprintf(l, sizeof(l), "%s  %s  %d OCT%s", pats[pat], nullarp::kRateOptions[rate], iv(2), iv(5) ? "  LATCH" : "");
        std::snprintf(r, sizeof(r), "%d BPM  %s", int(std::floor(bpm + 0.5)), sync ? "SYNC" : "FREE");
        label(24.0f, 70.0f, ALIGN_LEFT, 7.5f, 1.2f, theme::kMuted, l, true);
        label(456.0f, 70.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, r, true);

        char num[8];
        for (int k = 0; k < 16; ++k)
        {
            const float x = kLaneX + float(k) * kCellW;
            const bool in = k < n;
            box(x + 1.0f, kLaneY, 25.0f, kLaneH, k == cur ? theme::kBorder : theme::kBackground);
            const float h = kLaneH * float(iv(uint32_t(8 + k))) / 100.0f;
            if (h > 0.0f)
            {
                beginPath();
                rect(x + 3.0f, kLaneY + kLaneH - h, 21.0f, h);
                fillColor(col(theme::kAccent, k == cur ? 1.0f : in ? 0.55f : 0.15f));
                fill();
            }
            std::snprintf(num, sizeof(num), "%d", k + 1);
            fontSize(7.0f * kNotoLineToEm);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fillColor(col(k == cur ? theme::kText : theme::kMuted, in ? 1.0f : 0.5f));
            text(x + 13.5f, 151.0f, num, nullptr);
        }

        drawKeyboard(24.0f, 160.0f, 432.0f, 52.0f, 36, 96, -1);
    }

    // the lane step under (x, y) in design units, or -1
    int laneAt(float x, float y) const
    {
        if (x < kLaneX || x >= kLaneX + 16.0f * kCellW || y < kLaneY || y >= kLaneY + kLaneH)
            return -1;
        return std::max(0, std::min(15, int(std::floor((x - kLaneX) / kCellW))));
    }

    void drawLane(float x, float y)
    {
        const int k = std::max(0, std::min(15, int(std::floor((x - kLaneX) / kCellW))));
        const float v = std::max(0.0f, std::min(100.0f, std::floor(100.0f * (kLaneY + kLaneH - y) / kLaneH + 0.5f)));
        if (v != value(uint32_t(8 + k)))
            setGesture(uint32_t(8 + k), v);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == kMouseButtonLeft && ev.press && laneAt(x, y) >= 0)
        {
            fLaneDrag = true;
            drawLane(x, y);
            return true;
        }
        if (ev.button == kMouseButtonLeft && !ev.press && fLaneDrag)
        {
            fLaneDrag = false;
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (fLaneDrag)
        {
            const float s = uiScale();
            drawLane(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
            return true;
        }
        return NullKnobsUI::onMotion(ev);
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        const int k = laneAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (!presetOpen() && k >= 0 && ev.delta.getY() != 0.0f)
        {
            const float v = value(uint32_t(8 + k));
            setGesture(uint32_t(8 + k), std::max(0.0f, std::min(100.0f, v + (ev.delta.getY() > 0.0f ? 5.0f : -5.0f))));
            return true;
        }
        return NullKnobsUI::onScroll(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2 || param == 6 || param == 7)
            std::snprintf(buf, n, "%d", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    bool fLaneDrag = false;
};

UI* createUI()
{
    return new UIArpeggiator();
}

END_NAMESPACE_DISTRHO
