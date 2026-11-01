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
    { 7, NullKnobsUI::kSelect, "VIEW", 1, -1, false },
    { 8, NullKnobsUI::kSelect, "SIDECHAIN", 1, -1, false },
};

class UISpectrumAnalyzer : public NullKnobsUI
{
public:
    UISpectrumAnalyzer()
        : NullKnobsUI({ "SPECTRUM ANALYZER", "ADVANCED", 640, 280, nullptr, 2, nullspec::kSliders, nullspec::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true })
    {
        std::fill(std::begin(fSg), std::end(fSg), -1000.0f); // an empty history is silence
    }

protected:
    static constexpr int kCols = 558;
    static constexpr int kSgC = 186, kSgR = 60; // the spectrogram: columns (frames), log-frequency rows

    float fSg[kSgC * kSgR];
    float fSgCol[kSgR] = {};
    int fSgW = 0, fSgFrames = -1;

    // as the @gfx: each frame analysed since the last draw becomes a column (the loudest bin in
    // each row, Slope applied), kept whatever the View
    void updateSpectrogram(const nullspec::View* v, int n, float sr, float slope)
    {
        const int frames = v->frames.load(std::memory_order_relaxed);
        if (fSgFrames < 0)
            fSgFrames = frames;
        const int add = std::min(kSgC, frames - fSgFrames);
        fSgFrames = frames;
        if (add <= 0)
            return;
        for (int r = 0; r < kSgR; ++r)
        {
            const double f0 = 20.0 * std::pow(1000.0, double(r) / kSgR), f1 = 20.0 * std::pow(1000.0, double(r + 1) / kSgR);
            int a = int(std::ceil(f0 * n / sr));
            const int b = int(std::floor(std::min(n / 2.0, f1 * n / sr)));
            double p = 0.0;
            if (a <= b)
                for (; a <= b; ++a)
                    p = std::max(p, double(v->spec[a]));
            else
            {
                const double kc = std::min(n / 2.0 - 1.0, (f0 + f1) / 2.0 * n / sr);
                const int i = int(std::floor(kc));
                p = v->spec[i] + (v->spec[i + 1] - v->spec[i]) * (kc - i);
            }
            fSgCol[r] = float(10.0 * std::log10(p + 1e-20) + slope * std::log2(std::sqrt(f0 * f1) / 1000.0));
        }
        for (int c = 0; c < add; ++c)
        {
            std::copy(fSgCol, fSgCol + kSgR, fSg + fSgW * kSgR);
            if (++fSgW >= kSgC)
                fSgW = 0;
        }
    }

    // View Spectrogram: time left to right (newest at the right), frequency bottom to top,
    // brightness from -Range to 0 dB in 16 steps (one path per step)
    void drawSpectrogram(float range, float hop, float sr)
    {
        box(fPX, fPY, fPW, fPH, theme::kBackground);
        for (int q = 1; q <= 16; ++q)
        {
            beginPath();
            bool any = false;
            for (int c = 0; c < kSgC; ++c)
            {
                int gi = fSgW + c;
                if (gi >= kSgC)
                    gi -= kSgC;
                const float xl = std::floor(fPX + fPW * float(c) / kSgC), xr = std::floor(fPX + fPW * float(c + 1) / kSgC);
                for (int r = 0; r < kSgR; ++r)
                {
                    const int lv = int(std::floor((fSg[gi * kSgR + r] + range) / range * 16.0f));
                    if (std::min(lv, 16) != q)
                        continue;
                    const float yb = std::floor(fPY + fPH * (1.0f - float(r) / kSgR));
                    const float yt = std::floor(fPY + fPH * (1.0f - float(r + 1) / kSgR));
                    rect(xl, yt, xr - xl, yb - yt);
                    any = true;
                }
            }
            if (any)
            {
                fillColor(col(theme::kAccent, float(q) / 16.0f));
                fill();
            }
        }
        char buf[24];
        strokeWidth(1.0f);
        for (float d = 100.0f; d <= 10000.0f; d *= 10.0f)
        {
            const float y = std::floor(fPY + fPH * (1.0f - std::log(d / 20.0f) / std::log(1000.0f))) + 0.5f;
            beginPath();
            moveTo(fPX, y);
            lineTo(fPX + fPW, y);
            strokeColor(col(theme::kBorder, 0.8f));
            stroke();
            if (d >= 1000.0f)
                std::snprintf(buf, sizeof(buf), "%dk", int(d / 1000.0f));
            else
                std::snprintf(buf, sizeof(buf), "%d", int(d));
            label(fPX - 5.0f, y, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, buf);
        }
        std::snprintf(buf, sizeof(buf), "-%.1f S", double(kSgC * hop / sr));
        label(fPX, fPY + fPH + 10.0f, ALIGN_LEFT, 8.0f, 0.0f, theme::kMuted, buf);
        label(fPX + fPW, fPY + fPH + 10.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kMuted, "NOW");
    }

    void box(float x, float y, float w, float h, uint32_t rgb)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb));
        fill();
    }

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
        const nullspec::View* v = view();
        if (v)
            updateSpectrogram(v, v->n.load(std::memory_order_relaxed), v->sampleRate.load(std::memory_order_relaxed), value(1));
        if (value(7) >= 0.5f)
        {
            drawSpectrogram(range, v ? v->hop.load(std::memory_order_relaxed) : 1200.0f,
                            v ? v->sampleRate.load(std::memory_order_relaxed) : 48000.0f);
            return;
        }
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

        if (!v)
            return;
        const int n = v->n.load(std::memory_order_relaxed);
        const float sr = v->sampleRate.load(std::memory_order_relaxed);
        const float slope = value(1);
        static float xs[kCols], ys[kCols];
        // pass 0 the peak hold, 1 the spectrum, 2 the sidechain (Sidechain On)
        const bool sc = value(8) >= 0.5f;
        for (int pass = value(4) >= 0.5f ? 0 : 1; pass < (sc ? 3 : 2); ++pass)
        {
            const float* src = pass == 2 ? v->sc : pass ? v->spec : v->hold;
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
            if (pass == 1)
                curve(xs, ys, kCols, fPY + fPH);
            else
            {
                scissor(fPX, fPY, fPW, fPH);
                beginPath();
                moveTo(xs[0], ys[0]);
                for (int c = 1; c < kCols; ++c)
                    lineTo(xs[c], ys[c]);
                strokeColor(pass == 2 ? col(theme::kText, 0.55f) : col(theme::kMuted, 0.8f));
                strokeWidth(1.0f);
                stroke();
                resetScissor();
            }
        }
        if (sc)
            label(fPX + fPW - 4.0f, fPY + 8.0f, ALIGN_RIGHT, 8.0f, 0.0f, theme::kText, "SIDECHAIN");
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
