/*
 * NULL JSFX - Organ (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As organ_gfx.jsfx: the nine drawbars in the display (click or drag one down to pull it
 * out, the wheel steps it), the rotary speaker's horn and drum turning, the expression and
 * the voices sounding. Two rows of controls below.
 */

#include "NullUI.hpp"
#include "OrganSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 9, NullKnobsUI::kSelect, "PERCUSSION", 0, -1, false },
    { 10, NullKnobsUI::kSelect, "HARMONIC", 0, -1, false },
    { 11, NullKnobsUI::kSelect, "PERC DECAY", 0, -1, false },
    { 12, NullKnobsUI::kSelect, "PERC VOLUME", 0, -1, false },
    { 13, NullKnobsUI::kKnob, "KEY CLICK", 0, -1, false },
    { 14, NullKnobsUI::kSelect, "VIBRATO", 0, -1, false },
    { 15, NullKnobsUI::kKnob, "DRIVE", 1, -1, false },
    { 16, NullKnobsUI::kSelect, "ROTARY", 1, -1, false },
    { 17, NullKnobsUI::kKnob, "ACCELERATION", 1, -1, false },
    { 18, NullKnobsUI::kKnob, "HORN/DRUM", 1, -1, true },
    { 19, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullorgan::kNumParams];
    for (uint32_t i = 0; i < nullorgan::kNumParams; ++i)
        s[i] = nullorgan::kSliders[i];
    s[9].options = nullorgan::kOnShort;
    s[10].options = nullorgan::kHarmShort;
    s[11].options = nullorgan::kDecayShort;
    s[12].options = nullorgan::kPercVolShort;
    s[14].options = nullorgan::kVibShort;
    s[16].options = nullorgan::kRotShort;
    return s;
}

static const char* const kFeet[] = { "16'", "5 1/3'", "8'", "4'", "2 2/3'", "2'", "1 3/5'", "1 1/3'", "1'" };
// 0: the sub drawbars, 1: white (octaves of the fundamental), 2: black (the other harmonics)
static const int kKind[] = { 0, 0, 1, 1, 2, 1, 2, 2, 1 };

class UIOrgan : public NullKnobsUI
{
public:
    UIOrgan()
        : NullKnobsUI({ "ORGAN", "INSTRUMENTS", 560, 170, nullptr, 2, uiSliders(), nullorgan::kNumParams, kControls, kNumControls, true })
    {
    }

protected:
    static float barX(int d) { return 32.0f + float(d) * 38.0f; }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    void rotor(float cx, float cy, float r, float ph, bool horn)
    {
        beginPath();
        circle(cx, cy, r);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        const float a = 2.0f * 3.14159265f * ph;
        const float dx = std::cos(a), dy = std::sin(a);
        beginPath();
        if (horn)
        {
            moveTo(cx - dx * (r - 4.0f), cy - dy * (r - 4.0f));
            lineTo(cx + dx * (r - 4.0f), cy + dy * (r - 4.0f));
        }
        else
        {
            moveTo(cx, cy);
            lineTo(cx + dx * (r - 4.0f), cy + dy * (r - 4.0f));
        }
        strokeColor(col(theme::kAccent));
        strokeWidth(2.0f);
        stroke();
        beginPath();
        circle(cx + dx * (r - 4.0f), cy + dy * (r - 4.0f), 3.0f);
        fillColor(col(theme::kAccent));
        fill();
    }

