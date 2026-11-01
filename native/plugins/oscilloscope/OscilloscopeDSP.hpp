/*
 * NULL JSFX - Oscilloscope, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/oscilloscope.jsfx (Listen, the only thing that touches the
 * audio) and of the capture in the @sample of gfx/oscilloscope_gfx.jsfx, in the same order: a
 * screen of kCols columns, each the minimum and maximum of its samples, started by a crossing
 * of the trigger level (Auto: or after two screens without one; Free: at once) and copied to a
 * View the interface reads.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullosc {

static constexpr int kCols = 512; // OS_N

// what the interface draws; written by the audio thread, read by the UI (a torn read only
// draws one odd frame). a = mid, left or side; b = right
struct View {
    float an[kCols] = {}, ax[kCols] = {}, bn[kCols] = {}, bx[kCols] = {};
    std::atomic<int> state { 0 };       // 0 waiting, 1 capturing
    std::atomic<float> wait { 0.0f };  // samples since the last screen
    std::atomic<float> win { 960.0f }; // the screen's width in samples
};

struct Params {
    double time = 20.0;  // ms
    int sync = 0;        // 0 off, 1..6 = 1/16 .. 2 bars
    int trigger = 0;     // 0 auto, 1 normal, 2 free
    bool falling = false;
    double level = 0.0;
    int view = 0;        // 0 mid, 1 left/right, 2 side
    double gain = 0.0;   // dB
    bool hold = false;
    bool listen = false;
};

class Oscilloscope {
public:
    void setView(View* v) noexcept { fView = v; }

    void setSampleRate(double sr) noexcept { fSr = sr; }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fG = std::pow(10.0, p.gain / 20.0);
        fListen = p.listen && p.view != 1;
    }

    // @init of the GFX file
    void reset() noexcept
    {
        std::fill(fCan, fCan + kCols, 0.0);
        std::fill(fCax, fCax + kCols, 0.0);
        std::fill(fCbn, fCbn + kCols, 0.0);
        std::fill(fCbx, fCbx + kCols, 0.0);
        fCap = false;
        fN = fCol = 0;
        fWait = fPrev = fPa = fPb = 0.0;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames, double tempo) noexcept
    {
        // @block
        double win = fP.sync > 0 ? (0.25 * std::pow(2.0, double(fP.sync - 1))) * 60.0 / std::max(tempo, 1.0) * fSr
                                 : fP.time / 1000.0 * fSr;
        fWin = std::max(16.0, std::floor(win));

        for (uint32_t i = 0; i < frames; ++i)
        {
            const double s0 = inL[i], s1 = inR[i];
            capture(s0, s1);
            if (fListen)
            {
                const float o = float(fP.view == 2 ? (s0 - s1) * 0.5 : (s0 + s1) * 0.5);
                outL[i] = o;
                outR[i] = o;
            }
            else
            {
                outL[i] = inL[i];
                outR[i] = inR[i];
            }
        }
        if (fView)
        {
            fView->state.store(fCap ? 1 : 0, std::memory_order_relaxed);
            fView->wait.store(float(fWait), std::memory_order_relaxed);
            fView->win.store(float(fWin), std::memory_order_relaxed);
        }
    }

private:
    void capture(double s0, double s1) noexcept
    {
        const double l = s0 * fG, r = s1 * fG;
        const double src = fP.view == 1 ? l : fP.view == 2 ? (l - r) * 0.5 : (l + r) * 0.5;
        if (!fCap && !fP.hold)
        {
            fWait += 1.0;
            const bool edge = fP.falling ? (fPrev > fP.level && src <= fP.level) : (fPrev < fP.level && src >= fP.level);
            if (fP.trigger == 2 || edge || (fP.trigger == 0 && fWait >= 2.0 * fWin))
            {
                fCap = true;
                fN = 0;
                fCol = 0;
                fCan[0] = fCax[0] = fPa = src;
                fCbn[0] = fCbx[0] = fPb = r;
            }
        }
        if (fCap)
        {
            const int c = int(std::min(double(kCols - 1), std::floor(double(fN) * kCols / fWin)));
            if (c > fCol)
            {
                // columns skipped (a screen shorter than kCols samples) get the line from the
                // last value to this one
                for (int k = fCol + 1; k <= c; ++k)
                {
                    const double t = double(k - fCol) / double(c - fCol);
                    fCan[k] = fCax[k] = fPa + (src - fPa) * t;
                    fCbn[k] = fCbx[k] = fPb + (r - fPb) * t;
                }
                fCol = c;
            }
            else
            {
                fCan[c] = std::min(fCan[c], src);
                fCax[c] = std::max(fCax[c], src);
                fCbn[c] = std::min(fCbn[c], r);
                fCbx[c] = std::max(fCbx[c], r);
            }
            fPa = src;
            fPb = r;
            fN += 1;
            if (double(fN) >= fWin)
            {
                for (int k = fCol + 1; k < kCols; ++k)
                {
                    fCan[k] = fCax[k] = fPa;
                    fCbn[k] = fCbx[k] = fPb;
                }
                if (fView)
                    for (int k = 0; k < kCols; ++k)
                    {
                        fView->an[k] = float(fCan[k]);
                        fView->ax[k] = float(fCax[k]);
                        fView->bn[k] = float(fCbn[k]);
                        fView->bx[k] = float(fCbx[k]);
                    }
                fCap = false;
                fWait = 0.0;
            }
        }
        fPrev = src;
    }

    View* fView = nullptr;
    Params fP;
    double fSr = 48000.0, fG = 1.0, fWin = 960.0;
    bool fListen = false, fCap = false;
    int fN = 0, fCol = 0;
    double fWait = 0.0, fPrev = 0.0, fPa = 0.0, fPb = 0.0;
    double fCan[kCols] = {}, fCax[kCols] = {}, fCbn[kCols] = {}, fCbx[kCols] = {};
};

} // namespace nullosc
