/*
 * NULL JSFX - Drum Machine (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As drum_machine_gfx.jsfx: the step grid in the display, a tab per voice with its five knobs,
 * and the kit's row. A click on a step cycles off, hit, accent, flam, roll; a right click
 * clears it; a click on a name selects the voice and plays it; the boxes on the right set
 * each track's length. An edit plays the User pattern (a factory or generated one is copied
 * first). Under the grid, the song: Song on or off and 8 sections (a pattern and its bars; a
 * click on the left half steps back, on the right half forward, the wheel too; a right click on
 * a pattern ends the song there), saved as the state "song". A click on a step also reaches
 * chance. The third row holds Probability, Seed, Generate, Density, Style and Outputs; Kit sits
 * next to the voice's knobs on every tab.
 */

#include "NullUI.hpp"
#include "DrumMachinePatterns.hpp"
#include "DrumMachineSliders.hpp"

#include <string>

START_NAMESPACE_DISTRHO

static const char* const kNames[] = { "KICK", "SNARE", "CLAP", "RIM", "CLOSED HAT",
                                      "OPEN HAT", "LOW TOM", "HIGH TOM", "COWBELL", "CYMBAL" };
static const char* const kToneNames[] = { "PUNCH", "SNAPPY", "ROOM", "CLAVE", "NOISE",
                                          "NOISE", "SWEEP", "SWEEP", "BRIGHT", "CRASH" };

// per voice: Level, Tune, Decay, Tone, Pan on the voice's tab, then Kit; the kit's rows on every tab
static constexpr uint32_t kNumControls = nulldrum::kTracks * 5 + 1 + 6 + 6;

static const NullKnobsUI::Control* controls()
{
    static NullKnobsUI::Control c[kNumControls];
    int n = 0;
    for (int t = 0; t < nulldrum::kTracks; ++t)
    {
        const uint32_t b = 6 + 5 * uint32_t(t);
        c[n++] = { b, NullKnobsUI::kKnob, "LEVEL", 0, t, false };
        c[n++] = { b + 1, NullKnobsUI::kKnob, "TUNE", 0, t, true };
        c[n++] = { b + 2, NullKnobsUI::kKnob, "DECAY", 0, t, false };
        c[n++] = { b + 3, NullKnobsUI::kKnob, kToneNames[t], 0, t, false };
        c[n++] = { b + 4, NullKnobsUI::kKnob, "PAN", 0, t, true };
    }
    c[n++] = { 61, NullKnobsUI::kSelect, "KIT", 0, -1, false };
    c[n++] = { 0, NullKnobsUI::kSelect, "PATTERN", 1, -1, false };
    c[n++] = { 1, NullKnobsUI::kKnob, "SWING", 1, -1, false };
    c[n++] = { 2, NullKnobsUI::kKnob, "ACCENT", 1, -1, false };
    c[n++] = { 3, NullKnobsUI::kKnob, "DRIVE", 1, -1, false };
    c[n++] = { 4, NullKnobsUI::kKnob, "GRIT", 1, -1, false };
    c[n++] = { 5, NullKnobsUI::kKnob, "VOLUME", 1, -1, false };
    c[n++] = { 56, NullKnobsUI::kKnob, "PROBABILITY", 2, -1, false };
    c[n++] = { 57, NullKnobsUI::kKnob, "SEED", 2, -1, false };
    c[n++] = { 58, NullKnobsUI::kSelect, "GENERATE", 2, -1, false };
    c[n++] = { 59, NullKnobsUI::kKnob, "DENSITY", 2, -1, false };
    c[n++] = { 60, NullKnobsUI::kSelect, "STYLE", 2, -1, false };
    c[n++] = { 62, NullKnobsUI::kSelect, "OUTPUTS", 2, -1, false };
    return c;
}

// the sliders with the selectors' upper-case names
static const SliderInfo* uiSliders()
{
    static SliderInfo s[nulldrum::kNumParams];
    for (uint32_t i = 0; i < nulldrum::kNumParams; ++i)
        s[i] = nulldrum::kSliders[i];
    s[0].options = nulldrum::kPatternShort;
    s[58].options = nulldrum::kGenerateShort;
    s[60].options = nulldrum::kStyleShort;
    s[61].options = nulldrum::kKitShort;
    s[62].options = nulldrum::kOutputsShort;
    s[63].options = nulldrum::kSongShort;
    return s;
}

