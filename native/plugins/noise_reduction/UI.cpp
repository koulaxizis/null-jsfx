/*
 * NULL JSFX - Noise Reduction (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "NoiseReductionSliders.hpp"
#include "NoiseFile.hpp"

#include <cctype>
#include <string>

START_NAMESPACE_DISTRHO

// 4.2: the cells drawn here over a placeholder: the Noise File (steps through the files of
// null_jsfx_noise, like Convolution's IR File) and Clear (Use Noise File Off)
static constexpr uint32_t kFileCell = 9, kClearCell = 11;

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LEARN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "AUTO", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "REDUCTION", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "THRESHOLD", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "SMOOTHING", 0, -1, false },
    { 8, NullKnobsUI::kKnob, "MIX", 0, -1, false },
    { 5, NullKnobsUI::kKnob, "ATTACK", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "RELEASE", 1, -1, false },
    { 7, NullKnobsUI::kSelect, "LISTEN", 1, -1, false },
    { 0, NullKnobsUI::kSelect, "NOISE FILE", 1, -1, false },
    { 9, NullKnobsUI::kSelect, "USE FILE", 1, -1, false },
    { 0, NullKnobsUI::kSelect, "PROFILE", 1, -1, false },
};

class UINoiseReduction : public NullKnobsUI
{
public:
    UINoiseReduction()
        : NullKnobsUI({ "NOISE REDUCTION", "ADVANCED", 480, 150, nullptr, 2, nullnr::kSliders, nullnr::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kCols = 200;

    const nullnr::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<NoiseReductionBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    // as in noise_reduction_gfx.jsfx: the input spectrum (grey area), the noise estimate in use
    // (thin bright line) and the gain per bin (silver, hanging from 0 dB: the reduction curve), on
    // a log-frequency axis; 0 dB = a full-scale sine, down to -96 dB
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 14.0f, fVW - 30.0f, fVH - 30.0f);
        grid(1, 4, false);
        strokeWidth(1.0f);
        for (float f = 100.0f; f <= 10000.0f; f *= 10.0f)
        {
            const float x = std::floor(freqX(f)) + 0.5f;
            beginPath();
            moveTo(x, fPY);
            lineTo(x, fPY + fPH);
            strokeColor(col(theme::kBorder, 0.6f));
            stroke();
        }
        label(fPX - 6.0f, fPY, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "0");
        label(fPX - 6.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-48");
        label(fPX - 6.0f, fPY + fPH, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "-96");
        label(freqX(100.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "100");
        label(freqX(1000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "1k");
        label(freqX(10000.0f), fPY + fPH + 10.0f, ALIGN_CENTER, 8.0f, 0.0f, theme::kMuted, "10k");

        const nullnr::View* v = view();
        const bool autoOn = value(1) >= 0.5f;
        const bool learning = v && v->mode.load(std::memory_order_relaxed) == 1;
        const float learnt = v ? v->learnt.load(std::memory_order_relaxed) : 0.0f;
        const int src = v ? v->source.load(std::memory_order_relaxed) : 0;
        const bool hasNoise = autoOn || src > 0;

        // status on the left, legend on the right, above the plot: the profile learned live,
        // from the noise file (first 60 s only of a longer file) or none
        char buf[64];
        if (autoOn)
            std::snprintf(buf, sizeof(buf), "AUTO");
        else if (learning)
            std::snprintf(buf, sizeof(buf), "LEARNING %.1f S", double(learnt));
        else if (src == 1)
            std::snprintf(buf, sizeof(buf), v->cut.load(std::memory_order_relaxed) ? "PROFILE: FROM FILE %.1f S (FIRST 60 S)"
                                                                                   : "PROFILE: FROM FILE %.1f S",
                          double(learnt));
        else if (src == 2)
            std::snprintf(buf, sizeof(buf), "PROFILE: LEARNED %.1f S", double(learnt));
        else if (value(9) >= 0.5f && v && v->fileFailed.load(std::memory_order_relaxed))
            std::snprintf(buf, sizeof(buf), "PROFILE: NONE (FILE NOT READABLE)");
        else
            std::snprintf(buf, sizeof(buf), "PROFILE: NONE (LEARN, FILE OR AUTO)");
        const float ty = fVY + 4.0f;
        label(fPX, ty, ALIGN_LEFT, 8.0f, 0.0f, learning ? theme::kText : theme::kMuted, buf);
        float x = fPX + fPW;
        label(x, ty, ALIGN_RIGHT, 8.0f, 0.0f, theme::kAccent, "REDUCTION");
        x -= textWidth("REDUCTION") + 12.0f;
        label(x, ty, ALIGN_RIGHT, 8.0f, 0.0f, theme::kText, "NOISE");
        x -= textWidth("NOISE") + 12.0f;
        label(x, ty, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "SPECTRUM");
        if (!v)
            return;

        // three passes over 201 columns: the loudest bin under each column (spectrum, noise) or
        // the lowest gain
        const double sr = v->sampleRate.load(std::memory_order_relaxed);
        const int nb = nullnr::kNB, n = nullnr::kN;
        static float xs[kCols + 1], ys[kCols + 1];
        for (int pass = 0; pass < 3; ++pass)
        {
            const float* src = pass == 0 ? v->spec : pass == 1 ? v->noise : v->gain;
            for (int j = 0; j <= kCols; ++j)
            {
                const int b0 = std::min(nb - 1, int(std::floor(20.0 * std::pow(1000.0, j / 200.0) * n / sr)));
                const int b1 = std::max(b0, std::min(nb - 1, int(std::floor(20.0 * std::pow(1000.0, (j + 1) / 200.0) * n / sr))));
                float m = src[b0];
                for (int b = b0 + 1; b <= b1; ++b)
                    m = pass == 2 ? std::fmin(m, src[b]) : std::fmax(m, src[b]);
                const float db = pass == 2 ? 20.0f * std::log10(std::fmax(m, 0.0000001f))
                                           : 20.0f * std::log10(std::fmax(m / (n / 4.0f), 0.0000001f));
                xs[j] = fPX + fPW * float(j) / 200.0f;
                ys[j] = linY(db, -96.0f, 0.0f);
            }
            if (pass == 1 && !hasNoise)
                continue;
            scissor(fPX, fPY, fPW, fPH);
            if (pass != 1)
            {
                // the spectrum shaded up from the bottom, the reduction down from the top
                const float edge = pass == 0 ? fPY + fPH : fPY;
                beginPath();
                moveTo(xs[0], edge);
                for (int j = 0; j <= kCols; ++j)
                    lineTo(xs[j], ys[j]);
                lineTo(xs[kCols], edge);
                closePath();
                fillColor(pass == 0 ? col(theme::kMuted, 0.18f) : col(theme::kAccent, 0.12f));
                fill();
            }
            beginPath();
            moveTo(xs[0], ys[0]);
            for (int j = 1; j <= kCols; ++j)
                lineTo(xs[j], ys[j]);
            strokeColor(pass == 0 ? col(theme::kMuted, 0.8f) : pass == 1 ? col(theme::kText, 0.9f) : col(theme::kAccent));
            strokeWidth(pass == 2 ? 2.0f : 1.0f);
            lineJoin(ROUND);
            stroke();
            resetScissor();
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 3)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

    // the noise file cell (the file's name) and Clear, drawn over their placeholders
    void onNanoDisplay() override
    {
        NullKnobsUI::onNanoDisplay();
        resetTransform();
        const float s = uiScale();
        scale(s, s);
        const bool on = value(9) >= 0.5f;
        for (uint32_t c : { kFileCell, kClearCell })
        {
            float cx, ry;
            cell(c, cx, ry);
            const float pw = cellW(c) - 10.0f;
            const bool hover = c == kFileCell ? fFileHover : (fClearHover && on);
            beginPath();
            roundedRect(cx - pw * 0.5f + 0.5f, ry + 33.5f, pw - 1.0f, 21.0f, 4.0f);
            fillColor(col(theme::kBackground));
            fill();
            strokeColor(col(hover ? (c == kClearCell ? theme::kAccent : theme::kTrackHover) : theme::kBorder));
            strokeWidth(1.0f);
            stroke();
            if (c == kFileCell)
            {
                label(cx, ry + 44.0f, ALIGN_CENTER, 8.0f, 0.5f, on ? theme::kText : theme::kMuted,
                      fFileName.empty() ? "NONE" : fFileName.c_str(), true);
                drawArrow(cx - pw * 0.5f + 7.0f, ry + 44.0f, -1.0f);
                drawArrow(cx + pw * 0.5f - 7.0f, ry + 44.0f, 1.0f);
            }
            else
                label(cx, ry + 44.0f, ALIGN_CENTER, 8.0f, 0.5f, on ? theme::kText : theme::kMuted, "CLEAR", true);
        }
        drawPresetMenu();
    }

    void stepFile(int dir)
    {
        const std::vector<std::string> files = nullnr::listNoiseFiles();
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
        setState("noisefile", files[size_t(next)].c_str());
        stateChanged("noisefile", files[size_t(next)].c_str());
    }

    // Clear: Use Noise File Off, which empties the profile
    void clearProfile()
    {
        if (value(9) < 0.5f)
            return;
        histTouch();
        editParameter(9, true);
        presetSet(9, 0.0f);
        editParameter(9, false);
        histEnd();
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "noisefile") != 0)
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

    bool onMouse(const MouseEvent& ev) override
    {
        if (presetMouse(ev))
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && hit(kFileCell, x, y))
        {
            float cx, ry;
            cell(kFileCell, cx, ry);
            stepFile(x < cx ? -1 : 1);
            return true;
        }
        if (ev.button == 1 && hit(kClearCell, x, y))
        {
            if (ev.press)
                clearProfile();
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        const bool hf = hit(kFileCell, x, y), hc = hit(kClearCell, x, y);
        if (hf != fFileHover || hc != fClearHover)
        {
            fFileHover = hf;
            fClearHover = hc;
            repaint();
        }
        return NullKnobsUI::onMotion(ev);
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        if (presetOpen())
            return true;
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (hit(kFileCell, x, y))
        {
            const float d = float(ev.delta.getY()) + float(ev.delta.getX());
            if (d != 0.0f)
                stepFile(d > 0.0f ? 1 : -1);
            return true;
        }
        if (hit(kClearCell, x, y))
            return true;
        return NullKnobsUI::onScroll(ev);
    }

    // width of a plot label (8 pt Noto Sans) in design units
    float textWidth(const char* s)
    {
        fontSize(8.0f * kNotoLineToEm);
        textLetterSpacing(0.0f);
        Rectangle<float> r;
        return textBounds(0.0f, 0.0f, s, nullptr, r);
    }

private:
    std::string fFileName, fFilePath;
    bool fFileHover = false, fClearHover = false;
};

UI* createUI()
{
    return new UINoiseReduction();
}

END_NAMESPACE_DISTRHO
