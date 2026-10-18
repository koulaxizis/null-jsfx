/*
 * NULL JSFX - Bass (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As bass_gfx.jsfx: the display holds the phrase chips (a click chooses one, like the keys
 * C1 to G#1), the steps of the phrase with the step that plays, and a keyboard from C1 to B4
 * with the phrase keys, the keys held and the note playing. Three rows of knobs below.
 */

#include "NullUI.hpp"
#include "BassDSP.hpp"
#include "BassSliders.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "OSCILLATOR", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "OCTAVE", 0, -1, true },
    { 2, NullKnobsUI::kKnob, "DETUNE", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SUB", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "PUNCH", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "DRIVE", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "CUTOFF", 1, -1, false, true },
    { 7, NullKnobsUI::kKnob, "RESONANCE", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "ENV AMOUNT", 1, -1, true },
    { 9, NullKnobsUI::kKnob, "FILTER DECAY", 1, -1, false, true },
    { 16, NullKnobsUI::kKnob, "VELOCITY", 1, -1, false },
    { 18, NullKnobsUI::kKnob, "VOLUME", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "ATTACK", 2, -1, false, true },
    { 11, NullKnobsUI::kKnob, "DECAY", 2, -1, false, true },
    { 12, NullKnobsUI::kKnob, "SUSTAIN", 2, -1, false },
    { 13, NullKnobsUI::kKnob, "RELEASE", 2, -1, false, true },
    { 14, NullKnobsUI::kSelect, "GLIDE MODE", 2, -1, false },
    { 15, NullKnobsUI::kKnob, "GLIDE", 2, -1, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullbass::kNumParams];
    for (uint32_t i = 0; i < nullbass::kNumParams; ++i)
        s[i] = nullbass::kSliders[i];
    s[0].options = nullbass::kOscShort;
    s[14].options = nullbass::kGlideShort;
    s[17].options = nullbass::kPhraseShort;
    return s;
}

