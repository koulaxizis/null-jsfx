/*
 * NULL JSFX - Spectrum Analyzer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "SpectrumAnalyzerSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "FFT SIZE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "SLOPE", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "RELEASE", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "RANGE", 0, -1, false },
    { 4, NullKnobsUI::kSelect, "PEAK HOLD", 1, -1, false },
    { 5, NullKnobsUI::kSelect, "CHANNEL", 1, -1, false },
    { 6, NullKnobsUI::kSelect, "MONITOR", 1, -1, false },
};

class UISpectrumAnalyzer : public NullKnobsUI
{
public:
    UISpectrumAnalyzer()
        : NullKnobsUI({ "SPECTRUM ANALYZER", "ADVANCED", 640, 280, nullptr, 2, nullspec::kSliders, nullspec::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    static constexpr int kCols = 558;

    const nullspec::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<SpectrumAnalyzerBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    // as the @gfx of spectrum_analyzer_gfx.jsfx: 20 Hz to 20 kHz (log), 0 dB down to -Range
    void drawDisplay() override
    {
        const int ri = int(std::lround(value(3)));
        const float range = ri == 0 ? 60.0f : ri == 1 ? 80.0f : ri == 2 ? 100.0f : 120.0f;
        const float dbs = range <= 80.0f ? 10.0f : 20.0f;
        setPlot(52.0f, 66.0f, 558.0f, 242.0f);
        char buf[16];
        strokeWidth(1.0f);
        for (float d = 10.0f; d <= 10000.0f; d *= 10.0f)
            for (int m = 1; m <= 9; ++m)
            {
                const float f = float(m) * d;
                if (f < 20.0f || f > 20000.0f)
                    continue;
                const float x = std::round(freqX(f)) + 0.5f;
                beginPath();
                moveTo(x, fPY);
                lineTo(x, fPY + fPH);
                strokeColor(col(theme::kBorder, m == 1 ? 1.0f : 0.5f));
                stroke();
                if (m == 1 || m == 2 || m == 5)
                {
                    if (f >= 1000.0f)
                        std::snprintf(buf, sizeof(buf), "%dk", int(f / 1000.0f));
                    else
                        std::snprintf(buf, sizeof(buf), "%d", int(f));
                    label(x, fPY + fPH + 10.0f, f == 20.0f ? ALIGN_LEFT : f == 20000.0f ? ALIGN_RIGHT : ALIGN_CENTER,
                          8.0f, 0.0f, theme::kMuted, buf);
                }
            }
        for (float db = 0.0f; db >= -range; db -= dbs)
        {
            const float y = std::round(linY(db, -range, 0.0f)) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(db == 0.0f ? col(theme::kTrackHover) : col(theme::kBorder, 0.6f));
            stroke();
            std::snprintf(buf, sizeof(buf), "%d", int(db));
            label(fPX - 5.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }

        const nullspec::View* v = view();
        if (!v)
            return;
        const int n = v->n.load(std::memory_order_relaxed);
        const float sr = v->sampleRate.load(std::memory_order_relaxed);
        const float slope = value(1);
        static float xs[kCols], ys[kCols];
        for (int pass = value(4) >= 0.5f ? 0 : 1; pass < 2; ++pass)
        {
            const float* src = pass ? v->spec : v->hold;
            for (int c = 0; c < kCols; ++c)
            {
                // the loudest bin under the column, or the two nearest bins interpolated
                const double f0 = 20.0 * std::pow(1000.0, double(c) / kCols);
                const double f1 = 20.0 * std::pow(1000.0, double(c + 1) / kCols);
                const double k0 = f0 * n / sr, k1 = std::min(n / 2.0, f1 * n / sr);
                int a = int(std::ceil(k0));
                const int b = int(std::floor(k1));
                double p = 0.0;
                if (a <= b)
                    for (; a <= b; ++a)
                        p = std::max(p, double(src[a]));
                else
                {
                    const double kc = std::min(n / 2.0 - 1.0, (k0 + k1) * 0.5);
                    const int i = int(std::floor(kc));
                    p = src[i] + (src[i + 1] - src[i]) * (kc - i);
                }
                const double db = 10.0 * std::log10(p + 1e-20) + slope * std::log2(std::sqrt(f0 * f1) / 1000.0);
                xs[c] = fPX + fPW * (float(c) + 0.5f) / float(kCols);
                ys[c] = linY(float(db), -range, 0.0f);
            }
            if (pass)
                curve(xs, ys, kCols, fPY + fPH);
            else
            {
                scissor(fPX, fPY, fPW, fPH);
                beginPath();
                moveTo(xs[0], ys[0]);
                for (int c = 1; c < kCols; ++c)
                    lineTo(xs[c], ys[c]);
                strokeColor(col(theme::kMuted, 0.8f));
                strokeWidth(1.0f);
                stroke();
                resetScissor();
            }
        }
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%.1f dB/oct", double(v));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }
};

UI* createUI()
{
    return new UISpectrumAnalyzer();
}

END_NAMESPACE_DISTRHO
