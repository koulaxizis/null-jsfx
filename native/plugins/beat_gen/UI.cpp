/*
 * NULL JSFX - Beat Gen (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "BeatGenDSP.hpp"
#include "BeatGenSliders.hpp"

START_NAMESPACE_DISTRHO

// as in beat_gen_gfx.jsfx: tab PATTERN, then one control per lane on DENSITY, STEPS, NOTES and LOCK
static const NullKnobsUI::Control kControls[] = {
    { 32, NullKnobsUI::kSelect, "STYLE", 0, 0, false },
    { 33, NullKnobsUI::kKnob, "SEED", 0, 0, false },
    { 34, NullKnobsUI::kSelect, "RATE", 0, 0, false },
    { 35, NullKnobsUI::kKnob, "MUTATE", 0, 0, false },
    { 36, NullKnobsUI::kSelect, "EVERY", 0, 0, false },
    { 39, NullKnobsUI::kKnob, "SWING", 0, 0, false },
    { 37, NullKnobsUI::kKnob, "VELOCITY", 1, 0, false },
    { 38, NullKnobsUI::kKnob, "VEL RANGE", 1, 0, false },
    { 40, NullKnobsUI::kKnob, "GATE", 1, 0, false },
    { 41, NullKnobsUI::kSelect, "RUN", 1, 0, false },
    { 42, NullKnobsUI::kSelect, "CHANNEL", 1, 0, false },
    // DENSITY tab
    { 0, NullKnobsUI::kKnob, "KICK", 0, 1, false },
    { 1, NullKnobsUI::kKnob, "SNARE", 0, 1, false },
    { 2, NullKnobsUI::kKnob, "CL HAT", 0, 1, false },
    { 3, NullKnobsUI::kKnob, "OP HAT", 0, 1, false },
    { 4, NullKnobsUI::kKnob, "CLAP", 1, 1, false },
    { 5, NullKnobsUI::kKnob, "TOM", 1, 1, false },
    { 6, NullKnobsUI::kKnob, "PERC", 1, 1, false },
    { 7, NullKnobsUI::kKnob, "CYMBAL", 1, 1, false },
    // STEPS tab
    { 8, NullKnobsUI::kKnob, "KICK", 0, 2, false },
    { 9, NullKnobsUI::kKnob, "SNARE", 0, 2, false },
    { 10, NullKnobsUI::kKnob, "CL HAT", 0, 2, false },
    { 11, NullKnobsUI::kKnob, "OP HAT", 0, 2, false },
    { 12, NullKnobsUI::kKnob, "CLAP", 1, 2, false },
    { 13, NullKnobsUI::kKnob, "TOM", 1, 2, false },
    { 14, NullKnobsUI::kKnob, "PERC", 1, 2, false },
    { 15, NullKnobsUI::kKnob, "CYMBAL", 1, 2, false },
    // NOTES tab
    { 16, NullKnobsUI::kKnob, "KICK", 0, 3, false },
    { 17, NullKnobsUI::kKnob, "SNARE", 0, 3, false },
    { 18, NullKnobsUI::kKnob, "CL HAT", 0, 3, false },
    { 19, NullKnobsUI::kKnob, "OP HAT", 0, 3, false },
    { 20, NullKnobsUI::kKnob, "CLAP", 1, 3, false },
    { 21, NullKnobsUI::kKnob, "TOM", 1, 3, false },
    { 22, NullKnobsUI::kKnob, "PERC", 1, 3, false },
    { 23, NullKnobsUI::kKnob, "CYMBAL", 1, 3, false },
    // LOCK tab
    { 24, NullKnobsUI::kSelect, "KICK", 0, 4, false },
    { 25, NullKnobsUI::kSelect, "SNARE", 0, 4, false },
    { 26, NullKnobsUI::kSelect, "CL HAT", 0, 4, false },
    { 27, NullKnobsUI::kSelect, "OP HAT", 0, 4, false },
    { 28, NullKnobsUI::kSelect, "CLAP", 1, 4, false },
    { 29, NullKnobsUI::kSelect, "TOM", 1, 4, false },
    { 30, NullKnobsUI::kSelect, "PERC", 1, 4, false },
    { 31, NullKnobsUI::kSelect, "CYMBAL", 1, 4, false },
};

static const char* const kNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kLaneNames[] = { "KICK", "SNARE", "CL HAT", "OP HAT", "CLAP", "TOM", "PERC", "CYMBAL" };
static const char* const kStyleNames[] = { "FOUR ON FLOOR", "BREAKBEAT", "HIP HOP", "TRAP", "RANDOM" };
static const nullbeatgen::UiSliders kUi;

class UIBeatGen : public NullKnobsUI
{
public:
    UIBeatGen()
        : NullKnobsUI({ "BEAT GEN", "MIDI", 480, 120, "PATTERN|DENSITY|STEPS|NOTES|LOCK", 2, kUi.s,
                        nullbeatgen::kNumSliders, kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // the plot rectangle of the display (design units), as in beat_gen_gfx.jsfx
    static constexpr float kPX = 24.0f + 58.0f, kPY = 62.0f + 4.0f, kPW = 432.0f - 62.0f, kPH = 104.0f - 14.0f;

    nullbeatgen::Params params() const
    {
        return nullbeatgen::paramsFrom([this](uint32_t i) { return nullbeatgen::sliderValue(i, value(i)); });
    }

    // as in beat_gen_gfx.jsfx: one row per lane, one cell per step (brighter = louder), the steps
    // past the lane's Steps dimmed, the step each lane plays outlined (Mutate's variation while it
    // plays); a locked lane's name is bright, with a mark
    void drawDisplay() override
    {
        double cur = -1.0, mb = 0.0;
        float playing = 0.0f;
        if (nulljsfx::Taps* t = taps())
        {
            cur = double(t->extra.load(std::memory_order_relaxed)) - 1.0;
            mb = double(t->aux[0].load(std::memory_order_relaxed));
            playing = t->inLevel.load(std::memory_order_relaxed);
        }
        const nullbeatgen::Params p = params();
        bool hit[128], mh[128];
        int vel[128];
        nullbeatgen::makePattern(p, hit, vel);
        const bool mut = p.mutPct > 0.0;
        if (mut)
            nullbeatgen::mutateInto(p, mb, hit, mh);
        const float cw = kPW / 16.0f, rh = kPH / 8.0f;
        for (int l = 0; l < nullbeatgen::kLanes; ++l)
        {
            const float y0 = kPY + float(l) * rh;
            label(kPX - 6.0f, y0 + rh * 0.5f, ALIGN_RIGHT, 7.0f, 0.0f, p.lock[l] ? theme::kText : theme::kMuted, kLaneNames[l]);
            if (p.lock[l])
            {
                beginPath();
                rect(kPX - 54.0f, y0 + rh * 0.5f - 2.0f, 4.0f, 4.0f);
                fillColor(col(theme::kText));
                fill();
            }
            const double n = double(p.steps[l]);
            const int lc = cur >= 0.0 ? int(cur - std::floor(cur / n) * n + 0.0001) : -1;
            for (int i = 0; i < 16; ++i)
            {
                const int k = l * 16 + i;
                const bool h = mut ? mh[k] : hit[k];
                const float x0 = kPX + float(i) * cw + 1.0f;
                beginPath();
                rect(x0, y0 + 1.0f, cw - 2.0f, rh - 2.0f);
                fillColor(i >= p.steps[l] ? col(theme::kBackground, 0.4f)
                          : h ? col(theme::kAccent, 0.25f + 0.75f * float(vel[k]) / 127.0f) : col(theme::kBackground));
                fill();
                if (i == lc)
                {
                    strokeColor(col(theme::kText));
                    strokeWidth(1.0f);
                    stroke();
                }
            }
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s   SEED %d   CH %d   %s", kStyleNames[p.style < 0 ? 0 : p.style > 4 ? 4 : p.style],
                      int(p.seed), p.channel + 1, p.run ? "ALWAYS" : playing > 0.5f ? "PLAYING" : "WAITS FOR PLAY");
        label(240.0f, 62.0f + 104.0f - 3.0f, ALIGN_CENTER, 7.5f, 1.0f, theme::kMuted, buf, true);
    }

    // a click on a lane's name switches its Lock
    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        if (ev.button == 1 && ev.press)
        {
            const float s = uiScale();
            const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
            if (x >= kPX - 56.0f && x < kPX - 2.0f && y >= kPY && y < kPY + kPH)
            {
                const int l = std::min(7, int((y - kPY) / (kPH / 8.0f)));
                const uint32_t idx = uint32_t(24 + l);
                setGesture(idx, value(idx) >= 0.5f ? 0.0f : 1.0f);
                return true;
            }
        }
        return NullKnobsUI::onMouse(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const double x = nullbeatgen::sliderValue(param, v);
        if (param >= 16 && param < 24)
        {
            const int k = int(std::lround(x));
            std::snprintf(buf, n, "%s%d", kNotes[k % 12], k / 12 - 1);
        }
        else if (param < 8 || param == 38 || param == 39 || param == 40)
            std::snprintf(buf, n, "%d%%", int(x));
        else if (param == 35)
        {
            if (x <= 0.0)
                std::snprintf(buf, n, "OFF");
            else
                std::snprintf(buf, n, "%d%%", int(x));
        }
        else if (param < 16 || param == 33 || param == 37)
            std::snprintf(buf, n, "%d", int(x));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIBeatGen();
}

END_NAMESPACE_DISTRHO
