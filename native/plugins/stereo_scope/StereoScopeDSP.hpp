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
    std::atomic<float> band[3] = { { 0.0f }, { 0.0f }, { 0.0f } }; // Band Correlation: Low, Mid, High
};

// correlation of the integrated energies, 0 when either side is silent (sc_corr)
inline double correlation(double ll, double rr, double lr) noexcept
{
    const double p = ll * rr;
    return p > 0.00000000000001 ? std::max(-1.0, std::min(1.0, lr / std::sqrt(p))) : 0.0;
}

// RBJ biquad (Q 0.7071), transposed direct form II, as bd_coef / bd_tick in the JSFX
struct Biquad {
    double c0 = 1.0, c1 = 0.0, c2 = 0.0, c3 = 0.0, c4 = 0.0;
    void set(double f, bool hp, double sr) noexcept
    {
        const double w = (2.0 * 3.141592653589793 * f) / sr;
        const double cs = std::cos(w);
        const double al = std::sin(w) / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + al;
        c0 = (hp ? (1.0 + cs) / 2.0 : (1.0 - cs) / 2.0) / a0;
        c1 = (hp ? -(1.0 + cs) : 1.0 - cs) / a0;
        c2 = c0;
        c3 = (-2.0 * cs) / a0;
        c4 = (1.0 - al) / a0;
    }
};
struct BiquadState {
    double z1 = 0.0, z2 = 0.0;
    double tick(double x, const Biquad& q) noexcept
    {
        const double y = q.c0 * x + z1;
        z1 = (q.c1 * x - q.c3 * y) + z2;
        z2 = q.c2 * x - q.c4 * y;
        if (std::fabs(z1) < 1e-21) z1 = 0.0;
        if (std::fabs(z2) < 1e-21) z2 = 0.0;
        return y;
    }
};

class StereoScope {
public:
    void setView(View* v) noexcept { fView = v; }

    // @init (and the display-only init of the GFX version)
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        ll = rr = lr = 0.0;
        for (double* e : { fBll, fBrr, fBlr })
            std::fill(e, e + 3, 0.0);
        for (BiquadState& b : fFl)
            b = BiquadState();
        for (BiquadState& b : fFr)
            b = BiquadState();
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
    void setParams(double integrationMs, int monitor, bool bands = false) noexcept
    {
        k = 1.0 - std::exp(-1.0 / (integrationMs / 1000.0 * fSr));
        mon = monitor;
        fBands = bands;
        fLp1.set(200.0, false, fSr);
        fHp1.set(200.0, true, fSr);
        fLp2.set(2000.0, false, fSr);
        fHp2.set(2000.0, true, fSr);
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
            if (fBands)
            {
                // Band Correlation: Low (LP 200 Hz), Mid (HP 200 Hz, LP 2 kHz), High (HP 2 kHz)
                const double bl[3] = { fFl[0].tick(dl, fLp1), fFl[1].tick(fFl[3].tick(dl, fHp1), fLp2), fFl[2].tick(dl, fHp2) };
                const double br[3] = { fFr[0].tick(dr, fLp1), fFr[1].tick(fFr[3].tick(dr, fHp1), fLp2), fFr[2].tick(dr, fHp2) };
                for (int b = 0; b < 3; ++b)
                {
                    fBll[b] += k * (bl[b] * bl[b] - fBll[b]);
                    fBrr[b] += k * (br[b] * br[b] - fBrr[b]);
                    fBlr[b] += k * (bl[b] * br[b] - fBlr[b]);
                }
            }

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
        for (int b = 0; b < 3; ++b)
            fView->band[b].store(float(correlation(fBll[b], fBrr[b], fBlr[b])), std::memory_order_relaxed);
        fView->hn.store(fHn, std::memory_order_relaxed);
        fView->hw.store(fHw, std::memory_order_release);
        fView->rw.store(fRw, std::memory_order_release);
    }

    View* fView = nullptr;
    double fSr = 48000.0, k = 0.0001, ll = 0.0, rr = 0.0, lr = 0.0;
    int mon = 0;
    int fRw = 0, fHw = 0, fHn = 0, fHc = 0, fHper = 1600;
    double fPrel = 0.9999, fPl = 0.0, fPr = 0.0;
    bool fBands = false;
    Biquad fLp1, fHp1, fLp2, fHp2;
    BiquadState fFl[4], fFr[4];
    double fBll[3] = {}, fBrr[3] = {}, fBlr[3] = {};
};

} // namespace nullscope