static const char* const kPc[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

class UIBass : public NullKnobsUI
{
public:
    UIBass()
        : NullKnobsUI({ "BASS", "INSTRUMENTS", 560, 156, nullptr, 3, uiSliders(), nullbass::kNumParams, kControls, kNumControls, true })
    {
    }

protected:
    static constexpr int kLo = 24, kHi = 71;
    static float chipX(int i) { return 24.0f + float(i) * (480.0f / 9.0f + 4.0f); }
    static constexpr float kChipW = 480.0f / 9.0f;

    static bool white(int pc) { return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11; }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    void outline(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f);
        strokeColor(col(rgb));
        strokeWidth(1.0f);
        stroke();
    }

    void drawDisplay() override
    {
        bool playing[128] = {}, held[128] = {};
        int sel = int(std::lround(value(17))), step = -1;
        if (nulljsfx::Taps* t = taps())
        {
            for (int n = 0; n < 128; ++n)
            {
                playing[n] = (uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
                held[n] = (uint32_t(t->aux[8 + n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u;
            }
            const int e = int(std::lround(t->extra.load(std::memory_order_relaxed)));
            sel = e / 100;
            step = e % 100 - 1;
        }
        sel = std::max(0, std::min(8, sel));

        // the phrase chips
        for (int i = 0; i < 9; ++i)
        {
            const float x = chipX(i);
            const bool lit = i == sel;
            beginPath();
            roundedRect(x + 0.5f, 64.5f, kChipW - 1.0f, 17.0f, 4.0f);
            fillColor(col(lit ? theme::kAccent : theme::kBackground));
            fill();
            strokeColor(col(lit ? theme::kAccent : fHoverChip == i ? theme::kTrackHover : theme::kBorder));
            strokeWidth(1.0f);
            stroke();
            label(x + kChipW * 0.5f, 73.0f, ALIGN_CENTER, 7.0f, 0.5f, lit ? theme::kBackground : theme::kText, nullbass::kPhraseShort[i], true);
        }

        // the steps of the phrase
        if (sel == 0)
        {
            box(24.0f, 88.0f, 512.0f, 42.0f, theme::kBackground);
            outline(24.0f, 88.0f, 512.0f, 42.0f, theme::kBorder);
            label(280.0f, 109.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted, "PHRASE OFF: THE KEYS FROM C2 UP PLAY THE BASS", true);
        }
        else
        {
            const nullbass::PhraseDef& d = nullbass::kPhrases[sel - 1];
            double last = 0.0;
            for (int i = 0; i < 16; ++i)
            {
                const float x = 24.0f + float(i) * 32.0f;
                box(x, 88.0f, 30.0f, 42.0f, theme::kBackground);
                const double iv = nullbass::ival(d.notes[i]);
                if (iv > -100)
                    last = iv;
                const float h = 8.0f + float(std::max(-12.0, std::min(12.0, last)) + 12.0) / 24.0f * 30.0f;
                if (iv > -100)
                {
                    box(x + 3.0f, 130.0f - h, 27.0f, h, theme::kAccent, d.mods[i] == 'a' ? 1.0f : 0.6f);
                    if (d.mods[i] == 's')
                        box(x + 3.0f, 90.0f, 8.0f, 2.0f, theme::kText);
                }
                else if (iv < -150)
                    box(x, 130.0f - h, 30.0f, h, theme::kAccent, 0.3f); // a tie holds the note before
                outline(x, 88.0f, 30.0f, 42.0f, i == step ? theme::kText : theme::kBorder);
                if (i == step)
                    outline(x + 1.0f, 89.0f, 28.0f, 40.0f, theme::kText);
                if (i % 4 == 0)
                {
                    char b[4];
                    std::snprintf(b, sizeof(b), "%d", i / 4 + 1);
                    label(x + 26.0f, 95.0f, ALIGN_RIGHT, 6.5f, 0.0f, theme::kMuted, b, true);
                }
            }
        }

        // the root (the last key held)
        int root = -1;
        for (int n = 36; n < 128; ++n)
            if (playing[n])
                root = n;
        char info[48];
        if (root >= 0)
            std::snprintf(info, sizeof(info), "NOTE %s%d", kPc[root % 12], root / 12 - 1);
        else
            std::snprintf(info, sizeof(info), "NOTE -");
        label(24.0f, 139.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kText, info, true);
        label(536.0f, 139.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, "C1 OFF  C#1-G#1 PHRASES  C2+ NOTES", true);

        // the keyboard: the phrase keys tinted, the active one dotted
        const float ky = 146.0f, kh = 52.0f;
        int nw = 0;
        for (int n = kLo; n <= kHi; ++n)
            nw += white(n % 12);
        const float ww = 512.0f / float(nw), bw = ww * 0.62f, bh = kh * 0.6f;
        int i = 0;
        for (int n = kLo; n <= kHi; ++n)
        {
            if (!white(n % 12))
                continue;
            const float x = 24.0f + float(i) * ww;
            const bool zone = n <= 32;
            box(x + 0.5f, ky, ww - 1.0f, kh, playing[n] ? theme::kAccent : zone ? 0x55534f : 0x2d2b28);
            if (held[n])
                box(x + 2.5f, ky + 2.0f, ww - 5.0f, 3.0f, playing[n] ? theme::kBackground : theme::kText);
            if (zone && n - 24 == sel)
            {
                beginPath();
                circle(x + ww * 0.5f, ky + kh - 13.0f, 2.6f);
                fillColor(col(theme::kAccent));
                fill();
            }
            if (n % 12 == 0)
            {
                char b[8];
                std::snprintf(b, sizeof(b), "C%d", n / 12 - 1);
                label(x + ww * 0.5f, ky + kh - 5.0f, ALIGN_CENTER, 6.0f, 0.0f, playing[n] ? theme::kBackground : theme::kMuted, b, true);
            }
            ++i;
        }
        i = 0;
        for (int n = kLo; n <= kHi; ++n)
        {
            if (white(n % 12))
            {
                ++i;
                continue;
            }
            const float x = 24.0f + float(i) * ww - bw * 0.5f;
            const bool zone = n <= 32;
            box(x - 1.0f, ky, bw + 2.0f, bh + 1.0f, theme::kCard);
            box(x, ky, bw, bh, playing[n] ? theme::kAccent : zone ? 0x3e3c39 : 0x131211);
            if (held[n])
                box(x + 1.5f, ky + 2.0f, bw - 3.0f, 3.0f, playing[n] ? theme::kBackground : theme::kText);
            if (zone && n - 24 == sel)
            {
                beginPath();
                circle(x + bw * 0.5f, ky + bh - 11.0f, 2.2f);
                fillColor(col(theme::kAccent));
                fill();
            }
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 18)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (param == 1)
            std::snprintf(buf, n, "%+d", int(std::lround(v)));
        else if (param == 2)
            std::snprintf(buf, n, "%d ct", int(std::lround(v)));
        else if (param == 6)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 8)
            std::snprintf(buf, n, "%+d%%", int(std::lround(v)));
        else if (param == 9 || (param >= 10 && param <= 13 && param != 12) || param == 15)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f s", v / 1000.0f);
            else
                std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    int chipAt(float x, float y) const
    {
        if (y < 64.0f || y >= 82.0f)
            return -1;
        for (int i = 0; i < 9; ++i)
            if (x >= chipX(i) && x < chipX(i) + kChipW)
                return i;
        return -1;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetOpen() || !ev.press || ev.button != kMouseButtonLeft)
            return NullKnobsUI::onMouse(ev);
        const float s = uiScale();
        const int c = chipAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (c < 0)
            return NullKnobsUI::onMouse(ev);
        setGesture(17, float(c));
        if (nulljsfx::Taps* t = taps())
            t->request.store(c + 1);
        repaint();
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const int c = presetOpen() ? -1 : chipAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (c != fHoverChip)
        {
            fHoverChip = c;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

private:
    int fHoverChip = -1;
};

UI* createUI()
{
    return new UIBass();
}

END_NAMESPACE_DISTRHO
