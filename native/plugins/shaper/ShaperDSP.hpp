/*
 * NULL JSFX - Shaper, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/shaper.jsfx, computed in double like EEL2: Drive gain, then
 * (with Oversampling) 1 or 2 polyphase halfband FIR stages up (NULL Clipper's 63 and 23 taps), a
 * short pad delay so the latency is a whole number of samples, Bias and the transfer curve (a
 * 1024-step table of a monotone cubic through (0, 0) and up to 8 points, read with linear
 * interpolation), the same stages down, a 10 Hz DC blocker, Tone (RBJ low-pass), then Mix with
 * the dry input delayed by the latency and Output.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullshaper {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kTable = 1024;              // TL
static constexpr int kNumShapes = 8;

// the built-in points of each Shape (x, y pairs; x = 0 unused). Slot 0 is the negative half
// of Asymmetric (Custom reads the P sliders instead); 1 Soft .. 7 Asym, as shp_pt in the JSFX.
static const double kShapePoints[kNumShapes][16] = {
    { 0.2, 0.16, 0.5, 0.34, 1, 0.46, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0.1, 0.205, 0.25, 0.479, 0.4, 0.689, 0.6, 0.865, 0.8, 0.956, 1, 1, 0, 0, 0, 0 },
    { 0.4, 0.5, 0.72, 0.9, 0.8, 0.98, 0.85, 1, 1, 1, 0, 0, 0, 0, 0, 0 },
    { 0.05, 0.1, 0.2, 0.33, 0.4, 0.56, 0.65, 0.75, 1, 0.9, 0, 0, 0, 0, 0, 0 },
    { 0.25, 0.9, 0.5, -0.3, 0.75, 0.7, 1, -0.5, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0.167, 0.707, 0.333, 1, 0.5, 0.707, 0.667, 0, 0.833, -0.707, 1, -1, 0, 0, 0, 0 },
    { 0.18, 0.04, 0.26, 0.33, 0.43, 0.36, 0.51, 0.65, 0.68, 0.68, 0.76, 0.97, 1, 1, 0, 0 },
    { 0.1, 0.25, 0.3, 0.62, 0.6, 0.9, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
};

struct Params {
    int shape = 1;          // 0 Custom, 1 Soft, 2 Hard, 3 Tube, 4 Fold, 5 Sine, 6 Steps, 7 Asym
    double drive = 6.0, bias = 0.0;
    int symmetry = 0;       // 0 Symmetric, 1 Asymmetric
    int oversampling = 1;   // 0 off, 1 = 2x, 2 = 4x
    double tone = 20000.0, mix = 100.0, output = 0.0;
    double points[16] = { 0.1, 0.205, 0.25, 0.479, 0.4, 0.689, 0.6, 0.865, 0.8, 0.956, 1, 1, 0, 0, 0, 0 };
};

// latency in samples for an Oversampling setting (the JSFX's pdc_delay)
inline int latencyFor(int os) noexcept { return os <= 0 ? 0 : os == 1 ? 31 : 37; }

// lut_build: the table dst (kTable + 1 values over input 0..1) from 8 points src: (0, 0) plus
// the used points sorted by x (equal x: the first one counts), PCHIP segments, flat after the
// last point
inline void buildTable(const double* src, double* dst) noexcept
{
    double xs[10], ys[10], ms[10], ds[10];
    xs[0] = 0.0;
    ys[0] = 0.0;
    int n = 1;
    for (int i = 0; i < 8; ++i)
    {
        const double x = src[2 * i], y = src[2 * i + 1];
        if (x > 0.0)
        {
            int k = n;
            while (k > 1 && xs[k - 1] > x)
            {
                xs[k] = xs[k - 1];
                ys[k] = ys[k - 1];
                --k;
            }
            xs[k] = x;
            ys[k] = y;
            ++n;
        }
    }
    int m = 1;
    for (int i = 1; i < n; ++i)
        if (xs[i] > xs[m - 1])
        {
            xs[m] = xs[i];
            ys[m] = ys[i];
            ++m;
        }
    n = m;
    if (n < 2)
    {
        xs[1] = 1.0;
        ys[1] = 1.0;
        n = 2;
    }
    for (int k = 0; k < n - 1; ++k)
        ds[k] = (ys[k + 1] - ys[k]) / (xs[k + 1] - xs[k]);
    ms[0] = ds[0];
    ms[n - 1] = ds[n - 2];
    for (int k = 1; k < n - 1; ++k)
    {
        if (ds[k - 1] * ds[k] > 0.0)
        {
            const double h0 = xs[k] - xs[k - 1];
            const double h1 = xs[k + 1] - xs[k];
            const double w1 = 2.0 * h1 + h0;
            const double w2 = h1 + 2.0 * h0;
            ms[k] = (w1 + w2) / (w1 / ds[k - 1] + w2 / ds[k]);
        }
        else
            ms[k] = 0.0;
    }
    int j = 0;
    for (int k = 0; k <= kTable; ++k)
    {
        const double u = double(k) / double(kTable);
        while (j < n - 2 && u > xs[j + 1])
            ++j;
        if (u >= xs[n - 1])
            dst[k] = ys[n - 1];
        else
        {
            const double h = xs[j + 1] - xs[j];
            const double t = (u - xs[j]) / h;
            const double t2 = t * t;
            const double t3 = t2 * t;
            dst[k] = (2.0 * t3 - 3.0 * t2 + 1.0) * ys[j] + (t3 - 2.0 * t2 + t) * h * ms[j]
                     + (3.0 * t2 - 2.0 * t3) * ys[j + 1] + (t3 - t2) * h * ms[j + 1];
        }
    }
}

// shape(v): the table read with linear interpolation, mirrored (or the negative table) below 0
inline double lookup(const double* tp, const double* tn, bool asym, double v) noexcept
{
    double a = std::min(std::fabs(v), 1.0);
    const double p = a * kTable;
    const double i = std::min(std::floor(p), double(kTable - 1));
    const double f = p - i;
    const double* tb = (v < 0.0 && asym) ? tn : tp;
    const int ii = int(i);
    a = tb[ii] + (tb[ii + 1] - tb[ii]) * f;
    return v < 0.0 ? -a : a;
}

// the curve as the display shows it (shaper input -1..1 -> output), for the interfaces
struct Curve {
    double tp[kTable + 1] = {}, tn[kTable + 1] = {};
    bool asym = false;
    void set(int shape, int symmetry, const double* points) noexcept
    {
        buildTable(shape <= 0 ? points : kShapePoints[std::min(shape, kNumShapes - 1)], tp);
        buildTable(kShapePoints[0], tn);
        asym = symmetry > 0 || shape >= 7;
    }
    double operator()(double v) const noexcept { return lookup(tp, tn, asym, v); }
};

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
    Down<32, 15> d1;
    Down<12, 5> d2;
    double a0[1] = {}, a1[2] = {}, a2[4] = {};
    double pd[8] = {}, dr[64] = {};
    // DC blocker and Tone states (reset in init only, as in the JSFX)
    double dcx = 0.0, dcy = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    void reset() noexcept
    {
        u1.reset(); u2.reset();
        d1.reset(); d2.reset();
        std::memset(a0, 0, sizeof(a0)); std::memset(a1, 0, sizeof(a1)); std::memset(a2, 0, sizeof(a2));
        std::memset(pd, 0, sizeof(pd)); std::memset(dr, 0, sizeof(dr));
    }
    double* top(int osf) noexcept { return osf > 2 ? a2 : osf > 1 ? a1 : a0; }
};

class ShaperProcessor {
public:
    ShaperProcessor()
    {
        halfbandDesign(fE1, fD1, 63, 7.5);
        halfbandDesign(fE2, fD2, 23, 8.0);
        buildTable(kShapePoints[0], fTn);
        buildTable(kShapePoints[1], fTp);
    }

    // @init
    void init(double sr) noexcept
    {
        fSr = sr;
        fOsPrev = -1;
        fPw = 0;
        fDw = 0;
        fDcR = std::exp(-2.0 * kPi * 10.0 / sr);
        for (Channel* c : { &fL, &fR })
            c->dcx = c->dcy = c->x1 = c->x2 = c->y1 = c->y2 = 0.0;
    }

    // @slider
    void setParams(const Params& p) noexcept
    {
        const int shp = p.shape;
        fGin = std::pow(10.0, p.drive / 20.0);
        fBias = p.bias / 100.0;
        fAsym = p.symmetry > 0 || shp > 6;
        fMix = p.mix / 100.0;
        fGout = std::pow(10.0, p.output / 20.0);
        buildTable(shp <= 0 ? p.points : kShapePoints[std::min(shp, kNumShapes - 1)], fTp);
        fToneOn = p.tone < 20000.0;
        const double w0 = 2.0 * kPi * std::min(p.tone, 0.45 * fSr) / fSr;
        const double cw = std::cos(w0);
        const double al = std::sin(w0) / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + al;
        fB0 = (1.0 - cw) / 2.0 / a0;
        fB1 = (1.0 - cw) / a0;
        fA1 = -2.0 * cw / a0;
        fA2 = (1.0 - al) / a0;
        const int os = p.oversampling;
        fOsf = os <= 0 ? 1 : os == 1 ? 2 : 4;
        fLat = latencyFor(os);
        fPad = os >= 2 ? 2 : 0;
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
    double bias() const noexcept { return fBias; }

    void processSample(double& l, double& r) noexcept
    {
        const double inL = l, inR = r;
        const double xl = inL * fGin, xr = inR * fGin;
        double* tl = fL.top(fOsf);
        double* tr = fR.top(fOsf);
        double o0, o1;

        // up to the shaper's rate
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
            }
        }
        else
        {
            tl[0] = xl;
            tr[0] = xr;
        }

        // pad delay, Bias and the curve, at the top rate
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
            tl[i] = lookup(fTp, fTn, fAsym, cxl + fBias);
            tr[i] = lookup(fTp, fTn, fAsym, cxr + fBias);
        }

        // back down to the sample rate
        double yl, yr;
        if (fOsf > 1)
        {
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

        // DC blocker, Tone
        yl = post(fL, yl);
        yr = post(fR, yr);

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
    double post(Channel& c, double y) noexcept
    {
        c.dcy = y - c.dcx + fDcR * c.dcy;
        c.dcx = y;
        y = c.dcy;
        if (fToneOn)
        {
            const double t = fB0 * y + fB1 * c.x1 + fB0 * c.x2 - fA1 * c.y1 - fA2 * c.y2;
            c.x2 = c.x1;
            c.x1 = y;
            c.y2 = c.y1;
            c.y1 = t;
            y = t;
        }
        return y;
    }

    double fE1[32] = {}, fD1[32] = {}, fE2[12] = {}, fD2[12] = {};
    double fTp[kTable + 1] = {}, fTn[kTable + 1] = {};
    Channel fL, fR;
    double fSr = 44100.0, fGin = 1.0, fBias = 0.0, fMix = 1.0, fGout = 1.0, fDcR = 0.999;
    double fB0 = 0.0, fB1 = 0.0, fA1 = 0.0, fA2 = 0.0;
    bool fAsym = false, fToneOn = false;
    int fOsf = 1, fOsPrev = -1, fLat = 0, fPad = 0, fPw = 0, fDw = 0;
};

} // namespace nullshaper