class UIDrumMachine : public NullKnobsUI
{
public:
    UIDrumMachine()
        : NullKnobsUI({ "DRUM MACHINE", "INSTRUMENTS", 640, 240, "KICK|SNARE|CLAP|RIM|C HAT|O HAT|LO TOM|HI TOM|BELL|CYMBAL",
                        3, uiSliders(), nulldrum::kNumParams, controls(), kNumControls, true })
    {
        nulldrum::defaultUser(fUser);
        nulldrum::defaultSong(fSong);
    }

protected:
    // ---- geometry (design units, as in the GFX) -------------------------------------------------

    static float cellX(int i) { return 112.0f + float(i) * 27.0f + float(i / 4) * 4.0f; }
    static float rowY(int t) { return 82.0f + float(t) * 17.0f; }

    int pattern() const { return int(std::lround(value(0))); }
    bool generated() const { return value(58) >= 0.5f && pattern() > 0; }
    bool songOn() const { return value(63) >= 0.5f; }

    // the song's sections up to the first End (0 with Song Off)
    int songSections() const
    {
        int n = 0;
        if (songOn())
            for (int k = 0; k < nulldrum::kSongSections; ++k)
                if (fSong[k] >= 1 && n == k)
                    ++n;
        return n;
    }

    // the section that plays (the first one while stopped)
    int songSection() const
    {
        nulljsfx::Taps* t = taps();
        return t ? std::max(0, std::min(nulldrum::kSongSections - 1, int(std::lround(t->aux[nulldrum::kTracks].load(std::memory_order_relaxed))))) : 0;
    }

    // the pattern the grid shows: the song section's, the generated one, the User one (also
    // when Off) or the factory one that plays
    const double* shown()
    {
        int p = pattern();
        if (songSections() > 0)
            p = int(fSong[songSection()]);
        else if (generated())
        {
            // the plugin's parameters arrive as float: back on the JSFX's 1-steps
            nulldrum::generate(fGen, std::round(double(value(57))), std::round(double(value(59))), std::round(double(value(60))));
            return fGen;
        }
        if (p <= 1)
            return fUser;
        if (fFactoryShown != p)
        {
            nulldrum::factory(p - 1, fFactory);
            fFactoryShown = p;
        }
        return fFactory;
    }

    // what is under (x, y): kind 1 a name, 2 a step, 3 a length box; track and step
    int at(float x, float y, int& t, int& i) const
    {
        for (t = 0; t < nulldrum::kTracks; ++t)
        {
            const float yy = rowY(t);
            if (y < yy || y >= yy + 15.0f)
                continue;
            if (x >= 24.0f && x < 104.0f)
                return 1;
            for (i = 0; i < nulldrum::kSteps; ++i)
                if (x >= cellX(i) && x < cellX(i) + 25.0f)
                    return 2;
            if (x >= 566.0f && x < 616.0f)
                return 3;
        }
        // the song: 4 Song on or off, 5 a section's pattern, 6 its bars (i = the section)
        if (y >= 262.0f && y < 294.0f && x >= 24.0f && x < 104.0f)
            return 4;
        for (i = 0; i < nulldrum::kSongSections; ++i)
        {
            const float xx = 112.0f + float(i) * 63.0f;
            if (x < xx || x >= xx + 59.0f)
                continue;
            if (y >= 262.0f && y < 277.0f)
                return 5;
            if (y >= 279.0f && y < 294.0f)
                return 6;
        }
        return 0;
    }

    // ---- drawing ------------------------------------------------------------------------------

