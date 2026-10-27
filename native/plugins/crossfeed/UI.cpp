/*
 * NULL JSFX - Crossfeed (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "CrossfeedDSP.hpp"
#include "CrossfeedSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "CUTOFF", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "FEED", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "ANGLE", 0, -1, false },
    { 4, NullKnobsUI::kKnob, "ROOM", 1, -1, false },
    { 5, NullKnobsUI::kKnob, "MIX", 1, -1, false },
    { 6, NullKnobsUI::kKnob, "OUTPUT", 1, -1, true },
};

class UICrossfeed : public NullKnobsUI
{
public:
    UICrossfeed()
        : NullKnobsUI({ "CROSSFEED", "ESSENTIALS", 480, 120, nullptr, 2, nullxfeed::kSliders, nullxfeed::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    // as in crossfeed_gfx.jsfx: what each ear gets from its own side (accent) and the other side
    // (muted), -24 .. +24 dB, and the mode's numbers at the top left
    void drawDisplay() override
    {
        setPlot(fVX + 34.0f, fVY + 6.0f, fVW - 40.0f, fVH - 24.0f);
        gridFreq(36.0f);
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        double v[nullxfeed::kNumSliders];
        for (uint32_t i = 0; i < nullxfeed::kNumSliders; ++i)
            v[i] = nullxfeed::sliderValue(i, value(i));
        const nullxfeed::Params p = nullxfeed::params(v);
        const nullxfeed::Coefs c = nullxfeed::coefs(p, sr);
        const int n = int(fPW) + 1;
        scissor(fPX, fPY, fPW, fPH);
        for (int which = 1; which >= 0; --which)
        {
            beginPath();
            for (int i = 0; i < n; ++i)
            {
                const double f = 20.0 * std::pow(1000.0, double(i) / double(fPW));
                const float y = linY(float(c.pathDb(which, f, sr)), -24.0f, 24.0f);
                if (i == 0)
                    moveTo(fPX, y);
                else
                    lineTo(fPX + float(i), y);
            }
            strokeColor(col(which ? theme::kMuted : theme::kAccent));
            strokeWidth(2.0f);
            lineJoin(ROUND);
            stroke();
        }
        resetScissor();
        char buf[96];
        if (p.speakers)
            std::snprintf(buf, sizeof(buf), "SPEAKERS +-%d DEG   ITD %.2f ms   ROOM %d%%", int(std::lround(p.angle)),
                          c.itd / sr * 1000.0, int(std::lround(p.room)));
        else
            std::snprintf(buf, sizeof(buf), "BS2B   %d Hz   %.1f dB", int(std::lround(p.cutoff)), p.feed);
        label(fPX + 6.0f, fPY + 9.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        label(fPX + fPW - 70.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kAccent, "OWN SIDE");
        label(fPX + fPW - 6.0f, fPY + 9.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "OTHER SIDE");
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
        else if (param == 2)
            std::snprintf(buf, n, "%.1f dB", double(v));
        else if (param == 3)
            std::snprintf(buf, n, "%d deg", int(std::lround(v)));
        else if (param == 6)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UICrossfeed();
}

END_NAMESPACE_DISTRHO
