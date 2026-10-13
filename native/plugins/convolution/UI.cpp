/*
 * NULL JSFX - Convolution (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ConvolutionShared.hpp"
#include "ConvolutionSliders.hpp"
#include "ConvolutionFiles.hpp"

#include <cctype>
#include <string>

START_NAMESPACE_DISTRHO

// control 1 is the IR FILE cell: it holds the place in the row (the base class draws a
// selector for param 0 there) and this UI draws over it. Like the GFX's cell (the JSFX file
// slider), its left half picks the previous .wav in the null_jsfx_ir folder(s), the right half
// the next one; the choice is the "irfile" state (a path), which the host saves
static constexpr uint32_t kFileCell = 1;

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "SOURCE", 0, -1, false },
    { 0, NullKnobsUI::kSelect, "IR FILE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "LENGTH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "PRE-DELAY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "LOW CUT", 1, -1, false },
    { 4, NullKnobsUI::kKnob, "HIGH CUT", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, false },
};

class UIConvolution : public NullKnobsUI
{
public:
    UIConvolution()
        : NullKnobsUI({ "CONVOLUTION", "ADVANCED", 480, 120, nullptr, 2, nullconv::kSliders, nullconv::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    ConvolutionPluginBase* plugin() const
    {
        return static_cast<ConvolutionPluginBase*>(getPluginInstancePointer());
    }

    // as in convolution_gfx.jsfx: the IR's envelope over 3 s (0 to -60 dB from its peak)
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        grid(6, 4, false);
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-30");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-60");
        label(fPX + fPW / 3.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1 s");
        label(fPX + fPW * 2.0f / 3.0f, fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "2 s");
        label(fPX + fPW, fPY + fPH + 10.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "3 s");
        ConvolutionPluginBase* p = plugin();
        if (!p)
            return;
        const float secs = p->irSeconds.load(std::memory_order_relaxed);
        char buf[32];
        if (std::lround(value(0)) == nullconv::kFileSource && secs <= 0.0f)
            std::snprintf(buf, sizeof(buf), "NO FILE");
        else
            std::snprintf(buf, sizeof(buf), "IR %.2f s", double(secs));
        label(fPX + fPW, fVY + 4.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);

        float pk = 0.0f;
        for (int j = 0; j < nullconv::kEnvBins; ++j)
            pk = std::fmax(pk, p->envelope[j]);
        if (pk <= 0.0f)
            return;
        const float bottom = fPY + fPH, w = fPW / float(nullconv::kEnvBins);
        beginPath();
        for (int j = 0; j < nullconv::kEnvBins; ++j)
        {
            const float y = linY(20.0f * std::log10(std::fmax(p->envelope[j] / pk, 0.000001f)), -60.0f, 0.0f);
            if (y < bottom - 1.0f)
                rect(fPX + w * float(j), y, w + 0.5f, bottom - y);
        }
        fillColor(col(theme::kAccent, 0.85f));
        fill();
    }

    // the base draws everything; then the IR FILE cell on top of its placeholder, and the preset
    // list again over it
    void onNanoDisplay() override
    {
        NullKnobsUI::onNanoDisplay();
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
        const bool active = std::lround(value(0)) == nullconv::kFileSource;
        label(cx, ry + 44.0f, ALIGN_CENTER, 8.0f, 0.5f, active ? theme::kText : theme::kMuted,
              fFileName.empty() ? "NONE" : fFileName.c_str(), true);
        drawArrow(cx - pw * 0.5f + 7.0f, ry + 44.0f, -1.0f);
        drawArrow(cx + pw * 0.5f - 7.0f, ry + 44.0f, 1.0f);
        drawPresetMenu();
    }

    // previous (dir -1) or next (1) IR file, wrapping; the list is read again every time
    void stepFile(int dir)
    {
        const std::vector<std::string> files = nullconv::listIrFiles();
        if (files.empty())
        {
            fFileName = "NONE";
            repaint();
            return;
        }
        int cur = -1;
        for (size_t i = 0; i < files.size(); ++i)
            if (files[i] == fFilePath)
                cur = int(i);
        const int n = int(files.size());
        const int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
        setState("irfile", files[size_t(next)].c_str());
        stateChanged("irfile", files[size_t(next)].c_str());
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 4 && v >= 10000.0f)
            std::snprintf(buf, n, "%.1f kHz", double(v) / 1000.0);
        else if (param == 6)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    // the file's name, upper case, without folder and extension, shortened to fit the cell
    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "irfile") != 0)
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
        if (name.size() > 8)
            name = name.substr(0, 6) + "..";
        fFileName = name;
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
            stepFile(x < cx ? -1 : 1);
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
    std::string fFileName, fFilePath;
    bool fFileHover = false;
};

UI* createUI()
{
    return new UIConvolution();
}

END_NAMESPACE_DISTRHO
