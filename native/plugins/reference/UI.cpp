/*
 * NULL JSFX - Reference (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "ReferencePlugin.hpp"

#include <chrono>

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "LISTEN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "LEVEL MATCH", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "TRIM", 0, -1, true },
    { 3, NullKnobsUI::kSelect, "MONITOR", 0, -1, false },
};

class UIReference : public NullKnobsUI
{
public:
    UIReference()
        : NullKnobsUI({ "REFERENCE", "ADVANCED", 480, 160, nullptr, 1, nullref::kSliders, nullref::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    ReferenceBase* plugin() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        return static_cast<ReferenceBase*>(static_cast<Plugin*>(getPluginInstancePointer()));
#else
        return nullptr;
#endif
    }

    static void lufsText(char* buf, size_t n, float l)
    {
        if (l > -70.0f)
            std::snprintf(buf, n, "%.1f LUFS", double(l));
        else
            std::snprintf(buf, n, "-INF");
    }

    // as in reference_gfx.jsfx: left, the spectra (track filled, reference as a line, each bin
    // averaged over about a second); right, the loudnesses, the reference's gain, the correlations
    void drawDisplay() override
    {
        setPlot(fVX + 26.0f, fVY + 4.0f, 270.0f, fVH - 20.0f);
        ReferenceBase* p = plugin();
        if (p)
        {
            const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            fTrack.update(p->analyzer, false, now, true);
            fRef.update(p->analyzer, true, now, true);
        }
        gridFreq(36.0f);
        constexpr int cols = 270;
        float xs[cols], yt[cols], yr[cols];
        for (int c = 0; c < cols; ++c)
        {
            const double f0 = 20.0 * std::pow(1000.0, double(c) / cols), f1 = 20.0 * std::pow(1000.0, double(c + 1) / cols);
            xs[c] = fPX + fPW * (float(c) + 0.5f) / float(cols);
            yt[c] = linY(float(fTrack.bandDb(f0, f1, true)), -90.0f, 0.0f);
            yr[c] = linY(float(fRef.bandDb(f0, f1, true)), -90.0f, 0.0f);
        }
        scissor(fPX, fPY, fPW, fPH);
        beginPath();
        moveTo(fPX, fPY + fPH);
        for (int c = 0; c < cols; ++c)
            lineTo(xs[c], yt[c]);
        lineTo(fPX + fPW, fPY + fPH);
        closePath();
        fillColor(col(theme::kMuted, 0.25f));
        fill();
        beginPath();
        moveTo(xs[0], yr[0]);
        for (int c = 1; c < cols; ++c)
            lineTo(xs[c], yr[c]);
        strokeColor(col(theme::kAccent));
        strokeWidth(1.0f);
        stroke();
        resetScissor();
        label(fVX + 34.0f, fVY + 12.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kMuted, "TRACK", true);
        label(fVX + 70.0f, fVY + 12.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kAccent, "REFERENCE", true);

        const bool ref = value(0) >= 0.5f;
        const float tx = fVX + 318.0f, ty = fVY + 18.0f;
        char buf[32];
        label(tx, ty, ALIGN_LEFT, 7.5f, 1.0f, ref ? theme::kMuted : theme::kText, "TRACK", true);
        lufsText(buf, sizeof(buf), p ? p->trackLufs.load(std::memory_order_relaxed) : -200.0f);
        label(tx + 118.0f, ty, ALIGN_RIGHT, 7.5f, 0.5f, ref ? theme::kMuted : theme::kText, buf, true);
        label(tx, ty + 22.0f, ALIGN_LEFT, 7.5f, 1.0f, ref ? theme::kText : theme::kMuted, "REFERENCE", true);
        lufsText(buf, sizeof(buf), p ? p->referenceLufs.load(std::memory_order_relaxed) : -200.0f);
        label(tx + 118.0f, ty + 22.0f, ALIGN_RIGHT, 7.5f, 0.5f, ref ? theme::kText : theme::kMuted, buf, true);
        label(tx, ty + 44.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, "GAIN", true);
        std::snprintf(buf, sizeof(buf), "%+.1f dB", 20.0 * std::log10(std::max(1e-9f, p ? p->referenceGain.load(std::memory_order_relaxed) : 1.0f)));
        label(tx + 118.0f, ty + 44.0f, ALIGN_RIGHT, 7.5f, 0.5f, theme::kAccent, buf, true);
        label(tx, ty + 66.0f, ALIGN_LEFT, 7.5f, 1.0f, theme::kMuted, "CORRELATION", true);
        std::snprintf(buf, sizeof(buf), "%.2f / %.2f", p ? double(p->trackCorr.load(std::memory_order_relaxed)) : 0.0,
                      p ? double(p->referenceCorr.load(std::memory_order_relaxed)) : 0.0);
        label(tx + 118.0f, ty + 88.0f, ALIGN_RIGHT, 7.5f, 0.5f, theme::kText, buf, true);
        label(tx, ty + 88.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kMuted, "TRACK / REFERENCE", true);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 2)
            std::snprintf(buf, n, "%+.1f dB", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    nulleqan::Analyzer fTrack, fRef;
};

UI* createUI()
{
    return new UIReference();
}

END_NAMESPACE_DISTRHO