    void dtext(float x, float y, int align, float size, float spacing, uint32_t rgb, float alpha, const char* str, bool bold)
    {
        if (bold)
            fontFace("noto-bold");
        fontSize(size * kNotoLineToEm);
        textLetterSpacing(spacing);
        textAlign(align | ALIGN_MIDDLE);
        fillColor(col(rgb, alpha));
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

    void outline(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    // ng_arrow, faded
    void arrow(float x, float y, float dir, float alpha)
    {
        beginPath();
        moveTo(x - dir * 1.5f, y - 3.0f);
        lineTo(x + dir * 1.5f, y);
        lineTo(x - dir * 1.5f, y + 3.0f);
        strokeColor(col(theme::kMuted, alpha));
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
        const double* pat = shown();
        const int p = pattern();
        char buf[16];

        for (int i = 0; i < nulldrum::kSteps; ++i)
        {
            std::snprintf(buf, sizeof(buf), "%d", i + 1);
            const bool beat = i % 4 == 0;
            dtext(cellX(i) + 12.5f, 71.0f, ALIGN_CENTER, 7.0f, 0.0f, beat ? theme::kText : theme::kMuted, beat ? 0.9f : 0.6f, buf, true);
        }
        dtext(591.0f, 71.0f, ALIGN_CENTER, 7.0f, 1.0f, theme::kMuted, 0.6f, "STEPS", true);
        const int songN = songSections(), songK = songSection();
        if (songN > 0)
        {
            std::snprintf(buf, sizeof(buf), "SONG %d", songK + 1);
            dtext(28.0f, 71.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, 0.6f, buf, true);
        }
        else
            dtext(28.0f, 71.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, 0.6f, p == 0 ? "OFF: MIDI ONLY" : generated() ? "GENERATED" : p == 1 ? "USER" : "FACTORY", true);

        // the step that plays and each voice's hits, from the plugin
        double step = -1.0;
        if (nulljsfx::Taps* tp = taps())
        {
            step = tp->extra.load(std::memory_order_relaxed);
            for (int t = 0; t < nulldrum::kTracks; ++t)
            {
                const float h = tp->aux[t].load(std::memory_order_relaxed);
                if (h != fHits[t])
                {
                    fHits[t] = h;
                    fFlash[t] = 1.0f;
                }
            }
        }

        for (int t = 0; t < nulldrum::kTracks; ++t)
        {
            const float yy = rowY(t);
            const int len = std::max(1, std::min(16, int(std::lround(pat[nulldrum::kTracks * nulldrum::kSteps + t]))));
            const bool sel = t == page();
            if (sel)
                box(24.0f, yy + 2.0f, 2.0f, 11.0f, theme::kAccent, 1.0f);
            fontFace("noto-bold");
            fontSize(7.5f * kNotoLineToEm);
            textLetterSpacing(1.0f);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fillColor(sel ? col(theme::kText) : mix(theme::kMuted, theme::kText, fFlash[t]));
            NanoVG::text(31.0f, yy + 7.5f, kNames[t], nullptr);
            textLetterSpacing(0.0f);
            fontFace("noto");
            fFlash[t] *= 0.8f;

            for (int i = 0; i < nulldrum::kSteps; ++i)
            {
                const float xx = cellX(i);
                const int v = int(std::lround(pat[t * nulldrum::kSteps + i]));
                const bool on = i < len;
                const float a = (v > 0 ? (v == 1 ? 0.55f : v == 2 ? 1.0f : v == 5 ? 0.3f : 0.8f) : 1.0f) * (on ? 1.0f : 0.35f);
                box(xx, yy, 25.0f, 15.0f, v > 0 ? theme::kAccent : theme::kBackground, a);
                const float ma = on ? 1.0f : 0.6f;
                if (v == 3)
                    box(xx + 9.0f, yy + 3.0f, 2.0f, 9.0f, theme::kBackground, ma);
                if (v == 4)
                    for (int k = 0; k < 3; ++k)
                        box(xx + 8.0f + 4.0f * float(k), yy + 6.0f, 2.0f, 3.0f, theme::kBackground, ma);
                if (v == 2)
                    box(xx + 11.0f, yy + 6.0f, 3.0f, 3.0f, theme::kBackground, ma);
                if (v == 5)
                    outline(xx + 10.0f, yy + 5.0f, 5.0f, 5.0f, theme::kBackground, ma);
                const bool hover = fHoverKind == 2 && fHoverT == t && fHoverI == i;
                outline(xx, yy, 25.0f, 15.0f, hover ? theme::kTrackHover : theme::kBorder);
                // the playhead, at the track's own position
                if (step >= 0.0 && on && i == int(step - std::floor(step / len) * len))
                {
                    outline(xx, yy, 25.0f, 15.0f, theme::kText);
                    outline(xx + 1.0f, yy + 1.0f, 23.0f, 13.0f, theme::kText);
                }
            }

            box(566.0f, yy, 50.0f, 15.0f, theme::kBackground, 1.0f);
            outline(566.0f, yy, 50.0f, 15.0f, fHoverKind == 3 && fHoverT == t ? theme::kTrackHover : theme::kBorder);
            std::snprintf(buf, sizeof(buf), "%d", len);
            dtext(591.0f, yy + 7.5f, ALIGN_CENTER, 7.5f, 0.0f, len < 16 ? theme::kText : theme::kMuted, 1.0f, buf, false);
            drawArrow(572.0f, yy + 7.5f, -1.0f);
            drawArrow(610.0f, yy + 7.5f, 1.0f);
        }

        // the song: Song on or off, then the 8 sections (those after the first End dimmed, the
        // one that plays outlined)
        const bool son = songOn();
        box(24.0f, 262.0f, 80.0f, 32.0f, son ? theme::kAccent : theme::kBackground, 1.0f);
        outline(24.0f, 262.0f, 80.0f, 32.0f, fHoverKind == 4 ? theme::kTrackHover : theme::kBorder);
        dtext(64.0f, 271.0f, ALIGN_CENTER, 7.0f, 1.0f, son ? theme::kBackground : theme::kMuted, 1.0f, "SONG", true);
        dtext(64.0f, 285.0f, ALIGN_CENTER, 7.5f, 1.0f, son ? theme::kBackground : theme::kText, 1.0f, son ? "ON" : "OFF", true);
        int live = 0;
        for (int k = 0; k < nulldrum::kSongSections; ++k)
        {
            const float xx = 112.0f + float(k) * 63.0f;
            const int sp = int(fSong[k]), sb = int(fSong[nulldrum::kSongSections + k]);
            if (sp >= 1 && live == k)
                ++live;
            const float a = k <= live ? 1.0f : 0.35f;
            box(xx, 262.0f, 59.0f, 15.0f, theme::kBackground, 1.0f);
            box(xx, 279.0f, 59.0f, 15.0f, theme::kBackground, 1.0f);
            outline(xx, 262.0f, 59.0f, 15.0f, fHoverKind == 5 && fHoverI == k ? theme::kTrackHover : theme::kBorder);
            outline(xx, 279.0f, 59.0f, 15.0f, fHoverKind == 6 && fHoverI == k ? theme::kTrackHover : theme::kBorder);
            if (songN > 0 && step >= 0.0 && k == songK)
            {
                outline(xx, 262.0f, 59.0f, 15.0f, theme::kText);
                outline(xx + 1.0f, 263.0f, 57.0f, 13.0f, theme::kText);
            }
            const uint32_t c = sp >= 1 ? theme::kText : theme::kMuted;
            dtext(xx + 29.5f, 269.5f, ALIGN_CENTER, 6.5f, 0.5f, c, a, sp == 0 ? "END" : nulldrum::kPatternShort[std::min(sp, 19)], true);
            std::snprintf(buf, sizeof(buf), sb == 1 ? "%d BAR" : "%d BARS", sb);
            dtext(xx + 29.5f, 286.5f, ALIGN_CENTER, 7.5f, 0.0f, c, a, buf, false);
            arrow(xx + 6.0f, 286.5f, -1.0f, a);
            arrow(xx + 53.0f, 286.5f, 1.0f, a);
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        const int k = param >= 6 && param < 56 ? int(param - 6) % 5 : -1;
        if (param == 5)
            std::snprintf(buf, n, "%+.1f dB", v);
        else if (k == 0)
            std::snprintf(buf, n, "%.1f dB", v);
        else if (k == 1)
            std::snprintf(buf, n, "%+.1f st", v);
        else if (k == 4)
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

    // ---- the User pattern ---------------------------------------------------------------------

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "song") == 0)
        {
            const char* c = value;
            double p[nulldrum::kSongSize];
            for (int i = 0; i < nulldrum::kSongSize; ++i)
            {
                char* end = nullptr;
                p[i] = double(std::strtol(c, &end, 10));
                if (end == c)
                    return;
                c = end;
            }
            std::copy(p, p + nulldrum::kSongSize, fSong);
            repaint();
            return;
        }
        if (std::strcmp(key, "pattern") != 0)
            return;
        const char* c = value;
        double p[nulldrum::kPatternSize];
        for (int i = 0; i < nulldrum::kPatternSize; ++i)
        {
            char* end = nullptr;
            p[i] = double(std::strtol(c, &end, 10));
            if (end == c)
                return;
            c = end;
        }
        std::copy(p, p + nulldrum::kPatternSize, fUser);
        repaint();
    }

    // an edit plays the User pattern: from a factory or generated pattern it starts as a copy of
    // it (and Generate goes Off); in the song, the section shown becomes User
    void toUser()
    {
        const int p = pattern();
        if (songSections() > 0)
        {
            const int k = songSection(), sp = int(fSong[k]);
            if (sp >= 2)
            {
                nulldrum::factory(sp - 1, fUser);
                fSong[k] = 1;
                sendSong();
            }
            return;
        }
        if (generated())
        {
            std::copy(fGen, fGen + nulldrum::kPatternSize, fUser);
            setGesture(58, 0.0f);
            if (p != 1)
                setGesture(0, 1.0f);
            return;
        }
        if (p == 1)
            return;
        if (p >= 2)
            nulldrum::factory(p - 1, fUser);
        setGesture(0, 1.0f);
    }

    void send()
    {
        std::string s;
        for (int i = 0; i < nulldrum::kPatternSize; ++i)
        {
            s += i ? " " : "";
            s += std::to_string(int(fUser[i]));
        }
        setState("pattern", s.c_str());
        repaint();
    }

    void sendSong()
    {
        std::string s;
        for (int i = 0; i < nulldrum::kSongSize; ++i)
        {
            s += i ? " " : "";
            s += std::to_string(int(fSong[i]));
        }
        setState("song", s.c_str());
        repaint();
    }

    // a song section's pattern and bars
    void setSection(int k, int p, int b)
    {
        fSong[k] = std::max(0, std::min(19, p));
        fSong[nulldrum::kSongSections + k] = std::max(1, std::min(16, b));
        sendSong();
    }

    void setLength(int t, int len)
    {
        toUser();
        fUser[nulldrum::kTracks * nulldrum::kSteps + t] = std::max(1, std::min(16, len));
        send();
    }

    // ---- interaction --------------------------------------------------------------------------

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetOpen() || !ev.press || (ev.button != kMouseButtonLeft && ev.button != kMouseButtonRight))
            return NullKnobsUI::onMouse(ev);
        const float s = uiScale();
        int t = 0, i = 0;
        const int kind = at(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s, t, i);
        if (kind == 0)
            return NullKnobsUI::onMouse(ev);
        const float x = float(ev.pos.getX()) / s;
        if (kind == 4)
        {
            if (ev.button == kMouseButtonLeft)
                setGesture(63, songOn() ? 0.0f : 1.0f);
            return true;
        }
        if (kind == 5 || kind == 6)
        {
            const int sp = int(fSong[i]), sb = int(fSong[nulldrum::kSongSections + i]);
            const int d = x < 112.0f + float(i) * 63.0f + 29.5f ? -1 : 1;
            if (kind == 5 && ev.button == kMouseButtonRight)
                setSection(i, 0, sb);
            else if (ev.button == kMouseButtonLeft)
                setSection(i, kind == 5 ? sp + d : sp, kind == 6 ? sb + d : sb);
            return true;
        }
        const double* pat = shown();
        const int len = int(std::lround(pat[nulldrum::kTracks * nulldrum::kSteps + t]));
        if (kind == 1 && ev.button == kMouseButtonLeft)
        {
            setPage(t);
            if (nulljsfx::Taps* tp = taps())
                tp->request.store(t + 1);
        }
        else if (kind == 2)
        {
            const int v = int(std::lround(pat[t * nulldrum::kSteps + i]));
            toUser();
            fUser[t * nulldrum::kSteps + i] = ev.button == kMouseButtonRight ? 0 : (v >= 5 ? 0 : v + 1);
            send();
        }
        else if (kind == 3 && ev.button == kMouseButtonLeft)
            setLength(t, float(ev.pos.getX()) / s < 591.0f ? len - 1 : len + 1);
        return true;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        int t = 0, i = 0;
        const int kind = presetOpen() ? 0 : at(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s, t, i);
        if (kind != fHoverKind || t != fHoverT || i != fHoverI)
        {
            fHoverKind = kind;
            fHoverT = t;
            fHoverI = i;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        int t = 0, i = 0;
        const int kind = presetOpen() ? 0 : at(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s, t, i);
        if (kind == 5 || kind == 6)
        {
            const float d = float(ev.delta.getY()) + float(ev.delta.getX());
            if (d == 0.0f)
                return false;
            const int sp = int(fSong[i]), sb = int(fSong[nulldrum::kSongSections + i]), k = d > 0.0f ? 1 : -1;
            setSection(i, kind == 5 ? sp + k : sp, kind == 6 ? sb + k : sb);
            return true;
        }
        if (kind == 3)
        {
            const float d = float(ev.delta.getY()) + float(ev.delta.getX());
            if (d == 0.0f)
                return false;
            const int len = int(std::lround(shown()[nulldrum::kTracks * nulldrum::kSteps + t]));
            setLength(t, d > 0.0f ? len + 1 : len - 1);
            return true;
        }
        return NullKnobsUI::onScroll(ev);
    }

private:
    double fUser[nulldrum::kPatternSize];
    double fFactory[nulldrum::kPatternSize];
    double fGen[nulldrum::kPatternSize] = {};
    double fSong[nulldrum::kSongSize];
    int fFactoryShown = -1;
    float fHits[nulldrum::kTracks] = {}, fFlash[nulldrum::kTracks] = {};
    int fHoverKind = 0, fHoverT = 0, fHoverI = 0;
};

UI* createUI()
{
    return new UIDrumMachine();
}

END_NAMESPACE_DISTRHO
