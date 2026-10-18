/*
 * NULL JSFX - Synth: the eight wavetables, made by synthesis (no files)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The same steps as wt_make / wt_store in synth.jsfx: each frame's spectrum in natural bin
 * order (the FFT of a time-domain cycle, or harmonics filled in directly), then per octave
 * level L the bins above 512 / 2^L harmonics removed and the inverse FFT, every frame scaled
 * to a peak of 1 at level 0. 8 tables x 8 frames x 8 levels x 1024 samples, made once and
 * shared by every instance (and the interface).
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace nullsynth {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kTables = 8, kFrames = 8, kLevels = 8, kTableLen = 1024;

inline double sqrd(double x) { return x * x; }

inline double wtTri(double t) { return t < 0.25 ? 4 * t : t < 0.75 ? 2 - 4 * t : 4 * t - 4; }

// the time-domain tables: one cycle at phase t for frame position k (wt_shape)
inline double wtShape(int tb, double k, double t)
{
    if (tb == 0)
    {
        const double s = k * 3;
        const double i = std::min(2.0, std::floor(s));
        const double a = s - i;
        const double saw = 2 * (t + 0.5 - std::floor(t + 0.5)) - 1;
        const double x = i == 0 ? std::sin(2 * kPi * t) : i == 1 ? wtTri(t) : saw;
        const double y = i == 0 ? wtTri(t) : i == 1 ? 2 * (t + 0.5 - std::floor(t + 0.5)) - 1 : (t < 0.5 ? 1 : -1);
        return x + (y - x) * a;
    }
    if (tb == 1)
        return t < 0.5 - 0.45 * k ? 1 : -1;
    if (tb == 5)
    {
        const double x = t * (1 + 3.5 * k);
        return 2 * (x - std::floor(x)) - 1;
    }
    if (tb == 6)
    {
        const double g = 1 + 5 * k;
        return std::sin(kPi * 0.5 * g * std::sin(2 * kPi * t));
    }
    const double q = std::floor(std::pow(2.0, 5 - 4 * k) + 0.5);
    const double x = std::floor(std::sin(2 * kPi * t) * q + 0.5) / q;
    const double y = std::floor((2 * t - 1) * q + 0.5) / q;
    return x + (y - x) * k * 0.5;
}

// the additive tables: harmonic h's amplitude for frame position k (wt_harm)
inline double wtHarm(int tb, double k, int h)
{
    if (tb == 2)
    {
        const int j = h == 1 ? 0 : h == 2 ? 1 : h == 3 ? 2 : h == 4 ? 3 : h == 6 ? 4 : h == 8 ? 5 : h == 10 ? 6 : h == 12 ? 7 : h == 16 ? 8 : -1;
        return j < 0 ? 0 : std::max(0.0, std::min(1.0, k * 7 + 2 - j)) / (1 + 0.3 * j);
    }
    if (tb == 3)
    {
        const double v = k * 4;
        const double j = std::min(3.0, std::floor(v));
        const double a = v - j;
        const double f1 = j == 0 ? 800 + (400 - 800) * a : j == 1 ? 400 + (270 - 400) * a : j == 2 ? 270 + (450 - 270) * a : 450 + (325 - 450) * a;
        const double f2 = j == 0 ? 1150 + (1700 - 1150) * a : j == 1 ? 1700 + (2300 - 1700) * a : j == 2 ? 2300 + (800 - 2300) * a : 800 + (700 - 800) * a;
        const double f = h * 110.0;
        return (0.04 + std::exp(-sqrd((f - f1) / 90)) + 0.6 * std::exp(-sqrd((f - f2) / 130))) / std::sqrt(double(h));
    }
    double c = 0, j = 0;
    while (j < 24 && c < h)
    {
        j += 1;
        c = j * (j + 1) / 2;
    }
    return j <= 1 && h == 1 ? 1 : c == h ? std::exp(-sqrd((j - 1 - 10 * k) / (2 + 2 * k))) * 0.8 : 0;
}

inline const std::vector<double>& wavetables()
{
    static const std::vector<double> wt = [] {
        std::vector<double> t(size_t(kTables * kFrames * kLevels * kTableLen));
        std::vector<double> ws(2048), wb(2048);
        for (int tb = 0; tb < kTables; ++tb)
            for (int fr = 0; fr < kFrames; ++fr)
            {
                const double k = fr / 7.0;
                if (tb == 2 || tb == 3 || tb == 4)
                {
                    std::fill(ws.begin(), ws.end(), 0.0);
                    for (int h = 1; h <= 511; ++h)
                    {
                        const double a = wtHarm(tb, k, h);
                        ws[size_t(2 * h + 1)] = -a;
                        ws[size_t(2 * (1024 - h) + 1)] = a;
                    }
                }
                else
                {
                    for (int n = 0; n < 1024; ++n)
                    {
                        wb[size_t(2 * n)] = wtShape(tb, k, n / 1024.0);
                        wb[size_t(2 * n + 1)] = 0;
                    }
                    eelfft::fft(wb.data(), 1024);
                    eelfft::fftPermute(wb.data(), 1024);
                    ws = wb;
                }
                // wt_store
                double pk = 0;
                double* frame = t.data() + size_t(tb * 8 + fr) * 8192;
                for (int lv = 0; lv < kLevels; ++lv)
                {
                    const int h = std::min(511, int(std::floor(512 / std::pow(2.0, lv))));
                    wb = ws;
                    wb[0] = 0;
                    wb[1] = 0;
                    for (int k2 = h + 1; k2 < h + 1 + (1023 - 2 * h); ++k2)
                    {
                        wb[size_t(2 * k2)] = 0;
                        wb[size_t(2 * k2 + 1)] = 0;
                    }
                    eelfft::fftIpermute(wb.data(), 1024);
                    eelfft::ifft(wb.data(), 1024);
                    double* base = frame + lv * 1024;
                    for (int n = 0; n < 1024; ++n)
                        base[n] = wb[size_t(2 * n)];
                    if (lv == 0)
                        for (int n = 0; n < 1024; ++n)
                            pk = std::max(pk, std::fabs(base[n]));
                }
                const double sc = pk > 0 ? 1 / pk : 0;
                for (int n = 0; n < 8192; ++n)
                    frame[n] *= sc;
            }
        return t;
    }();
    return wt;
}

} // namespace nullsynth
