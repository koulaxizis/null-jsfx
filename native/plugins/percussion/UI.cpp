/*
 * NULL JSFX - Percussion (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As percussion_gfx.jsfx: the groove's steps in the display with the voices' names and keys
 * (a click on a name selects the voice and plays it), a tab per voice with its four knobs next
 * to the room's two, and the groove's row.
 */

#include "NullUI.hpp"
#include "PercussionGrooves.hpp"
#include "PercussionSliders.hpp"

START_NAMESPACE_DISTRHO

static const char* const kNames[] = { "CONGA", "TUMBA", "BONGO HI", "BONGO LO", "SHAKER", "TAMBOURINE",
                                      "COWBELL", "CLAVES", "GUIRO", "CABASA", "TRIANGLE" };
// each voice's main key (C4 = 60)
static const char* const kKeys[] = { "D#4", "E4", "C4", "C#4", "A#4", "F#3", "G#3", "D#5", "C#5", "A4", "A5" };

// per voice: Level, Tune, Decay, Pan on the voice's tab, then Room and Size; the groove's row
static constexpr uint32_t kNumControls = nullperc::kTracks * 4 + 2 + 6;

static const NullKnobsUI::Control* controls()
{
    static NullKnobsUI::Control c[kNumControls];
    int n = 0;
    for (int t = 0; t < nullperc::kTracks; ++t)
    {
        const uint32_t b = 8 + 4 * uint32_t(t);
        c[n++] = { b, NullKnobsUI::kKnob, "LEVEL", 0, t, false };
        c[n++] = { b + 1, NullKnobsUI::kKnob, "TUNE", 0, t, true };
        c[n++] = { b + 2, NullKnobsUI::kKnob, "DECAY", 0, t, false };
        c[n++] = { b + 3, NullKnobsUI::kKnob, "PAN", 0, t, true };
    }
    c[n++] = { 5, NullKnobsUI::kKnob, "ROOM", 0, -1, false };
    c[n++] = { 6, NullKnobsUI::kKnob, "SIZE", 0, -1, false };
    c[n++] = { 0, NullKnobsUI::kSelect, "GROOVE", 1, -1, false };
    c[n++] = { 1, NullKnobsUI::kSelect, "PLAY", 1, -1, false };
    c[n++] = { 2, NullKnobsUI::kKnob, "SWING", 1, -1, false };
    c[n++] = { 3, NullKnobsUI::kKnob, "ACCENT", 1, -1, false };
    c[n++] = { 4, NullKnobsUI::kKnob, "HUMANIZE", 1, -1, false };
    c[n++] = { 7, NullKnobsUI::kKnob, "VOLUME", 1, -1, false };
    return c;
}

// the sliders with the selectors' upper-case names
static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullperc::kNumParams];
    for (uint32_t i = 0; i < nullperc::kNumParams; ++i)
        s[i] = nullperc::kSliders[i];
    s[0].options = nullperc::kGrooveShort;
    s[1].options = nullperc::kGroovePlayShort;
    return s;
}

class UIPercussion : public NullKnobsUI
{
public:
    UIPercussion()
        : NullKnobsUI({ "PERCUSSION", "INSTRUMENTS", 640, 206, "CONGA|TUMBA|BONGO H|BONGO L|SHAKER|TAMB|BELL|CLAVES|GUIRO|CABASA|TRI",
                        2, uiSliders(), nullperc::kNumParams, controls(), kNumControls, true })
    {
        for (int k = 1; k <= nullperc::kNumGrooves; ++k)
            nullperc::grooveSteps(k, fSteps[k - 1]);
    }

protected:
    // ---- geometry (design units, as in the GFX) -------------------------------------------------

    static float cellP(int len, int spb) { return (488.0f - float(len / spb - 1) * 3.0f) / float(len); }
    static float cellX(int i, int len, int spb) { return 128.0f + float(i) * cellP(len, spb) + float(i / spb) * 3.0f; }
    static float rowY(int t) { return 80.0f + float(t) * 15.0f; }

    // the voice whose name is under (x, y), or -1
    int nameAt(float x, float y) const
    {
        for (int t = 0; t < nullperc::kTracks; ++t)
            if (x >= 24.0f && x < 124.0f && y >= rowY(t) && y < rowY(t) + 13.0f)
                return t;
        return -1;
    }

    // ---- drawing ------------------------------------------------------------------------------

    void dtext(float x, float y, int align, float size, float spacing, Color c, const char* str, bool bold)
    {
        if (bold)
            fontFace("noto-bold");
        fontSize(size * kNotoLineToEm);
        textLetterSpacing(spacing);
        textAlign(align | ALIGN_MIDDLE);
        fillColor(c);
        NanoVG::text(x, y, str, nullptr);
        textLetterSpacing(0.0f);
        if (bold)
            fontFace("noto");
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha)
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

    static Color mix(uint32_t a, uint32_t b, float k)
    {
        auto ch = [k](uint32_t x, uint32_t y, int sh) { return int(float((x >> sh) & 255) * (1.0f - k) + float((y >> sh) & 255) * k); };
        return Color(ch(a, b, 16), ch(a, b, 8), ch(a, b, 0));
    }

