/*
 * NULL JSFX - Sampler (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As sampler_gfx.jsfx: the sample's waveform with Start, the loop and the voices that play (a
 * click plays the root key), tabs SAMPLE and SHAPE. The FILE cell: its arrows step through the
 * .wav/.flac files of the null_jsfx_samples folder(s) and the built-in sound, its middle opens
 * a file browser for any WAV or FLAC file. The choice is the "sample" state (a path), which
 * the host saves with the project.
 */

#include "NullUI.hpp"
#include "SamplerShared.hpp"
#include "SamplerFiles.hpp"
#include "SamplerSliders.hpp"

#include <atomic>
#include <cctype>
#include <string>

START_NAMESPACE_DISTRHO

// control 0 is the FILE cell: it holds the place in the row (the base class would draw a
// selector for param 0 there) and this UI draws over it
static constexpr uint32_t kFileCell = 0;

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "FILE", 0, 0, false },
    { 0, NullKnobsUI::kSelect, "MODE", 0, 0, false },
    { 1, NullKnobsUI::kKnob, "ROOT", 0, 0, false },
    { 2, NullKnobsUI::kKnob, "TRANSPOSE", 0, 0, true },
    { 3, NullKnobsUI::kKnob, "FINE", 0, 0, true },
    { 19, NullKnobsUI::kKnob, "VOLUME", 0, 0, false },
    { 15, NullKnobsUI::kKnob, "START", 1, 0, false },
    { 16, NullKnobsUI::kSelect, "REVERSE", 1, 0, false },
    { 12, NullKnobsUI::kSelect, "LOOP", 1, 0, false },
    { 13, NullKnobsUI::kKnob, "LOOP START", 1, 0, false },
    { 14, NullKnobsUI::kKnob, "LOOP END", 1, 0, false },
    { 18, NullKnobsUI::kKnob, "BEND", 1, 0, false },
    { 4, NullKnobsUI::kKnob, "ATTACK", 0, 1, false, true },
    { 5, NullKnobsUI::kKnob, "DECAY", 0, 1, false, true },
    { 6, NullKnobsUI::kKnob, "SUSTAIN", 0, 1, false },
    { 7, NullKnobsUI::kKnob, "RELEASE", 0, 1, false, true },
    { 17, NullKnobsUI::kKnob, "VELOCITY", 0, 1, false },
    { 8, NullKnobsUI::kKnob, "CUTOFF", 1, 1, false, true },
    { 9, NullKnobsUI::kKnob, "RESONANCE", 1, 1, false },
    { 10, NullKnobsUI::kKnob, "FILTER ENV", 1, 1, true },
    { 11, NullKnobsUI::kKnob, "FILTER DECAY", 1, 1, false, true },
};

// the sliders with the selectors' upper-case names
static const SliderInfo* uiSliders()
{
    static SliderInfo s[nullsmp::kNumParams];
    for (uint32_t i = 0; i < nullsmp::kNumParams; ++i)
        s[i] = nullsmp::kSliders[i];
    s[0].options = nullsmp::kModeShort;
    s[12].options = nullsmp::kOffOnShort;
    s[16].options = nullsmp::kOffOnShort;
    return s;
}

