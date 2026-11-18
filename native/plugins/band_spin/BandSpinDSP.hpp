/*
 * NULL JSFX - Band Spin, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/band_spin.jsfx (computed in double, in the EEL2 order): the
 * Band Splitter crossover (nullbandsplit, the same LR4 filters and allpasses) gives four bands,
 * each on its own rotor: a constant-power pan on the sine of the angle, a volume dip and a
 * Doppler delay (up to 1 ms, linear interpolation) on its cosine. Follow speeds a rotor up with
 * its band's level. The delay lines are allocated in setSampleRate(), never in process().
 */

#pragma once

#include "../band_splitter/BandSplitterDSP.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullbandspin {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kBufLen = 2048;              // BS_N: each delay line

struct Params {
    double xover[3] = { 200.0, 1200.0, 5000.0 };
    double speed[4] = { 0.6, 1.1, 2.3, 4.7 };
    double vol = 0.4, volK = 1.0 / 0.8, pan = 0.6, dmax = 0.0, fol = 0.0, mix = 1.0;
};

// @slider, slider1..slider12 in order
inline Params paramsFor(const double* s, double sr) noexcept
{
    Params p;
    p.xover[0] = s[0]; p.xover[1] = s[1]; p.xover[2] = s[2];
    for (int b = 0; b < 4; ++b)
        p.speed[b] = s[3 + b];
    p.vol = s[7] / 100.0;
    p.volK = 1.0 / (1.0 - 0.5 * p.vol);
    p.pan = s[8] / 100.0;
    p.dmax = std::min(s[9] / 100.0 * 0.001 * sr, double(kBufLen - 4));
    p.fol = s[10] / 100.0;
    p.mix = s[11] / 100.0;
    return p;
}

class BandSpinProcessor {
public:
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBuf.assign(size_t(8 * kBufLen), 0.0);
        fEa = std::exp(-1.0 / (0.01 * sr));
        fEr = std::exp(-1.0 / (0.3 * sr));
        fIc = std::exp(-1.0 / (0.3 * sr));
        apply();
        reset();
    }

    // @init: the filters and delay lines cleared, the rotors still and a quarter turn apart
    // (they spin up to their speeds through the inertia)
    void reset() noexcept
    {
        fL = nullbandsplit::Channel();
        fR = nullbandsplit::Channel();
        std::fill(fBuf.begin(), fBuf.end(), 0.0);
        fWpos = 0;
        for (int b = 0; b < 4; ++b)
        {
            fFq[b] = 0.0;
            fPh[b] = 0.25 * b;
            fEv[b] = 0.0;
        }
    }

    // slider1..slider12 of band_spin.jsfx
    void setParams(const double* s) noexcept
    {
        std::copy(s, s + 12, fS);
        apply();
    }

    double phase(int b) const noexcept { return fPh[b]; }
    double speed(int b) const noexcept { return fFq[b]; }

    void processSample(double& l, double& r) noexcept
    {
        const Params& p = fP;
        fL.tick(l, 4, fC);
        fR.tick(r, 4, fC);
        double dl = 0.0, dr = 0.0, wl = 0.0, wr = 0.0;
        for (int b = 0; b < 4; ++b)
        {
            double xl = fL.b[b], xr = fR.b[b];
            dl += xl;
            dr += xr;
            double tgt;
            if (p.fol > 0.0)
            {
                const double ex = std::max(std::fabs(xl), std::fabs(xr));
                double e = fEv[b];
                e = ex > e ? fEa * e + (1.0 - fEa) * ex : fEr * e + (1.0 - fEr) * ex;
                fEv[b] = e;
                const double ee = e > 0.00001 ? std::max(0.0, std::min(1.0, (20.0 * std::log10(e) + 48.0) / 48.0)) : 0.0;
                tgt = p.speed[b] * (1.0 + 2.0 * p.fol * ee);
            }
            else
                tgt = p.speed[b];
            fFq[b] = fIc * fFq[b] + (1.0 - fIc) * tgt;
            double ph = fPh[b] + fFq[b] / fSr;
            if (ph >= 1.0) ph -= 1.0;
            fPh[b] = ph;
            const double a = 2.0 * kPi * ph;
            const double cs = std::cos(a);
            if (p.dmax > 0.0)
            {
                double* bl = fBuf.data() + 2 * b * kBufLen;
                double* br = bl + kBufLen;
                bl[fWpos] = xl;
                br[fWpos] = xr;
                double rp = fWpos - p.dmax * 0.5 * (1.0 + cs);
                if (rp < 0.0) rp += kBufLen;
                const int i0 = int(std::floor(rp));
                const double fr = rp - i0;
                int i1 = i0 + 1;
                if (i1 >= kBufLen) i1 = 0;
                xl = bl[i0] + (bl[i1] - bl[i0]) * fr;
                xr = br[i0] + (br[i1] - br[i0]) * fr;
            }
            const double g = (1.0 - p.vol * 0.5 * (1.0 + cs)) * p.volK;
            const double pn = p.pan * std::sin(a);
            wl += xl * g * std::sqrt(1.0 - pn);
            wr += xr * g * std::sqrt(1.0 + pn);
        }
        if (++fWpos >= kBufLen) fWpos = 0;
        l = dl + (wl - dl) * p.mix;
        r = dr + (wr - dr) * p.mix;
    }

private:
    void apply() noexcept
    {
        fP = paramsFor(fS, fSr);
        double xo[3];
        nullbandsplit::effectiveXovers(fP.xover, fSr, xo);
        for (int k = 0; k < 3; ++k)
            fC[k].set(xo[k], fSr);
    }

    double fS[12] = { 200.0, 1200.0, 5000.0, 0.6, 1.1, 2.3, 4.7, 40.0, 60.0, 20.0, 0.0, 100.0 };
    Params fP;
    nullbandsplit::Coefs fC[3];
    nullbandsplit::Channel fL, fR;
    std::vector<double> fBuf;
    int fWpos = 0;
    double fSr = 48000.0, fEa = 0.0, fEr = 0.0, fIc = 0.0;
    double fFq[4] = {}, fPh[4] = {}, fEv[4] = {};
};

} // namespace nullbandspin
