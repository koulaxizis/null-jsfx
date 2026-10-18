/*
 * NULL JSFX - Clipper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/clipper.jsfx, computed in double like EEL2: Input gain, then
 * (with Oversampling) 1 to 3 polyphase halfband FIR stages up (63, 23 and 15 taps, Kaiser-
 * windowed sinc), a short pad delay so the latency is a whole number of samples, the clipper
 * (hard, or a tanh knee from Ceiling x (1 - Knee) into Ceiling; Stereo link or Dual), the same
 * stages down, then Mix with the dry input delayed by the latency and Output.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullclipper {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double input = 0.0, ceiling = -0.1, knee = 0.0;
    int oversampling = 2;  // 0 off, 1 = 2x, 2 = 4x, 3 = 8x
    double mix = 100.0, output = 0.0;
    int link = 1;          // 0 = Stereo, 1 = Dual
};

// latency in samples for an Oversampling setting (the JSFX's pdc_delay)
inline int latencyFor(int os) noexcept { return os <= 0 ? 0 : os == 1 ? 31 : os == 2 ? 37 : 39; }

inline double besselI0(double x) noexcept
{
    double s = 1.0, t = 1.0, k = 1.0;
    for (int i = 0; i < 40; ++i)
    {
        const double q = x / (2.0 * k);
        t *= q * q;
        s += t;
        k += 1.0;
    }
    return s;
}

// hb_design: the (taps + 1) / 2 nonzero side taps of a halfband lowpass, summing to 0.5
inline void halfbandDesign(double* e, double* d, int taps, double beta) noexcept
{
    const double c = (taps - 1) / 2.0;
    const int n = (taps + 1) / 2;
    double s = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double dd = 2.0 * i - c;
        const double a = std::fabs(dd);
        const double sg = (int((a - 1.0) / 2.0) % 2) ? -1.0 : 1.0;
        const double r = 2.0 * dd / (taps - 1);
        const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
        d[i] = sg / (kPi * a) * w;
        s += d[i];
    }
    for (int i = 0; i < n; ++i)
    {
        d[i] = 0.5 * d[i] / s;
        e[i] = 2.0 * d[i];
    }
}

// 2x polyphase upsampler (hb_up): history of the last N inputs, stored twice
template <int N>
struct Up {
    int p = 0;
    double buf[2 * N] = {};
    void reset() noexcept { p = 0; std::memset(buf, 0, sizeof(buf)); }
    void process(double x, const double* e, int m, double& o0, double& o1) noexcept
    {
        p = p > 0 ? p - 1 : N - 1;
        double* q = buf + p;
        q[0] = x;
        q[N] = x;
        double acc = 0.0;
        for (int i = 0; i < N; ++i)
            acc += e[i] * q[i];
        o0 = acc;
        o1 = q[m];
    }
};

// 2x polyphase downsampler (hb_down): even history stored twice, odd samples through M + 1
template <int N, int M>
struct Down {
    int p = 0, q = 0;
    double buf[2 * N] = {};
    double odd[M + 1] = {};
    void reset() noexcept
    {
        p = q = 0;
        std::memset(buf, 0, sizeof(buf));
        std::memset(odd, 0, sizeof(odd));
    }
    double process(double v0, double v1, const double* d) noexcept
    {
        p = p > 0 ? p - 1 : N - 1;
        double* b = buf + p;
        b[0] = v0;
        b[N] = v0;
        double acc = 0.0;
        for (int i = 0; i < N; ++i)
            acc += d[i] * b[i];
        acc += 0.5 * odd[q];
        odd[q] = v1;
        q = q < M ? q + 1 : 0;
        return acc;
    }
};

struct Channel {
    Up<32> u1;
    Up<12> u2;
    Up<8> u3;
    Down<32, 15> d1;
    Down<12, 5> d2;
    Down<8, 3> d3;
    double a0[1] = {}, a1[2] = {}, a2[4] = {}, a3[8] = {};
    double pd[8] = {}, dr[64] = {};
    void reset() noexcept
    {
        u1.reset(); u2.reset(); u3.reset();
        d1.reset(); d2.reset(); d3.reset();
        std::memset(a0, 0, sizeof(a0)); std::memset(a1, 0, sizeof(a1));
        std::memset(a2, 0, sizeof(a2)); std::memset(a3, 0, sizeof(a3));
        std::memset(pd, 0, sizeof(pd)); std::memset(dr, 0, sizeof(dr));
    }
    double* top(int osf) noexcept { return osf > 4 ? a3 : osf > 2 ? a2 : osf > 1 ? a1 : a0; }
};

// the static curve the display draws (cp_curve in the GFX JSFX): clip of x * Input, mixed,
// times Output
struct Curve {
    double gin = 1.0, cl = 1.0, kt = 1.0, kw = 0.0, mix = 1.0, gout = 1.0;
    void set(const Params& p) noexcept
    {
        gin = std::pow(10.0, p.input / 20.0);
        cl = std::pow(10.0, p.ceiling / 20.0);
        kt = cl * (1.0 - p.knee / 100.0);
        kw = cl - kt;
        mix = p.mix / 100.0;
        gout = std::pow(10.0, p.output / 20.0);
    }
    double operator()(double x) const noexcept
    {
        double y = x * gin;
        const double a = std::fabs(y);
        if (a > kt)
            y = (y > 0.0 ? 1.0 : y < 0.0 ? -1.0 : 0.0) * (kw > 0.0 ? kt + kw * (1.0 - 2.0 / (std::exp(2.0 * ((a - kt) / kw)) + 1.0)) : cl);
        return (x * (1.0 - mix) + y * mix) * gout;
    }
};

class ClipperProcessor {
public:
    ClipperProcessor()
    {
        halfbandDesign(fE1, fD1, 63, 7.5);
        halfbandDesign(fE2, fD2, 23, 8.0);
        halfbandDesign(fE3, fD3, 15, 7.5);
    }

    // @init
    void init(double) noexcept
    {
        fOsPrev = -1;
        fPw = 0;
        fDw = 0;
    }

    // @slider
    void setParams(const Params& p) noexcept
    {
        fGin = std::pow(10.0, p.input / 20.0);
        fCl = std::pow(10.0, p.ceiling / 20.0);
        fKt = fCl * (1.0 - p.knee / 100.0);
        fKw = fCl - fKt;
        fMix = p.mix / 100.0;
        fGout = std::pow(10.0, p.output / 20.0);
        fLink = p.link;
        const int os = p.oversampling;
        fOsf = os <= 0 ? 1 : os == 1 ? 2 : os == 2 ? 4 : 8;
        fLat = latencyFor(os);
        fPad = os == 2 ? 2 : os >= 3 ? 6 : 0;
        // a new rate starts every filter and delay from silence
        if (fOsf != fOsPrev)
        {
            fL.reset();
            fR.reset();
            fPw = 0;
            fDw = 0;
            fOsPrev = fOsf;
        }
    }

    int latency() const noexcept { return fLat; }
    double inputGain() const noexcept { return fGin; }
    double clipGain() const noexcept { return fCg; }     // lowest clip gain of the last sample
    double ceiling() const noexcept { return fCl; }

    void processSample(double& l, double& r) noexcept
    {
        const double inL = l, inR = r;
        const double xl = inL * fGin, xr = inR * fGin;
        double* tl = fL.top(fOsf);
        double* tr = fR.top(fOsf);
        double o0, o1;

        // up to the clipper's rate
        if (fOsf > 1)
        {
            fL.u1.process(xl, fE1, 15, o0, o1); fL.a1[0] = o0; fL.a1[1] = o1;
            fR.u1.process(xr, fE1, 15, o0, o1); fR.a1[0] = o0; fR.a1[1] = o1;
            if (fOsf > 2)
            {
                for (int i = 0; i < 2; ++i)
                {
                    fL.u2.process(fL.a1[i], fE2, 5, o0, o1); fL.a2[2 * i] = o0; fL.a2[2 * i + 1] = o1;
                    fR.u2.process(fR.a1[i], fE2, 5, o0, o1); fR.a2[2 * i] = o0; fR.a2[2 * i + 1] = o1;
                }
                if (fOsf > 4)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        fL.u3.process(fL.a2[i], fE3, 3, o0, o1); fL.a3[2 * i] = o0; fL.a3[2 * i + 1] = o1;
                        fR.u3.process(fR.a2[i], fE3, 3, o0, o1); fR.a3[2 * i] = o0; fR.a3[2 * i + 1] = o1;
                    }
                }
            }
        }
        else
        {
            tl[0] = xl;
            tr[0] = xr;
        }

        // pad delay and the clipper, at the top rate
        double cg = 1.0;
        for (int i = 0; i < fOsf; ++i)
        {
            double cxl = tl[i], cxr = tr[i];
            if (fPad > 0)
            {
                fL.pd[fPw] = cxl;
                fR.pd[fPw] = cxr;
                const int j = (fPw - fPad + 8) % 8;
                cxl = fL.pd[j];
                cxr = fR.pd[j];
                fPw = (fPw + 1) % 8;
            }
            if (fLink < 1)
            {
                const double a = std::max(std::fabs(cxl), std::fabs(cxr));
                if (a > fKt)
                {
                    const double g = clipMag(a) / a;
                    cxl *= g;
                    cxr *= g;
                    cg = std::min(cg, g);
                }
            }
            else
            {
                double a = std::fabs(cxl);
                if (a > fKt)
                {
                    const double g = clipMag(a) / a;
                    cxl *= g;
                    cg = std::min(cg, g);
                }
                a = std::fabs(cxr);
                if (a > fKt)
                {
                    const double g = clipMag(a) / a;
                    cxr *= g;
                    cg = std::min(cg, g);
                }
            }
            tl[i] = cxl;
            tr[i] = cxr;
        }
        fCg = cg;

        // back down to the sample rate
        double yl, yr;
        if (fOsf > 1)
        {
            if (fOsf > 4)
                for (int i = 0; i < 4; ++i)
                {
                    fL.a2[i] = fL.d3.process(fL.a3[2 * i], fL.a3[2 * i + 1], fD3);
                    fR.a2[i] = fR.d3.process(fR.a3[2 * i], fR.a3[2 * i + 1], fD3);
                }
            if (fOsf > 2)
                for (int i = 0; i < 2; ++i)
                {
                    fL.a1[i] = fL.d2.process(fL.a2[2 * i], fL.a2[2 * i + 1], fD2);
                    fR.a1[i] = fR.d2.process(fR.a2[2 * i], fR.a2[2 * i + 1], fD2);
                }
            yl = fL.d1.process(fL.a1[0], fL.a1[1], fD1);
            yr = fR.d1.process(fR.a1[0], fR.a1[1], fD1);
        }
        else
        {
            yl = tl[0];
            yr = tr[0];
        }

        // dry signal delayed by the latency, mix, output
        fL.dr[fDw] = inL;
        fR.dr[fDw] = inR;
        const int j = (fDw - fLat + 64) % 64;
        const double dryL = fL.dr[j], dryR = fR.dr[j];
        fDw = (fDw + 1) % 64;
        l = (dryL * (1.0 - fMix) + yl * fMix) * fGout;
        r = (dryR * (1.0 - fMix) + yr * fMix) * fGout;
    }

private:
    double clipMag(double a) const noexcept
    {
        return fKw > 0.0 ? fKt + fKw * (1.0 - 2.0 / (std::exp(2.0 * ((a - fKt) / fKw)) + 1.0)) : fCl;
    }

    double fE1[32] = {}, fD1[32] = {}, fE2[12] = {}, fD2[12] = {}, fE3[8] = {}, fD3[8] = {};
    Channel fL, fR;
    double fGin = 1.0, fCl = 1.0, fKt = 1.0, fKw = 0.0, fMix = 1.0, fGout = 1.0, fCg = 1.0;
    int fLink = 1, fOsf = 1, fOsPrev = -1, fLat = 0, fPad = 0, fPw = 0, fDw = 0;
};

} // namespace nullclipper
