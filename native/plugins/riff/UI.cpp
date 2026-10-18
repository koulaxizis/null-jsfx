/*
 * NULL JSFX - Riff (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "RiffDSP.hpp"
#include "RiffSliders.hpp"

START_NAMESPACE_DISTRHO

// as riff_gfx.jsfx: key, scale, rate, steps, octaves, seed / density, length, variation,
// accent, velocity range, rhythm lock
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "KEY", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "SCALE", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "RATE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "STEPS", 0, -1, false },
    { 6, NullKnobsUI::kKnob, "OCTAVES", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SEED", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "DENSITY", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "LENGTH", 1, -1, false },
    { 11, NullKnobsUI::kKnob, "VARIATION", 1, -1, false },
    { 10, NullKnobsUI::kKnob, "ACCENT", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "VEL RANGE", 1, -1, false },
    { 8, NullKnobsUI::kKnob, "RHYTHM LOCK", 1, -1, false },
};

static const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kScaleNames[] = { "MAJOR", "MINOR", "HARMONIC MINOR", "MELODIC MINOR", "DORIAN", "PHRYGIAN",
                                           "LYDIAN", "MIXOLYDIAN", "LOCRIAN", "MAJOR PENTATONIC", "MINOR PENTATONIC",
                                           "BLUES" };
static const char* const kRateNames[] = { "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/32T" };

// the Rhythm Lock switch in the display (design units)
static constexpr float kLockX = 372.0f, kLockY = 65.0f, kLockW = 84.0f, kLockH = 18.0f;

class UIRiff : public NullKnobsUI
{
public:
    UIRiff()
        : NullKnobsUI({ "RIFF", "MIDI", 480, 160, nullptr, 2, nullriff::kUiSliders, nullriff::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    int iv(uint32_t p) const { return int(std::lround(value(p))); }

    nullriff::Params params() const
    {
        nullriff::Params p;
        p.key = iv(0); p.scale = iv(1); p.seed = iv(2); p.dens = iv(3) / 100.0; p.steps = iv(4); p.rate = iv(5);
        p.octs = iv(6); p.nlen = iv(7) / 100.0; p.lock = iv(8); p.vrng = iv(9) / 100.0; p.acc = iv(10) / 100.0;
        p.vari = iv(11) / 100.0;
        return p;
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    static uint32_t mix(uint32_t a, uint32_t b, float k)
    {
        uint32_t o = 0;
        for (int sh = 0; sh <= 16; sh += 8)
        {
            const float ca = float((a >> sh) & 255u), cb = float((b >> sh) & 255u);
            o |= uint32_t(std::lround(ca * (1.0f - k) + cb * k)) << sh;
        }
        return o;
    }

    // as in riff_gfx.jsfx: the key and scale, the leading key and the step (or PLAY A KEY), the
    // Rhythm Lock switch, and the pattern as a piano roll: a column per step (beats shaded), a
    // row per degree (the roots tinted), each note a bar as long as Note Length, brighter the
    // louder, the step playing lit
    void drawDisplay() override
    {
        using namespace nullriff;
        const Params p = params();
        int step = -1, root = -1, sounding = -1;
        if (nulljsfx::Taps* t = taps())
        {
            step = int(std::lround(t->extra.load(std::memory_order_relaxed))) - 1;
            root = int(std::lround(t->aux[0].load(std::memory_order_relaxed))) - 1;
            sounding = int(std::lround(t->aux[1].load(std::memory_order_relaxed))) - 1;
        }
        generate(p, fPat);

        char line[96];
        std::snprintf(line, sizeof(line), "%s %s", kNoteNames[p.key], kScaleNames[p.scale]);
        label(30.0f, 74.0f, ALIGN_LEFT, 9.0f, 1.2f, theme::kText, line, true);
        if (step >= 0 && root >= 0)
            std::snprintf(line, sizeof(line), "ROOT %s%d   STEP %d/%d", kNoteNames[root % 12], root / 12 - 1, step + 1, p.steps);
        else
            std::snprintf(line, sizeof(line), "%s   %d STEPS   PLAY A KEY", kRateNames[p.rate], p.steps);
        label(360.0f, 74.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, line, true);

        // the Rhythm Lock switch
        const bool locked = p.lock > 0;
        beginPath();
        roundedRect(kLockX + 0.5f, kLockY + 0.5f, kLockW - 1.0f, kLockH - 1.0f, 4.0f);
        fillColor(col(locked ? theme::kAccent : theme::kBackground));
        fill();
        strokeColor(col(locked ? theme::kAccent : fLockHover ? 0x45433f : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        if (locked)
            std::snprintf(line, sizeof(line), "LOCKED %d", p.lock);
        else
            std::snprintf(line, sizeof(line), "LOCK RHYTHM");
        label(kLockX + kLockW * 0.5f, kLockY + kLockH * 0.5f, ALIGN_CENTER, 7.0f, 0.8f,
              locked ? theme::kBackground : theme::kText, line, true);

        // the piano roll
        const float x = 24.0f, y = 90.0f, w = 432.0f, h = 110.0f;
        const int sz = kScales[p.scale][0], top = sz * p.octs, nr = top + 1;
        const float cw = w / float(p.steps), rh = h / float(nr);
        const int m = p.rate >= 3 ? 3 : 4;
        for (int i = 0; i < p.steps; ++i)
            box(x + float(i) * cw, y, cw, h, i == step ? 0x34322f : (i / m) % 2 ? 0x201f1d : theme::kBackground);
        for (int d = 0; d < nr; d += sz)
            box(x, y + float(top - d) * rh, w, rh, 0x2d2b28, 0.7f);
        for (int i = 1; i < p.steps; ++i)
        {
            const float xx = x + float(i) * cw;
            beginPath();
            moveTo(xx, y);
            lineTo(xx, y + h);
            strokeColor(col(i % m == 0 ? 0x3a3835 : theme::kBorder, i % m == 0 ? 1.0f : 0.35f));
            strokeWidth(1.0f / uiScale());
            stroke();
        }
        for (int i = 0; i < p.steps; ++i)
        {
            if (!fPat.on[i])
                continue;
            const int d = fPat.deg[i];
            const bool lit = i == step && sounding >= 0;
            const float k = fPat.acc[i] ? 1.0f : float(0.75 - 0.6 * p.vrng * fPat.vr[i]);
            box(x + float(i) * cw + 1.0f, y + float(top - d) * rh + 0.5f, std::max(2.0f, (cw - 2.0f) * float(p.nlen)),
                std::max(1.5f, rh - 1.0f), lit ? theme::kText : mix(0x4a4845, theme::kAccent, k));
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int i = int(std::lround(v));
        if (param == 6)
            std::snprintf(buf, n, "%d OCT", i);
        else if (param == 8 && i <= 0)
            std::snprintf(buf, n, "OFF");
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    bool inLock(float x, float y) const
    {
        return x >= kLockX && x < kLockX + kLockW && y >= kLockY && y < kLockY + kLockH;
    }

    // the Rhythm Lock switch: lock the current seed's rhythm, or unlock
    bool onMouse(const MouseEvent& ev) override
    {
        const float s = uiScale();
        if (!presetOpen() && ev.press && ev.button == 1 && inLock(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s))
        {
            setGesture(8, iv(8) > 0 ? 0.0f : float(iv(2)));
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const bool h = inLock(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (h != fLockHover)
        {
            fLockHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

private:
    nullriff::Pattern fPat;
    bool fLockHover = false;
};

UI* createUI()
{
    return new UIRiff();
}

END_NAMESPACE_DISTRHO
