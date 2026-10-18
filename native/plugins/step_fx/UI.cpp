/*
 * NULL JSFX - Step FX (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StepFxDSP.hpp"
#include "StepFxSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 16, NullKnobsUI::kSelect, "NOTE", 0, -1, false },
    { 17, NullKnobsUI::kKnob, "STEPS", 0, -1, false },
    { 18, NullKnobsUI::kKnob, "SWING", 0, -1, false },
    { 19, NullKnobsUI::kSelect, "REPEAT", 0, -1, false },
    { 20, NullKnobsUI::kKnob, "FILTER", 0, -1, false, true },
    { 21, NullKnobsUI::kKnob, "CRUSH", 0, -1, false },
    { 22, NullKnobsUI::kKnob, "PITCH", 1, -1, false },
    { 23, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 24, NullKnobsUI::kSelect, "MIDI", 1, -1, false },
};

class UIStepFx : public NullKnobsUI
{
public:
    UIStepFx()
        : NullKnobsUI({ "STEP FX", "ADVANCED", 480, 130, nullptr, 2, nullstepfx::kSliders, nullstepfx::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the step cells (design units), as in step_fx_gfx.jsfx: two rows of 8 under a status line
    static constexpr float kVX = 24.0f, kVY = 62.0f;
    float stepW() const { return (float(kWidth) - 48.0f - 7.0f * 4.0f) / 8.0f; }
    float cellX(int k) const { return kVX + float(k % 8) * (stepW() + 4.0f); }
    float cellY(int k) const { return kVY + 16.0f + float(k / 8) * 48.0f; }

    int cellAt(float x, float y) const
    {
        for (int k = 0; k < 16; ++k)
            if (x >= cellX(k) && x < cellX(k) + stepW() && y >= cellY(k) && y < cellY(k) + 44.0f)
                return k;
        return -1;
    }

    int effect(int k) const { return std::max(0, std::min(8, int(std::lround(value(uint32_t(k)))))); }

    // as in step_fx_gfx.jsfx: the 16 steps (click for the next effect, right-click for Off,
    // wheel to step through), the playing step marked, steps past Steps dimmed; above them the
    // playing step (or the effect the held MIDI notes pick) and the step length at the tempo
    void drawDisplay() override
    {
        int cur = 0, midiE = 0;
        double bpm = 120.0;
        if (nulljsfx::Taps* t = taps())
        {
            cur = int(std::lround(t->extra.load(std::memory_order_relaxed)));
            midiE = int(std::lround(t->aux[0].load(std::memory_order_relaxed)));
            const float tp = t->gr.load(std::memory_order_relaxed);
            if (tp > 0.0f)
                bpm = tp;
        }
        const bool midi = value(24) >= 0.5f;
        const int n = std::max(1, std::min(16, int(std::lround(value(17)))));
        if (midi)
            cur = -1;

        char l[48], r[48];
        if (midi)
        {
            static const char* const kNames[] = { "", "REPEAT", "REVERSE", "FILTER", "GATE", "TAPE STOP", "PITCH DOWN", "PITCH UP", "CRUSH" };
            if (midiE <= 0 || midiE > 8)
                std::snprintf(l, sizeof(l), "MIDI: NO NOTE");
            else
                std::snprintf(l, sizeof(l), "MIDI: %s", kNames[midiE]);
            std::snprintf(r, sizeof(r), "NOTES C3 TO G3");
        }
        else
        {
            std::snprintf(l, sizeof(l), "STEP %d OF %d", cur + 1, n);
            const int len = std::max(0, std::min(4, int(std::lround(value(16)))));
            std::snprintf(r, sizeof(r), "%s AT %d BPM", nullstepfx::kLengthOptions[len], int(std::floor(bpm + 0.5)));
        }
        label(kVX, kVY + 5.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, l, true);
        label(kVX + fVW, kVY + 5.0f, ALIGN_RIGHT, 7.5f, 1.0f, theme::kMuted, r, true);

        const float cw = stepW();
        char num[8];
        for (int k = 0; k < 16; ++k)
        {
            const float x0 = cellX(k), y0 = cellY(k);
            const int e = effect(k);
            const bool in = k < n && !midi;
            beginPath();
            rect(x0, y0, cw, 44.0f);
            fillColor(e > 0 ? col(theme::kAccent, k == cur ? 0.40f : (in ? 0.22f : 0.08f))
                            : col(k == cur ? theme::kBorder : theme::kBackground));
            fill();
            beginPath();
            rect(x0 + 0.5f, y0 + 0.5f, cw - 1.0f, 43.0f);
            strokeColor(col(theme::kBorder));
            strokeWidth(1.0f);
            stroke();
            if (k == cur)
            {
                beginPath();
                rect(x0 + 4.0f, y0 + 39.0f, cw - 8.0f, 2.0f);
                fillColor(col(theme::kText));
                fill();
            }
            std::snprintf(num, sizeof(num), "%d", k + 1);
            labelA(x0 + 5.0f, y0 + 8.0f, ALIGN_LEFT, 7.0f, 0.0f, theme::kMuted, in ? 1.0f : 0.5f, num, false);
            labelA(x0 + cw * 0.5f, y0 + 24.0f, ALIGN_CENTER, 7.5f, 0.5f, e > 0 ? theme::kText : theme::kMuted,
                   in ? 1.0f : 0.5f, nullstepfx::kEffectShort[e], true);
        }
    }

    // label() with an alpha
    void labelA(float x, float y, int align, float size, float spacing, uint32_t rgb, float a, const char* str, bool bold)
    {
        if (bold)
            fontFace("noto-bold");
        fontSize(size * kNotoLineToEm);
        textLetterSpacing(spacing);
        textAlign(align | ALIGN_MIDDLE);
        fillColor(col(rgb, a));
        text(x, y, str, nullptr);
        textLetterSpacing(0.0f);
        if (bold)
            fontFace("noto");
    }

    // a click on a step: left = the next effect, right = Off
    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        if (ev.press && (ev.button == kMouseButtonLeft || ev.button == kMouseButtonRight))
        {
            const float s = uiScale();
            const int k = cellAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
            if (k >= 0)
            {
                const int e = effect(k);
                setGesture(uint32_t(k), float(ev.button == kMouseButtonRight ? 0 : (e + 1) % nullstepfx::kNumEffects));
                return true;
            }
        }
        return NullKnobsUI::onMouse(ev);
    }

    // the wheel over a step steps through the effects
    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        const int k = cellAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (!presetOpen() && k >= 0 && ev.delta.getY() != 0.0f)
        {
            const int e = effect(k);
            setGesture(uint32_t(k), float(ev.delta.getY() > 0.0f ? std::min(8, e + 1) : std::max(0, e - 1)));
            return true;
        }
        return NullKnobsUI::onScroll(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 22)
            std::snprintf(buf, n, "%d st", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIStepFx();
}

END_NAMESPACE_DISTRHO
