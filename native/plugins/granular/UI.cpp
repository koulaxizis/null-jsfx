/*
 * NULL JSFX - Granular (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As granular_gfx.jsfx: the sound's waveform with Position, the Spray around it and the grains
 * that play. The FILE cell works as in the Sampler: its arrows step through the .wav/.flac files
 * of the null_jsfx_samples folder(s) and the built-in sound, its middle opens a file browser for
 * any WAV or FLAC file. The choice is the "sample" state (a path), which the host saves with
 * the project.
 */

#include "NullUI.hpp"
#include "GranularSliders.hpp"
#include "../sampler/SamplerFiles.hpp"

#include <cctype>
#include <string>

START_NAMESPACE_DISTRHO

// control 0 is the FILE cell: it holds the place in the row and this UI draws over it
static constexpr uint32_t kFileCell = 0;

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "FILE", 0, -1, false },
    { 0, NullKnobsUI::kKnob, "POSITION", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SPRAY", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "SCAN", 0, -1, true },
    { 8, NullKnobsUI::kKnob, "ROOT", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "SIZE", 1, -1, false, true },
    { 4, NullKnobsUI::kKnob, "DENSITY", 1, -1, false, true },
    { 5, NullKnobsUI::kKnob, "PITCH SPREAD", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "STEREO", 1, -1, false },
    { 7, NullKnobsUI::kKnob, "REVERSE", 1, -1, false },
    { 9, NullKnobsUI::kKnob, "ATTACK", 2, -1, false, true },
    { 10, NullKnobsUI::kKnob, "RELEASE", 2, -1, false, true },
    { 11, NullKnobsUI::kKnob, "VELOCITY", 2, -1, false },
    { 12, NullKnobsUI::kKnob, "VOLUME", 2, -1, true },
};

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

class UIGranular : public NullKnobsUI
{
public:
    UIGranular()
        : NullKnobsUI({ "GRANULAR", "INSTRUMENTS", 560, 150, nullptr, 3, nullgrn::kSliders, nullgrn::kNumParams,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    GranularPluginBase* view() const { return static_cast<GranularPluginBase*>(getPluginInstancePointer()); }

    static constexpr float kX0 = 28.0f, kW = 504.0f, kY0 = 82.0f, kH = 106.0f;

    void drawDisplay() override
    {
        GranularPluginBase* p = view();
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
            std::snprintf(buf, sizeof(buf), "BUILT-IN SOUND: ORGAN TONE, C4, 2.00 S");
        label(28.0f, 71.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, buf, true);

        int ng = 0;
        if (p)
            for (int i = 0; i < nullgrn::kGrains; ++i)
                if (p->grainPos[i] >= 0.0f)
                    ++ng;
        char nb[8];
        noteName(int(std::lround(value(8))), nb, sizeof(nb));
        std::snprintf(buf, sizeof(buf), ng == 1 ? "ROOT %s, %d GRAIN" : "ROOT %s, %d GRAINS", nb, ng);
        label(kX0 + kW, 71.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, buf, true);

        const float mid = kY0 + kH * 0.5f;
        box(kX0, kY0, kW, kH, theme::kBackground, 1.0f);
        // the Spray around Position (wrapping at the ends)
        const float pos0 = float(jsfxValue(nullgrn::kSliders[0], value(0)) / 100.0);
        const float spray = float(jsfxValue(nullgrn::kSliders[1], value(1)) / 200.0);
        const float a = pos0 - spray, b = pos0 + spray;
        box(kX0 + kW * std::fmax(0.0f, a), kY0, std::fmax(1.0f, kW * (std::fmin(1.0f, b) - std::fmax(0.0f, a))), kH,
            theme::kAccent, 0.10f);
        if (a < 0.0f)
            box(kX0 + kW * (1.0f + a), kY0, std::fmax(1.0f, kW * -a), kH, theme::kAccent, 0.10f);
        if (b > 1.0f)
            box(kX0, kY0, std::fmax(1.0f, kW * (b - 1.0f)), kH, theme::kAccent, 0.10f);
        line(kX0, mid, kX0 + kW, mid, theme::kBorder, 1.0f);

        // the waveform (its peak at 90% of the height)
        if (p)
        {
            float pk = 0.0f;
            for (int j = 0; j < nullgrn::kOverview; ++j)
                pk = std::fmax(pk, p->overview[j]);
            const float k = pk > 0.0f ? 0.9f / pk : 0.0f, w = kW / float(nullgrn::kOverview);
            beginPath();
            for (int j = 0; j < nullgrn::kOverview; ++j)
            {
                const float h = std::fmax(0.5f, p->overview[j] * k * kH * 0.5f);
                rect(kX0 + float(j) * w, mid - h, w + 0.3f, 2.0f * h);
            }
            fillColor(col(theme::kMuted, 0.8f));
            fill();
        }
        // Position
        line(kX0 + kW * pos0, kY0, kX0 + kW * pos0, kY0 + kH, theme::kAccent, 1.0f);
        // the grains
        if (p)
            for (int i = 0; i < nullgrn::kGrains; ++i)
            {
                const float x = p->grainPos[i];
                if (x < 0.0f)
                    continue;
                const float y = kY0 + 10.0f + float(i % 8) * (kH - 20.0f) / 7.0f;
                box(kX0 + kW * std::fmin(1.0f, x) - 1.0f, y - 3.0f, 2.0f, 6.0f, theme::kText, 0.25f + 0.75f * p->grainWin[i]);
            }
        beginPath();
        rect(kX0 + 0.5f, kY0 + 0.5f, kW - 1.0f, kH - 1.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
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
        const double x = jsfxValue(nullgrn::kSliders[param], v);
        switch (param)
        {
        case 0: case 1: std::snprintf(buf, n, "%.1f%%", x); break;
        case 2: std::snprintf(buf, n, "%+.1f%%/s", x); break;
        case 3: std::snprintf(buf, n, "%d ms", int(x)); break;
        case 4: std::snprintf(buf, n, "%.1f/s", x); break;
        case 5: std::snprintf(buf, n, "%.1f st", x); break;
        case 6: case 7: case 11: std::snprintf(buf, n, "%d%%", int(x)); break;
        case 8: noteName(int(x), buf, n); break;
        case 9: case 10: msText(float(x), buf, n); break;
        case 12: std::snprintf(buf, n, "%+.1f dB", x); break;
        default: NullKnobsUI::formatValue(param, v, buf, n); break;
        }
    }

    // the base draws everything; then the FILE cell over its placeholder, and the preset list
    // again on top
    void onNanoDisplay() override
    {
        NullKnobsUI::onNanoDisplay();
        resetTransform();
        const float s = uiScale();
        scale(s, s);
        float cx, ry;
        cell(kFileCell, cx, ry);
        const float pw = cellW() - 10.0f;
        // cover the placeholder's value (its label FILE stays)
        beginPath();
        rect(cx - pw * 0.5f - 2.0f, ry + 22.0f, pw + 4.0f, 60.0f);
        fillColor(col(theme::kCard));
        fill();
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

    bool inFileCell(float x, float y) const { return hit(kFileCell, x, y); }

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
    return new UIGranular();
}

END_NAMESPACE_DISTRHO
