/*
 * NULL JSFX - Stereo Scope, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/stereo_scope.jsfx, computed in double like EEL2: the audio
 * passes through untouched (Monitor Mono / Side fold it for listening), and one-poles of time
 * constant Integration track <LL>, <RR> and <LR>, from which the interface derives correlation,
 * balance and width. The display-only part (sample ring, peaks, correlation history) mirrors
 * the @sample additions of stereo_scope_gfx.jsfx and goes to a View the UI reads.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace nullscope {

static constexpr int kRing = 8192; // goniometer samples (sc_rb)
static constexpr int kHist = 150;  // correlation history, 1/30 s each (sc_hb)

// what the interface draws; written by the audio thread, read by the UI (a torn read only
// draws one odd frame)
struct View {
    float ringL[kRing] = {}, ringR[kRing] = {};
    std::atomic<int> rw { 0 };
    std::atomic<float> ll { 0.0f }, rr { 0.0f }, lr { 0.0f }, pl { 0.0f }, pr { 0.0f };
    float hist[kHist] = {};
    std::atomic<int> hw { 0 }, hn { 0 };
};

// correlation of the integrated energies, 0 when either side is silent (sc_corr)
inline double correlation(double ll, double rr, double lr) noexcept
{
    const double p = ll * rr;
    return p > 0.00000000000001 ? std::max(-1.0, std::min(1.0, lr / std::sqrt(p))) : 0.0;
}

class StereoScope {
public:
    void setView(View* v) noexcept { fView = v; }

    // @init (and the display-only init of the GFX version)
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        ll = rr = lr = 0.0;
        fRw = 0;
        fHw = fHn = fHc = 0;
        fHper = int(std::floor(sr / 30.0));
        fPrel = std::exp(-1.0 / (0.3 * sr));
        fPl = fPr = 0.0;
        if (fView)
        {
            std::fill(fView->ringL, fView->ringL + kRing, 0.0f);
            std::fill(fView->ringR, fView->ringR + kRing, 0.0f);
            std::fill(fView->hist, fView->hist + kHist, 0.0f);
            publish();
        }
    }

    // @slider
    void setParams(double integrationMs, int monitor) noexcept
    {
        k = 1.0 - std::exp(-1.0 / (integrationMs / 1000.0 * fSr));
        mon = monitor;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double dl = inL[i], dr = inR[i];
            ll += k * (dl * dl - ll);
            rr += k * (dr * dr - rr);
            lr += k * (dl * dr - lr);
            if (std::fabs(ll) < 1e-21) ll = 0.0;
            if (std::fabs(rr) < 1e-21) rr = 0.0;
            if (std::fabs(lr) < 1e-21) lr = 0.0;

            if (mon == 1)
            {
                const double o = (dl + dr) * 0.5;
                outL[i] = static_cast<T>(o);
                outR[i] = static_cast<T>(o);
            }
            else if (mon == 2)
            {
                const double o = (dl - dr) * 0.5;
                outL[i] = static_cast<T>(o);
                outR[i] = static_cast<T>(o);
            }
            else
            {
                outL[i] = inL[i];
                outR[i] = inR[i];
            }

            if (!fView)
                continue;
            // display only
            fView->ringL[fRw] = float(dl);
            fView->ringR[fRw] = float(dr);
            if (++fRw >= kRing)
                fRw = 0;
            fPl = std::fabs(dl) > fPl ? std::fabs(dl) : fPl * fPrel;
            fPr = std::fabs(dr) > fPr ? std::fabs(dr) : fPr * fPrel;
            if (fPl < 1e-15) fPl = 0.0;
            if (fPr < 1e-15) fPr = 0.0;
            if (++fHc >= fHper)
            {
                fHc = 0;
                fView->hist[fHw] = float(correlation(ll, rr, lr));
                if (++fHw >= kHist)
                    fHw = 0;
                fHn = std::min(kHist, fHn + 1);
            }
        }
        if (fView)
            publish();
    }

private:
    void publish() noexcept
    {
        fView->ll.store(float(ll), std::memory_order_relaxed);
        fView->rr.store(float(rr), std::memory_order_relaxed);
        fView->lr.store(float(lr), std::memory_order_relaxed);
        fView->pl.store(float(fPl), std::memory_order_relaxed);
        fView->pr.store(float(fPr), std::memory_order_relaxed);
        fView->hn.store(fHn, std::memory_order_relaxed);
        fView->hw.store(fHw, std::memory_order_release);
        fView->rw.store(fRw, std::memory_order_release);
    }

    View* fView = nullptr;
    double fSr = 48000.0, k = 0.0001, ll = 0.0, rr = 0.0, lr = 0.0;
    int mon = 0;
    int fRw = 0, fHw = 0, fHn = 0, fHc = 0, fHper = 1600;
    double fPrel = 0.9999, fPl = 0.0, fPr = 0.0;
};

} // namespace nullscope