    void drawDisplay() override
    {
        // the step and each voice's hits from the plugin, and the groove that plays
        double step = -1.0;
        int playing = 0;
        if (nulljsfx::Taps* tp = taps())
        {
            step = tp->extra.load(std::memory_order_relaxed);
            playing = int(std::lround(tp->aux[nullperc::kTracks].load(std::memory_order_relaxed)));
            for (int t = 0; t < nullperc::kTracks; ++t)
            {
                const float h = tp->aux[t].load(std::memory_order_relaxed);
                if (h != fHits[t])
                {
                    fHits[t] = h;
                    fFlash[t] = 1.0f;
                }
            }
        }
        if (playing > 0)
            fLast = playing;
        const bool keys = value(1) >= 0.5f;
        const int sel = int(std::lround(value(0)));
        const int g = playing > 0 ? playing : keys && fLast > 0 ? fLast : sel;

        char buf[64];
        int len = 16, spb = 4;
        if (g > 0 && g <= nullperc::kNumGrooves)
        {
            const nullperc::Groove& gr = nullperc::kGrooves[g - 1];
            len = gr.len;
            spb = gr.spb;
            std::snprintf(buf, sizeof(buf), "%s: %d BEATS OF %s", nullperc::kGrooveShort[g], len / spb, spb == 3 ? "TRIPLETS" : "SIXTEENTHS");
        }
        else
            std::snprintf(buf, sizeof(buf), "OFF: MIDI ONLY");
        dtext(128.0f, 71.0f, ALIGN_LEFT, 7.0f, 1.0f, col(theme::kMuted, 0.6f), buf, true);
        dtext(616.0f, 71.0f, ALIGN_RIGHT, 7.0f, 1.0f, col(theme::kMuted, 0.6f), keys ? "KEYS: HOLD C1..G1" : "PLAYS WITH THE HOST", true);

        const float p = cellP(len, spb), cw = std::max(1.0f, p - 2.0f);
        for (int t = 0; t < nullperc::kTracks; ++t)
        {
            const float yy = rowY(t);
            const bool selected = t == page();
            if (selected)
                box(24.0f, yy + 1.0f, 2.0f, 11.0f, theme::kAccent, 1.0f);
            dtext(31.0f, yy + 6.5f, ALIGN_LEFT, 7.0f, 1.0f, selected ? col(theme::kText) : mix(theme::kMuted, theme::kText, fFlash[t]), kNames[t], true);
            dtext(121.0f, yy + 6.5f, ALIGN_RIGHT, 7.0f, 0.0f, col(theme::kMuted, 0.7f), kKeys[t], false);
            fFlash[t] *= 0.8f;

            for (int i = 0; i < len; ++i)
            {
                const float xx = cellX(i, len, spb);
                const int v = g > 0 ? int(fSteps[g - 1][t * nullperc::kMaxSteps + i]) : 0;
                const float a = v == 1 || v == 4 || v == 6 ? 0.55f : v == 3 ? 0.25f : v == 5 ? 0.85f : 1.0f;
                box(xx, yy, cw, 13.0f, v > 0 ? theme::kAccent : theme::kBackground, a);
                if (v == 2 || v == 5)
                    box(xx + cw * 0.5f - 1.5f, yy + 5.0f, 3.0f, 3.0f, theme::kBackground, 1.0f);
                if (v == 4)
                    box(xx + cw * 0.25f, yy + 5.5f, cw * 0.5f, 2.0f, theme::kBackground, 1.0f);
                if (v == 6)
                    box(xx + 2.0f, yy + 5.5f, cw - 4.0f, 2.0f, theme::kBackground, 1.0f);
                outline(xx, yy, cw, 13.0f, theme::kBorder);
                // the playhead
                if (g == playing && step >= 0.0 && i == int(step - std::floor(step / len) * len))
                {
                    outline(xx, yy, cw, 13.0f, theme::kText);
                    outline(xx + 1.0f, yy + 1.0f, cw - 2.0f, 11.0f, theme::kText);
                }
            }
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int k = param >= 8 ? int(param - 8) % 4 : -1;
        if (param == 7)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (k == 0)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (k == 1)
            std::snprintf(buf, n, "%+.1f st", v);
        else if (k == 3)
        {
            const int pan = int(std::lround(v));
            if (pan == 0)
                std::snprintf(buf, n, "C");
            else
                std::snprintf(buf, n, "%c%d", pan < 0 ? 'L' : 'R', std::abs(pan));
        }
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    // ---- interaction --------------------------------------------------------------------------

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetOpen() || !ev.press || ev.button != kMouseButtonLeft)
            return NullKnobsUI::onMouse(ev);
        const float s = uiScale();
        const int t = nameAt(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (t < 0)
            return NullKnobsUI::onMouse(ev);
        setPage(t);
        if (nulljsfx::Taps* tp = taps())
            tp->request.store(t + 1);
        return true;
    }

private:
    double fSteps[nullperc::kNumGrooves][nullperc::kTracks * nullperc::kMaxSteps];
    float fHits[nullperc::kTracks] = {}, fFlash[nullperc::kTracks] = {};
    int fLast = 0;
};

UI* createUI()
{
    return new UIPercussion();
}

END_NAMESPACE_DISTRHO
