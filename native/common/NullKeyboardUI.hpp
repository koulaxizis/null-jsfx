/*
 * NULL JSFX - the keyboard and the chips of the MIDI effects' displays (Chords, Scale): the
 * twins of kb_draw and the chip rows in chords_gfx.jsfx and scale_gfx.jsfx
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullUI.hpp"

START_NAMESPACE_DISTRHO

class NullKeyboardUI : public NullKnobsUI
{
public:
    explicit NullKeyboardUI(const Layout& l) : NullKnobsUI(l) {}

protected:
    // per note 0..127, filled by the display before drawKeyboard
    bool fSounding[128] = {}, fHeld[128] = {}, fPreview[128] = {};
    // per semitone from the tonic: in the scale shown
    bool fInScale[12] = {};
    // per note: a label on the white key (Scale's White Keys mode), or nullptr
    const char* fKeyLabel[128] = {};

    static bool isWhite(int pc) { return pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11; }

    // the notes sounding and the keys held, from the plugin (aux[0..7], aux[8..15])
    void readNotes()
    {
        nulljsfx::Taps* t = taps();
        for (int n = 0; n < 128; ++n)
        {
            fSounding[n] = t && ((uint32_t(t->aux[n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u);
            fHeld[n] = t && ((uint32_t(t->aux[8 + n / 16].load(std::memory_order_relaxed)) >> (n % 16)) & 1u);
        }
    }

    void box(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb));
        fill();
    }

    void dot(float x, float y, float r, uint32_t rgb)
    {
        beginPath();
        circle(x, y, r);
        fillColor(col(rgb));
        fill();
    }

    // a keyboard from note lo to hi: scale notes tinted, the tonic underlined, sounding notes
    // silver, held keys marked on top, the preview notes dotted
    void drawKeyboard(float x, float y, float w, float h, int lo, int hi, int tonic)
    {
        int nw = 0;
        for (int n = lo; n <= hi; ++n)
            nw += isWhite(n % 12);
        const float ww = w / float(nw), bw = ww * 0.62f, bh = h * 0.6f;
        int i = 0;
        for (int n = lo; n <= hi; ++n)
        {
            const int pc = n % 12;
            if (!isWhite(pc))
                continue;
            const float xx = x + float(i) * ww;
            const bool in = fInScale[(pc - tonic + 12) % 12], on = fSounding[n];
            box(xx + 0.5f, y, ww - 1.0f, h, on ? theme::kAccent : in ? 0x55534f : 0x2d2b28);
            if (pc == tonic)
                box(xx + 2.5f, y + h - 4.0f, ww - 5.0f, 2.0f, on ? theme::kBackground : theme::kMuted);
            if (fKeyLabel[n])
                label(xx + ww * 0.5f, y + h - 13.0f, ALIGN_CENTER, 7.0f, 0.0f, on ? theme::kBackground : theme::kText, fKeyLabel[n], true);
            if (fPreview[n])
                dot(xx + ww * 0.5f, y + h - 13.0f, 2.6f, on ? theme::kBackground : theme::kAccent);
            if (fHeld[n])
                box(xx + 2.5f, y + 2.0f, ww - 5.0f, 3.0f, on ? theme::kBackground : theme::kText);
            ++i;
        }
        i = 0;
        for (int n = lo; n <= hi; ++n)
        {
            const int pc = n % 12;
            if (isWhite(pc))
            {
                ++i;
                continue;
            }
            const float xx = x + float(i) * ww - bw * 0.5f;
            const bool in = fInScale[(pc - tonic + 12) % 12], on = fSounding[n];
            box(xx - 1.0f, y, bw + 2.0f, bh + 1.0f, theme::kCard);
            box(xx, y, bw, bh, on ? theme::kAccent : in ? 0x3e3c39 : 0x131211);
            if (pc == tonic)
                box(xx + 1.5f, y + bh - 4.0f, bw - 3.0f, 2.0f, on ? theme::kBackground : theme::kMuted);
            if (fPreview[n])
                dot(xx + bw * 0.5f, y + bh - 11.0f, 2.2f, on ? theme::kBackground : theme::kAccent);
            if (fHeld[n])
                box(xx + 1.5f, y + 2.0f, bw - 3.0f, 3.0f, on ? theme::kBackground : theme::kText);
        }
    }

    // a chip (28 x 18) with a short text, lit or not
    void chip(float x, float y, const char* text, bool lit)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, 27.0f, 17.0f, 4.0f);
        fillColor(col(lit ? theme::kAccent : theme::kBackground));
        fill();
        strokeColor(col(lit ? theme::kAccent : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x + 14.0f, y + 9.0f, ALIGN_CENTER, 7.5f, 0.0f, lit ? theme::kBackground : theme::kText, text, true);
    }
};

END_NAMESPACE_DISTRHO
