/*
 * NULL JSFX - Filter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "FilterDSP.hpp"
#include "FilterSliders.hpp"

START_NAMESPACE_DISTRHO

// as filter_gfx.jsfx: tab FILTER (cutoff, resonance, gain, mode, slope, drive) and tab
// MODULATION (LFO rate, sync, depth, envelope depth, attack, release)
static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "CUTOFF", 0, 0, false, true },
    { 1, NullKnobsUI::kKnob, "RESONANCE", 0, 0, false },
    { 2, NullKnobsUI::kKnob, "GAIN", 0, 0, true },
    { 3, NullKnobsUI::kSelect, "MODE", 0, 0, false },
    { 4, NullKnobsUI::kSelect, "SLOPE", 0, 0, false },
    { 5, NullKnobsUI::kKnob, "DRIVE", 0, 0, false },
    { 6, NullKnobsUI::kKnob, "LFO RATE", 0, 1, false, true },
    { 7, NullKnobsUI::kSelect, "LFO SYNC", 0, 1, false },
    { 8, NullKnobsUI::kKnob, "LFO DEPTH", 0, 1, false },
    { 9, NullKnobsUI::kKnob, "ENV DEPTH", 0, 1, true },
    { 10, NullKnobsUI::kKnob, "ATTACK", 0, 1, false },
    { 11, NullKnobsUI::kKnob, "RELEASE", 0, 1, false, true },
};

class UIFilter : public NullKnobsUI
{
public:
    UIFilter()
        : NullKnobsUI({ "FILTER", "ESSENTIALS", 480, 120, "FILTER|MODULATION", 1, nullfilter::kSliders, nullfilter::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in filter_gfx.jsfx: the filter's magnitude response with Gain (-24..+24 dB), cutoff dashed;
    // with the LFO or the envelope on, the range they sweep dashed faintly and the response at
    // the moving cutoff on top
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, fVW - 30.0f, fVH - 20.0f);
        gridFreq(36.0f);
        const float fc = value(0), lfo = value(8), env = value(9);
        const bool modOn = lfo > 0.0f || std::fabs(env) >= 0.05f;
        if (lfo > 0.0f)
        {
            dashedV(freqX(fc * std::pow(2.0f, -lfo)), 0.25f);
            dashedV(freqX(fc * std::pow(2.0f, lfo)), 0.25f);
        }
        if (std::fabs(env) >= 0.05f)
            dashedV(freqX(fc * std::pow(2.0f, env)), 0.25f);
        dashedV(freqX(fc));

        const int sync = int(std::lround(value(7)));
        char a[24] = "", b[24] = "", line[64];
        if (lfo > 0.0f)
        {
            if (sync > 0)
                std::snprintf(a, sizeof(a), "LFO %s %.1f OCT", nullfilter::kSyncOptions[std::min(sync, 9)], double(lfo));
            else
                std::snprintf(a, sizeof(a), "LFO %.2f HZ %.1f OCT", double(value(6)), double(lfo));
        }
        if (std::fabs(env) >= 0.05f)
            std::snprintf(b, sizeof(b), "ENV %+.1f OCT", double(env));
        std::snprintf(line, sizeof(line), "%s%s%s%s", value(4) >= 0.5f ? "24 DB" : "12 DB", a[0] ? "  " : "", a, b[0] ? "  " : "");
        std::strncat(line, b, sizeof(line) - std::strlen(line) - 1);
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, line);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        constexpr int n = 404;
        float xs[n], ys[n];
        nullfilter::Coefs k;
        k.compute(value(0), value(1), value(2), int(std::lround(value(3))), sr);
        k.slope24 = value(4) >= 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(k.magnitudeDb(f, sr)), -24.0f, 24.0f);
        }
        curve(xs, ys, n, linY(0.0f, -24.0f, 24.0f));

        if (modOn)
        {
            float fm = fc;
            if (nulljsfx::Taps* t = taps())
                fm = t->extra.load(std::memory_order_relaxed);
            if (!(fm >= 20.0f))
                fm = fc;
            k.compute(fm, value(1), value(2), int(std::lround(value(3))), sr);
            k.slope24 = value(4) >= 0.5f;
            beginPath();
            for (int i = 0; i < n; ++i)
            {
                const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
                const float y = linY(float(k.magnitudeDb(f, sr)), -24.0f, 24.0f);
                if (i == 0)
                    moveTo(xs[i], y);
                else
                    lineTo(xs[i], y);
            }
            strokeColor(col(theme::kText, 0.55f));
            strokeWidth(1.0f);
            stroke();
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        }
        else if (param == 1)
            std::snprintf(buf, n, "%.1f", double(v));
        else if (param == 2)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else if (param == 6)
            std::snprintf(buf, n, "%.2f Hz", double(v));
        else if (param == 8)
            std::snprintf(buf, n, "%.1f oct", double(v));
        else if (param == 9)
        {
            if (std::fabs(v) < 0.05f)
                std::snprintf(buf, n, "0.0 oct");
            else
                std::snprintf(buf, n, "%+.1f oct", double(v));
        }
        else if (param == 10 || param == 11)
            std::snprintf(buf, n, "%d ms", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UIFilter();
}

END_NAMESPACE_DISTRHO