    void drawDisplay() override
    {
        char buf[32];
        // the drawbars: the bar pulled out from the top, its handle showing the value
        for (int d = 0; d < 9; ++d)
        {
            const float x = barX(d);
            const int v = int(std::lround(value(uint32_t(d))));
            label(x + 14.0f, 70.0f, ALIGN_CENTER, 6.5f, 0.0f, theme::kMuted, kFeet[d], true);
            box(x + 12.0f, 78.0f, 4.0f, 120.0f, theme::kBackground);
            const uint32_t c = kKind[d] == 0 ? 0x6b6762 : kKind[d] == 1 ? theme::kAccent : 0x131211;
            const float hy = 78.0f + float(v) * 13.0f;
            if (v > 0)
                box(x + 4.0f, 78.0f, 20.0f, hy - 78.0f, c, 0.35f);
            beginPath();
            roundedRect(x + 0.5f, hy + 0.5f, 27.0f, 15.0f, 3.0f);
            fillColor(col(c));
            fill();
            strokeColor(col(fHover == d || fDrag == d ? theme::kTrackHover : kKind[d] == 2 ? 0x45433f : c));
            strokeWidth(1.0f);
            stroke();
            std::snprintf(buf, sizeof(buf), "%d", v);
            label(x + 14.0f, hy + 8.0f, ALIGN_CENTER, 8.0f, 0.0f, kKind[d] == 1 ? theme::kBackground : theme::kText, buf, true);
        }

        // the rotary speaker
        float hornPh = 0.0f, drumPh = 0.25f, hornF = 0.8f, drumF = 0.7f, ex = 1.0f, rot = float(std::lround(value(16)));
        int voices = 0;
        if (nulljsfx::Taps* t = taps())
        {
            hornPh = t->aux[0].load(std::memory_order_relaxed);
            drumPh = t->aux[1].load(std::memory_order_relaxed);
            hornF = t->aux[2].load(std::memory_order_relaxed);
            drumF = t->aux[3].load(std::memory_order_relaxed);
            ex = t->aux[4].load(std::memory_order_relaxed);
            rot = t->aux[5].load(std::memory_order_relaxed);
            voices = int(std::lround(t->extra.load(std::memory_order_relaxed)));
        }
        const int r = std::max(0, std::min(3, int(std::lround(rot))));
        static const char* const kRot[] = { "OFF", "SLOW", "FAST", "STOP" };
        label(392.0f, 70.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, "ROTARY", true);
        label(536.0f, 70.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kText, kRot[r], true);
        rotor(426.0f, 108.0f, 22.0f, hornPh, true);
        rotor(502.0f, 108.0f, 22.0f, drumPh, false);
        std::snprintf(buf, sizeof(buf), "HORN %.1f HZ", double(hornF));
        label(426.0f, 141.0f, ALIGN_CENTER, 6.5f, 0.5f, theme::kMuted, buf, true);
        std::snprintf(buf, sizeof(buf), "DRUM %.1f HZ", double(drumF));
        label(502.0f, 141.0f, ALIGN_CENTER, 6.5f, 0.5f, theme::kMuted, buf, true);

        // expression and voices
        label(392.0f, 160.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, "EXPRESSION", true);
        std::snprintf(buf, sizeof(buf), "%d%%", int(std::lround(ex * 100.0f)));
        label(536.0f, 160.0f, ALIGN_RIGHT, 7.0f, 0.0f, theme::kText, buf, true);
        box(392.0f, 168.0f, 144.0f, 5.0f, theme::kBackground);
        box(392.0f, 168.0f, 144.0f * std::max(0.0f, std::min(1.0f, ex)), 5.0f, theme::kAccent);
        label(392.0f, 190.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, "VOICES", true);
        std::snprintf(buf, sizeof(buf), "%d / 16", voices);
        label(536.0f, 190.0f, ALIGN_RIGHT, 7.0f, 0.0f, theme::kText, buf, true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 19)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 18)
        {
            const int b = int(std::lround(v));
            if (b == 50)
                std::snprintf(buf, n, "EVEN");
            else
                std::snprintf(buf, n, b > 50 ? "HORN %d" : "DRUM %d", b > 50 ? (b - 50) * 2 : (50 - b) * 2);
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    int barAt(float x, float y) const
    {
        if (y < 76.0f || y >= 200.0f)
            return -1;
        for (int d = 0; d < 9; ++d)
            if (x >= barX(d) - 4.0f && x < barX(d) + 32.0f)
                return d;
        return -1;
    }

    void setBar(int d, float y)
    {
        const float v = std::max(0.0f, std::min(8.0f, std::round((y - 86.0f) / 13.0f)));
        if (v != value(uint32_t(d)))
            setFromUI(uint32_t(d), v);
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetOpen() || ev.button != kMouseButtonLeft)
            return NullKnobsUI::onMouse(ev);
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (!ev.press)
        {
            if (fDrag >= 0)
            {
                editParameter(uint32_t(fDrag), false);
                fDrag = -1;
                repaint();
                return true;
            }
            return NullKnobsUI::onMouse(ev);
        }
        const int d = barAt(x, y);
        if (d < 0)
            return NullKnobsUI::onMouse(ev);
        fDrag = d;
        editParameter(uint32_t(d), true);
        setBar(d, y);
        repaint();
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (fDrag >= 0)
        {
            setBar(fDrag, y);
            return true;
        }
        const int h = presetOpen() ? -1 : barAt(x, y);
        if (h != fHover)
        {
            fHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        const int d = presetOpen() ? -1 : barAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (d < 0)
            return NullKnobsUI::onScroll(ev);
        const float dy = float(ev.delta.getY()) + float(ev.delta.getX());
        if (dy == 0.0f)
            return false;
        // the wheel up pushes the drawbar in (less), down pulls it out (more)
        const float v = std::max(0.0f, std::min(8.0f, value(uint32_t(d)) + (dy > 0.0f ? -1.0f : 1.0f)));
        setGesture(uint32_t(d), v);
        return true;
    }

private:
    int fHover = -1, fDrag = -1;
};

UI* createUI()
{
    return new UIOrgan();
}

END_NAMESPACE_DISTRHO