static const char* const kPitch[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

static void noteName(int n, char* buf, size_t len)
{
    n = std::max(0, std::min(127, n));
    std::snprintf(buf, len, "%s%d", kPitch[n % 12], n / 12 - 1);
}

static void msText(float v, char* buf, size_t n)
{
    if (v >= 1000.0f)
        std::snprintf(buf, n, "%.2f s", double(v) / 1000.0);
    else
        std::snprintf(buf, n, "%d ms", int(std::lround(v)));
}

class UISampler : public NullKnobsUI
{
public:
    UISampler()
        : NullKnobsUI({ "SAMPLER", "INSTRUMENTS", 640, 160, "SAMPLE|SHAPE", 2, uiSliders(), nullsmp::kNumParams,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    SamplerPluginBase* view() const
    {
        return static_cast<SamplerPluginBase*>(getPluginInstancePointer());
    }

    static constexpr float kX0 = 28.0f, kW = 584.0f, kY0 = 82.0f, kH = 116.0f;

    void drawDisplay() override
    {
        SamplerPluginBase* p = view();
        char buf[160];
        const bool ok = p && p->fileOk.load(std::memory_order_relaxed);
        if (ok)
        {
            const int ch = p->channels.load(std::memory_order_relaxed);
            std::snprintf(buf, sizeof(buf), "%s: %s, %.1f KHZ, %.2f S", fFileName.empty() ? "FILE" : fFileName.c_str(),
                          ch == 1 ? "MONO" : ch == 2 ? "STEREO" : "MULTI", double(p->fileRate.load()) / 1000.0, double(p->seconds.load()));
        }
        else if (!fFilePath.empty())
            std::snprintf(buf, sizeof(buf), "CANNOT READ %s: BUILT-IN SOUND", fFileName.c_str());
        else
            std::snprintf(buf, sizeof(buf), "BUILT-IN SOUND: SOFT PIANO, C4, 1.50 S");
        label(28.0f, 71.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, buf, true);
        char nb[8];
        noteName(int(std::lround(value(1))), nb, sizeof(nb));
        std::snprintf(buf, sizeof(buf), "%s, ROOT %s", value(0) >= 0.5f ? "CHROMATIC" : "ONE-SHOT", nb);
        label(612.0f, 71.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, buf, true);

        const float mid = kY0 + kH * 0.5f;
        box(kX0, kY0, kW, kH, theme::kBackground, 1.0f);
        const bool chrom = value(0) >= 0.5f, rev = value(16) >= 0.5f;
        // the loop's part (as the DSP: Chromatic only, at least 64 frames)
        const float ls = std::floor(value(13) * 10.0f + 0.5f) / 1000.0f, le = std::floor(value(14) * 10.0f + 0.5f) / 1000.0f;
        const float secs = p ? p->seconds.load(std::memory_order_relaxed) : 1.5f;
        const float rate = p ? p->fileRate.load(std::memory_order_relaxed) : 44100.0f;
        const bool loop = chrom && value(12) >= 0.5f && (le - ls) * secs * rate >= 64.0f;
        if (loop)
        {
            const float a = rev ? 1.0f - le : ls, b = rev ? 1.0f - ls : le;
            box(kX0 + kW * a, kY0, std::max(1.0f, kW * (b - a)), kH, theme::kAccent, 0.12f);
        }
        line(kX0, mid, kX0 + kW, mid, theme::kBorder, 1.0f);

        // the waveform (its peak at 90% of the height); before Start dimmed
        const float st = value(15) / 100.0f;
        const float sx = rev ? 1.0f - st : st;
        if (p)
        {
            float pk = 0.0f;
            for (int j = 0; j < nullsmp::kOverview; ++j)
                pk = std::fmax(pk, p->overview[j]);
            const float k = pk > 0.0f ? 0.9f / pk : 0.0f, w = kW / float(nullsmp::kOverview);
            for (int pass = 0; pass < 2; ++pass)
            {
                beginPath();
                for (int j = 0; j < nullsmp::kOverview; ++j)
                {
                    const float fx = float(j) / float(nullsmp::kOverview);
                    const bool on = rev ? fx <= sx : fx >= sx;
                    if (on != (pass == 1))
                        continue;
                    const float a = std::fmax(0.5f, p->overview[j] * k * kH * 0.5f);
                    rect(kX0 + float(j) * w, mid - a, w + 0.3f, 2.0f * a);
                }
                fillColor(col(theme::kAccent, pass == 1 ? 0.85f : 0.3f));
                fill();
            }
        }
        // Start, and the loop's ends
        const float xs = kX0 + kW * sx;
        line(xs, kY0, xs, kY0 + kH, theme::kText, 0.9f);
        label(xs + (rev ? -5.0f : 5.0f), kY0 + 7.0f, ALIGN_CENTER, 6.5f, 0.0f, theme::kText, "S", true);
        if (loop)
        {
            const float a = rev ? 1.0f - le : ls, b = rev ? 1.0f - ls : le;
            line(kX0 + kW * a, kY0, kX0 + kW * a, kY0 + kH, theme::kText, 0.5f);
            line(kX0 + kW * b, kY0, kX0 + kW * b, kY0 + kH, theme::kText, 0.5f);
        }
        // the voices that play
        int nv = 0;
        if (nulljsfx::Taps* tp = taps())
            for (int i = 0; i < nullsmp::kVoices; ++i)
            {
                const float x = tp->aux[i].load(std::memory_order_relaxed);
                if (x < 0.0f)
                    continue;
                ++nv;
                box(kX0 + kW * std::fmin(1.0f, x), kY0, 2.0f, kH, theme::kText, 0.8f);
            }
        beginPath();
        rect(kX0 + 0.5f, kY0 + 0.5f, kW - 1.0f, kH - 1.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        std::snprintf(buf, sizeof(buf), "%d / 16 VOICES", nv);
        label(28.0f, 206.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, buf, true);
        label(612.0f, 206.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, "CLICK THE WAVE TO HEAR THE ROOT KEY", true);
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    void line(float x0, float y0, float x1, float y1, uint32_t rgb, float alpha)
    {
        beginPath();
        moveTo(x0 + 0.5f, y0);
        lineTo(x1 + 0.5f, y1);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        switch (param)
        {
        case 1: noteName(int(std::lround(v)), buf, n); break;
        case 2: std::snprintf(buf, n, "%+d st", int(std::lround(v))); break;
        case 3: std::snprintf(buf, n, "%+d ct", int(std::lround(v))); break;
        case 4: case 5: case 7: case 11: msText(v, buf, n); break;
        case 8:
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
            break;
        case 10: std::snprintf(buf, n, "%+d%%", int(std::lround(v))); break;
        case 13: case 14: case 15: std::snprintf(buf, n, "%.1f%%", double(v)); break;
        case 18: std::snprintf(buf, n, "%d st", int(std::lround(v))); break;
        case 19: std::snprintf(buf, n, "%+.1f dB", double(v)); break;
        default: NullKnobsUI::formatValue(param, v, buf, n); break;
        }
    }

    // the base draws everything; then the FILE cell over its placeholder, and the preset list
    // again on top
    void onNanoDisplay() override
    {
        NullKnobsUI::onNanoDisplay();
        if (page() != 0)
            return;
        resetTransform();
        const float s = uiScale();
        scale(s, s);
        float cx, ry;
        cell(kFileCell, cx, ry);
        const float pw = cellW() - 10.0f;
        beginPath();
        roundedRect(cx - pw * 0.5f + 0.5f, ry + 33.5f, pw - 1.0f, 21.0f, 4.0f);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(fFileHover ? theme::kTrackHover : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        const bool ok = view() && view()->fileOk.load(std::memory_order_relaxed);
        label(cx, ry + 44.0f, ALIGN_CENTER, 8.0f, 0.5f, ok ? theme::kText : theme::kMuted,
              fFilePath.empty() ? "BUILT-IN" : fShort.c_str(), true);
        drawArrow(cx - pw * 0.5f + 7.0f, ry + 44.0f, -1.0f);
        drawArrow(cx + pw * 0.5f - 7.0f, ry + 44.0f, 1.0f);
#if DISTRHO_UI_FILE_BROWSER
        label(cx, ry + 77.0f, ALIGN_CENTER, 6.5f, 1.0f, theme::kMuted, "CLICK: BROWSE", true);
#endif
        drawPresetMenu();
    }

    // the previous (dir -1) or next (1) file of the folder(s), the built-in sound before the
    // first; the list is read again every time
    void stepFile(int dir)
    {
        std::vector<std::string> list = nullsmp::listSampleFiles();
        list.insert(list.begin(), std::string()); // the built-in sound
        int cur = 0;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i] == fFilePath)
                cur = int(i);
        const int n = int(list.size());
        const int next = (cur + dir + n) % n;
        setState("sample", list[size_t(next)].c_str());
        stateChanged("sample", list[size_t(next)].c_str());
    }

    // the file's name, upper case, without folder and extension (whole for the display, short
    // for the cell)
    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "sample") != 0)
            return;
        fFilePath = value ? value : "";
        std::string name = fFilePath;
        const size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos)
            name = name.substr(slash + 1);
        const size_t dot = name.find_last_of('.');
        if (dot != std::string::npos && dot > 0)
            name = name.substr(0, dot);
        for (char& c : name)
            c = char(std::toupper(static_cast<unsigned char>(c)));
        fFileName = name.size() > 40 ? name.substr(0, 38) + ".." : name;
        fShort = name.size() > 9 ? name.substr(0, 7) + ".." : name;
        repaint();
    }

    bool inFileCell(float x, float y) const { return page() == 0 && hit(kFileCell, x, y); }

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && inFileCell(x, y))
        {
            float cx, ry;
            cell(kFileCell, cx, ry);
            const float pw = cellW() - 10.0f;
            // the arrows' quarter at each end steps, the middle opens the file browser
            if (x < cx - pw * 0.25f)
                stepFile(-1);
            else if (x >= cx + pw * 0.25f)
                stepFile(1);
            else
            {
#if DISTRHO_UI_FILE_BROWSER
                requestStateFile("sample");
#else
                stepFile(1);
#endif
            }
            return true;
        }
        if (ev.button == 1 && ev.press && x >= kX0 && x < kX0 + kW && y >= kY0 && y < kY0 + kH)
        {
            if (nulljsfx::Taps* tp = taps())
                tp->request.store(1);
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const bool h = inFileCell(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s);
        if (h != fFileHover)
        {
            fFileHover = h;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        if (presetOpen())
            return true;
        const float s = uiScale();
        if (inFileCell(float(ev.pos.getX()) / s, float(ev.pos.getY()) / s))
        {
            const float d = float(ev.delta.getY()) + float(ev.delta.getX());
            if (d != 0.0f)
                stepFile(d > 0.0f ? 1 : -1);
            return true;
        }
        return NullKnobsUI::onScroll(ev);
    }

private:
    std::string fFilePath, fFileName, fShort;
    bool fFileHover = false;
};

UI* createUI()
{
    return new UISampler();
}

END_NAMESPACE_DISTRHO
