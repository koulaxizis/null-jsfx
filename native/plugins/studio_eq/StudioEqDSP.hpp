/*
 * NULL JSFX - Studio EQ, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/studio_eq.jsfx, computed in double in the EEL2 order: eight
 * bands in series (bell, shelves, Butterworth cuts of 6..96 dB/oct, notch, band pass), each on
 * stereo, mid, side, left or right. Zero Latency runs biquad cascades; Linear Phase builds four
 * 8192-tap FIRs from the same magnitude response (a 2x2 matrix per bin) and convolves them in
 * 1024-sample blocks with 16384-point FFTs (EelFft.hpp, EEL2's own transforms), latency 5120.
 * Every buffer is allocated in the constructor; process() never allocates.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullseq {

static constexpr int kBands = 8;
static constexpr int kBlock = 1024;
static constexpr int kLatency = 5120;

struct Params {
    double type[kBands] = {}, freq[kBands] = {}, gain[kBands] = {}, q[kBands] = {}, slope[kBands] = {}, channels[kBands] = {};
    double output = 0.0;
    bool autoGain = false, linear = false;
};

// the parameters from the 51 slider values (JSFX order)
template <typename Values>
inline Params params(const Values& value)
{
    Params p;
    for (int b = 0; b < kBands; ++b)
    {
        p.type[b] = value(uint32_t(b * 6));
        p.freq[b] = value(uint32_t(b * 6 + 1));
        p.gain[b] = value(uint32_t(b * 6 + 2));
        p.q[b] = value(uint32_t(b * 6 + 3));
        p.slope[b] = value(uint32_t(b * 6 + 4));
        p.channels[b] = value(uint32_t(b * 6 + 5));
    }
    p.output = value(48);
    p.autoGain = value(49) != 0.0;
    p.linear = value(50) != 0.0;
    return p;
}

inline int slopeOrder(double i)
{
    return i == 0 ? 1 : i == 1 ? 2 : i == 2 ? 3 : i == 3 ? 4 : i == 4 ? 6 : i == 5 ? 8 : i == 6 ? 12 : 16;
}

// the sections of every band (the JSFX's se_band), shared by the processor and the interface
struct Bands {
    double c[kBands][8][5] = {}; // b0 b1 b2 a1 a2 per section
    int ns[kBands] = {};
    int ch[kBands] = {};

    // se_rbj: kind 0 low shelf, 1 high shelf, 2 bell, 3 low pass, 4 high pass, 5 notch, 6 band pass
    static void rbj(double* c, int kind, double w, double gain, double q)
    {
        const double cs = std::cos(w), sn = std::sin(w);
        const double al = sn / (2 * q);
        const double aa = std::pow(10.0, gain / 40), sq = std::sqrt(aa);
        double a0;
        if (kind == 0)
        {
            c[0] = aa * ((aa + 1) - (aa - 1) * cs + 2 * sq * al);
            c[1] = 2 * aa * ((aa - 1) - (aa + 1) * cs);
            c[2] = aa * ((aa + 1) - (aa - 1) * cs - 2 * sq * al);
            a0 = (aa + 1) + (aa - 1) * cs + 2 * sq * al;
            c[3] = -2 * ((aa - 1) + (aa + 1) * cs);
            c[4] = (aa + 1) + (aa - 1) * cs - 2 * sq * al;
        }
        else if (kind == 1)
        {
            c[0] = aa * ((aa + 1) + (aa - 1) * cs + 2 * sq * al);
            c[1] = -2 * aa * ((aa - 1) + (aa + 1) * cs);
            c[2] = aa * ((aa + 1) + (aa - 1) * cs - 2 * sq * al);
            a0 = (aa + 1) - (aa - 1) * cs + 2 * sq * al;
            c[3] = 2 * ((aa - 1) - (aa + 1) * cs);
            c[4] = (aa + 1) - (aa - 1) * cs - 2 * sq * al;
        }
        else if (kind == 2)
        {
            c[0] = 1 + al * aa; c[1] = -2 * cs; c[2] = 1 - al * aa;
            a0 = 1 + al / aa; c[3] = -2 * cs; c[4] = 1 - al / aa;
        }
        else if (kind == 3)
        {
            c[0] = (1 - cs) / 2; c[1] = 1 - cs; c[2] = (1 - cs) / 2;
            a0 = 1 + al; c[3] = -2 * cs; c[4] = 1 - al;
        }
        else if (kind == 4)
        {
            c[0] = (1 + cs) / 2; c[1] = -(1 + cs); c[2] = (1 + cs) / 2;
            a0 = 1 + al; c[3] = -2 * cs; c[4] = 1 - al;
        }
        else if (kind == 5)
        {
            c[0] = 1; c[1] = -2 * cs; c[2] = 1;
            a0 = 1 + al; c[3] = -2 * cs; c[4] = 1 - al;
        }
        else
        {
            c[0] = al; c[1] = 0; c[2] = -al;
            a0 = 1 + al; c[3] = -2 * cs; c[4] = 1 - al;
        }
        c[0] /= a0; c[1] /= a0; c[2] /= a0; c[3] /= a0; c[4] /= a0;
    }

    // se_first: first-order bilinear low pass or high pass
    static void first(double* c, bool hp, double w)
    {
        const double k = std::tan(w / 2);
        c[0] = hp ? 1 / (k + 1) : k / (k + 1);
        c[1] = hp ? -c[0] : c[0];
        c[2] = 0;
        c[3] = (k - 1) / (k + 1);
        c[4] = 0;
    }

    void compute(const Params& p, double sr)
    {
        for (int b = 0; b < kBands; ++b)
        {
            const double tp = p.type[b], g = p.gain[b], q = std::max(p.q[b], 0.1);
            const int n = slopeOrder(p.slope[b]);
            ch[b] = int(p.channels[b]);
            const double w = 2 * M_PI * std::min(p.freq[b], sr * 0.49) / sr;
            int k = 0;
            if (tp == 1) { rbj(c[b][0], 2, w, g, q); k = 1; }
            else if (tp == 2) { rbj(c[b][0], 0, w, g, q); k = 1; }
            else if (tp == 3) { rbj(c[b][0], 1, w, g, q); k = 1; }
            else if (tp == 4 || tp == 5)
            {
                if (n & 1) { first(c[b][0], tp == 4, w); k = 1; }
                for (int j = 1; j <= n / 2; ++j)
                {
                    double qk = (n & 1) ? 1 / (2 * std::cos(j * M_PI / n)) : 1 / (2 * std::cos((2 * j - 1) * M_PI / (2 * n)));
                    if (j == n / 2)
                        qk *= q / 0.71;
                    rbj(c[b][k], tp == 4 ? 4 : 3, w, 0, qk);
                    ++k;
                }
            }
            else if (tp == 6) { rbj(c[b][0], 5, w, 0, q); k = 1; }
            else if (tp == 7) { rbj(c[b][0], 6, w, 0, q); k = 1; }
            ns[b] = k;
        }
    }

    // se_pow: |H|^2 of band b at w radians per sample
    double power(int b, double w) const
    {
        const double c1 = std::cos(w), c2 = std::cos(2 * w);
        double p = 1;
        for (int k = 0; k < ns[b]; ++k)
        {
            const double* s = c[b][k];
            const double nm = s[0] * s[0] + s[1] * s[1] + s[2] * s[2] + 2 * (s[0] * s[1] + s[1] * s[2]) * c1 + 2 * s[0] * s[2] * c2;
            const double dn = 1 + s[3] * s[3] + s[4] * s[4] + 2 * (s[3] + s[3] * s[4]) * c1 + 2 * s[4] * c2;
            p *= std::max(nm, 0.0) / std::max(dn, 1e-20);
        }
        return p;
    }

    // se_auto: minus the weighted mean level change of the bell and shelf bands, within +-24 dB
    double autoGain(const Params& p, double sr) const
    {
        double sum = 0;
        for (int b = 0; b < kBands; ++b)
        {
            const double tp = p.type[b];
            const double wt = ch[b] == 0 || ch[b] == 1 ? 1 : ch[b] == 2 ? 0 : 0.5;
            if (tp >= 1 && tp <= 3 && wt > 0)
                for (int i = 0; i < 64; ++i)
                {
                    const double f = std::min(20 * std::pow(1000.0, i / 63.0), sr * 0.49);
                    sum += wt * 10 * std::log10(std::max(power(b, 2 * M_PI * f / sr), 1e-20)) / 64;
                }
        }
        return std::max(-24.0, std::min(24.0, -sum));
    }

    // the display: the bands' level change at f Hz, added as if every band were stereo
    double responseDb(double f, double sr) const
    {
        const double w = 2 * M_PI * std::min(f, sr * 0.4999) / sr;
        double db = 0;
        for (int b = 0; b < kBands; ++b)
            if (ns[b] > 0)
                db += 10 * std::log10(std::max(power(b, w), 1e-20));
        return db;
    }
};

class StudioEqProcessor {
public:
    StudioEqProcessor()
        : fD(16384), fT(32768), fH(4 * 32768), fX(2 * 32768), fY(2 * 32768), fAcc(2 * 16384), fM(4 * 4097)
    {
    }

    // the JSFX @init: everything cleared, the next setParams resets the paths
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::fill(fZ[0][0], fZ[0][0] + kBands * 8 * 8, 0.0);
        std::fill(fIn, fIn + 2 * kBlock, 0.0);
        std::fill(fOut, fOut + 2 * kBlock, 0.0);
        std::fill(fH.begin(), fH.end(), 0.0);
        std::fill(fAcc.begin(), fAcc.end(), 0.0);
        fPos = 0;
        fAb = 0;
        fLinLast = -1;
    }

    // the JSFX @slider (update())
    void setParams(const Params& p)
    {
        const int lin = p.linear ? 1 : 0;
        if (lin != fLinLast)
        {
            std::fill(fIn, fIn + 2 * kBlock, 0.0);
            std::fill(fOut, fOut + 2 * kBlock, 0.0);
            std::fill(fZ[0][0], fZ[0][0] + kBands * 8 * 8, 0.0);
            std::fill(fAcc.begin(), fAcc.end(), 0.0);
            fPos = 0;
            fAb = 0;
            fLinLast = lin;
        }
        fLin = p.linear;
        fBands.compute(p, fSr);
        fGout = std::pow(10.0, (p.output + (p.autoGain ? fBands.autoGain(p, fSr) : 0)) / 20);
        if (fLin)
            design();
    }

    uint32_t latency() const { return fLin ? uint32_t(kLatency) : 0; }
    bool linear() const { return fLin; }
    const Bands& bands() const { return fBands; }

    void processSample(double& spl0, double& spl1)
    {
        if (fLin)
        {
            fIn[fPos] = spl0;
            fIn[kBlock + fPos] = spl1;
            spl0 = fOut[fPos] * fGout;
            spl1 = fOut[kBlock + fPos] * fGout;
            if (++fPos >= kBlock)
            {
                block();
                fPos = 0;
            }
            return;
        }
        double l = spl0, r = spl1;
        for (int b = 0; b < kBands; ++b)
        {
            const int n = fBands.ns[b];
            if (n <= 0)
                continue;
            const int h = fBands.ch[b];
            if (h == 0)
            {
                l = cas(l, b, 0, n);
                r = cas(r, b, 1, n);
            }
            else if (h == 3)
                l = cas(l, b, 0, n);
            else if (h == 4)
                r = cas(r, b, 1, n);
            else
            {
                double m = (l + r) * 0.5, s = (l - r) * 0.5;
                if (h == 1)
                    m = cas(m, b, 0, n);
                else
                    s = cas(s, b, 0, n);
                l = m + s;
                r = m - s;
            }
        }
        spl0 = l * fGout;
        spl1 = r * fGout;
    }

private:
    // se_cas: x through band b's n sections, state of channel ch
    double cas(double x, int b, int ch, int n)
    {
        for (int k = 0; k < n; ++k)
        {
            const double* c = fBands.c[b][k];
            double* s = fZ[b][k] + ch * 4;
            const double y = c[0] * x + c[1] * s[0] + c[2] * s[1] - c[3] * s[2] - c[4] * s[3];
            s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
            x = y;
        }
        return x;
    }

    // se_fir: one response row into the filter spectrum h
    void fir(const double* m, double* h)
    {
        double* d = fD.data();
        for (int k = 0; k < 4097; ++k)
        {
            d[2 * k] = m[k]; d[2 * k + 1] = 0;
            if (k > 0 && k < 4096) { d[2 * (8192 - k)] = m[k]; d[2 * (8192 - k) + 1] = 0; }
        }
        eelfft::fftIpermute(d, 8192);
        eelfft::ifft(d, 8192);
        double* t = fT.data();
        std::fill(fT.begin(), fT.end(), 0.0);
        for (int n = 0; n < 8192; ++n)
            t[2 * n] = d[2 * ((n + 4096) & 8191)] * (0.5 - 0.5 * std::cos(2 * M_PI * n / 8192)) / 134217728;
        eelfft::fft(t, 16384);
        std::memcpy(h, t, sizeof(double) * 32768);
    }

    // se_design: the 2x2 response per bin, band 1 first, then the four FIRs
    void design()
    {
        double *mll = fM.data(), *mlr = mll + 4097, *mrl = mlr + 4097, *mrr = mrl + 4097;
        for (int k = 0; k < 4097; ++k) { mll[k] = 1; mlr[k] = 0; mrl[k] = 0; mrr[k] = 1; }
        for (int b = 0; b < kBands; ++b)
        {
            if (fBands.ns[b] <= 0)
                continue;
            const int bc = fBands.ch[b];
            for (int k = 0; k < 4097; ++k)
            {
                const double h = std::sqrt(fBands.power(b, 2 * M_PI * k / 8192));
                if (bc == 0) { mll[k] *= h; mlr[k] *= h; mrl[k] *= h; mrr[k] *= h; }
                else if (bc == 3) { mll[k] *= h; mlr[k] *= h; }
                else if (bc == 4) { mrl[k] *= h; mrr[k] *= h; }
                else
                {
                    const double p = bc == 1 ? (h + 1) * 0.5 : (1 + h) * 0.5;
                    const double q = bc == 1 ? (h - 1) * 0.5 : (1 - h) * 0.5;
                    const double ll = mll[k], lr = mlr[k], rl = mrl[k], rr = mrr[k];
                    mll[k] = p * ll + q * rl; mlr[k] = p * lr + q * rr;
                    mrl[k] = q * ll + p * rl; mrr[k] = q * lr + p * rr;
                }
            }
        }
        for (int i = 0; i < 4; ++i)
            fir(fM.data() + i * 4097, fH.data() + i * 32768);
    }

    // se_block: one block through the 2x2 filter, overlap-add
    void block()
    {
        using namespace eelfft;
        double *xl = fX.data(), *xr = xl + 32768, *yl = fY.data(), *yr = yl + 32768, *t = fT.data();
        const double *hll = fH.data(), *hlr = hll + 32768, *hrl = hlr + 32768, *hrr = hrl + 32768;
        std::fill(fX.begin(), fX.end(), 0.0);
        for (int i = 0; i < kBlock; ++i) { xl[2 * i] = fIn[i]; xr[2 * i] = fIn[kBlock + i]; }
        fft(xl, 16384);
        fft(xr, 16384);
        std::memcpy(yl, xl, sizeof(double) * 32768); convolveC(yl, hll, 16384);
        std::memcpy(t, xr, sizeof(double) * 32768); convolveC(t, hlr, 16384);
        for (int i = 0; i < 32768; ++i) yl[i] += t[i];
        std::memcpy(yr, xl, sizeof(double) * 32768); convolveC(yr, hrl, 16384);
        std::memcpy(t, xr, sizeof(double) * 32768); convolveC(t, hrr, 16384);
        for (int i = 0; i < 32768; ++i) yr[i] += t[i];
        ifft(yl, 16384);
        ifft(yr, 16384);
        double *al = fAcc.data(), *ar = al + 16384;
        for (int i = 0; i < 9216; ++i)
        {
            const int j = (fAb + i) & 16383;
            al[j] += yl[2 * i];
            ar[j] += yr[2 * i];
        }
        for (int i = 0; i < kBlock; ++i)
        {
            const int j = (fAb + i) & 16383;
            fOut[i] = al[j];
            fOut[kBlock + i] = ar[j];
            al[j] = 0;
            ar[j] = 0;
        }
        fAb = (fAb + kBlock) & 16383;
    }

    double fSr = 48000.0;
    Bands fBands;
    double fZ[kBands][8][8] = {};
    double fGout = 1.0;
    bool fLin = false;
    int fLinLast = -1;
    double fIn[2 * kBlock] = {}, fOut[2 * kBlock] = {};
    int fPos = 0, fAb = 0;
    std::vector<double> fD, fT, fH, fX, fY, fAcc, fM;
};

} // namespace nullseq
