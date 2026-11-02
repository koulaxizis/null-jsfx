/*
 * NULL JSFX - Binaural Panner, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/binaural_panner.jsfx, computed in double in the EEL2 order: the
 * spherical head of Brown and Duda per source (air low-pass, rear shelf, elevation notch, 1/r
 * level, then per ear Woodworth's delay, glided, read with linear interpolation, and the head
 * shadow filter), six early reflections from the sources before their level, Output. No latency.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullbp {

static constexpr int kBL = 4096;

class Source {
public:
    void clear()
    {
        lp = x1 = x2 = y1 = y2 = n1 = n2 = m1 = m2 = 0.0;
        dlc = drc = 0.0;
        hl1 = hly = hr1 = hry = 0.0;
    }

    // the coefficients for azimuth az and elevation el (degrees)
    void setup(double az, double el, double dist, double hrad, double beta, double srate)
    {
        const double t = az * M_PI / 180.0, p = el * M_PI / 180.0;
        double f = std::min(0.45 * srate, 20000.0 * std::pow(10.0, -dist / 40.0));
        ac = 1.0 - std::exp(-2.0 * M_PI * f / srate);
        gd = 1.0 / dist;
        const double rear = std::max(0.0, -std::cos(t) * std::cos(p));
        double A = std::pow(10.0, -10.0 * rear / 40.0);
        double w = 2.0 * M_PI * std::min(4000.0, 0.45 * srate) / srate;
        double cw = std::cos(w);
        double al = std::sin(w) / 2.0 * std::sqrt(2.0);
        double a0 = (A + 1.0) - (A - 1.0) * cw + 2.0 * std::sqrt(A) * al;
        sb0 = A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * std::sqrt(A) * al) / a0;
        sb1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0;
        sb2 = A * ((A + 1.0) + (A - 1.0) * cw - 2.0 * std::sqrt(A) * al) / a0;
        sa1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0;
        sa2 = ((A + 1.0) - (A - 1.0) * cw - 2.0 * std::sqrt(A) * al) / a0;
        A = std::pow(10.0, -10.0 / 40.0);
        w = 2.0 * M_PI * std::min(6000.0 + 4000.0 * (el + 40.0) / 130.0, 0.45 * srate) / srate;
        cw = std::cos(w);
        al = std::sin(w) / (2.0 * 2.0);
        a0 = 1.0 + al / A;
        nb0 = (1.0 + al * A) / a0;
        nb1 = -2.0 * cw / a0;
        nb2 = (1.0 - al * A) / a0;
        na1 = -2.0 * cw / a0;
        na2 = (1.0 - al / A) / a0;
        const double se = std::sin(t) * std::cos(p);
        const double lat = std::asin(std::max(-1.0, std::min(1.0, se)));
        const double tau = hrad / 343.0 * (std::fabs(lat) + std::sin(std::fabs(lat))) * srate;
        dlt = lat > 0.0 ? tau : 0.0;
        drt = lat < 0.0 ? tau : 0.0;
        const double k = 2.0 * srate;
        double th = std::acos(std::max(-1.0, std::min(1.0, -se))) * 180.0 / M_PI;
        const double alL = 1.05 + 0.95 * std::cos(th / 150.0 * M_PI);
        th = std::acos(std::max(-1.0, std::min(1.0, se))) * 180.0 / M_PI;
        const double alR = 1.05 + 0.95 * std::cos(th / 150.0 * M_PI);
        lb0 = (alL * k + beta) / (k + beta);
        lb1 = (beta - alL * k) / (k + beta);
        la1 = (beta - k) / (k + beta);
        rb0 = (alR * k + beta) / (k + beta);
        rb1 = (beta - alR * k) / (k + beta);
        ra1 = la1;
    }

    void snap() { dlc = dlt; drc = drt; }

    // one sample: returns the signal for the reflections, the ears in yl and yr
    double run(double* buf, int pos, double x, double sk)
    {
        lp += ac * (x - lp);
        const double y = sb0 * lp + sb1 * x1 + sb2 * x2 - sa1 * y1 - sa2 * y2;
        x2 = x1;
        x1 = lp;
        y2 = y1;
        y1 = y;
        const double z = nb0 * y + nb1 * n1 + nb2 * n2 - na1 * m1 - na2 * m2;
        n2 = n1;
        n1 = y;
        m2 = m1;
        m1 = z;
        buf[pos] = z * gd;
        dlc += sk * (dlt - dlc);
        drc += sk * (drt - drc);
        const double xl = rd(buf, pos, dlc), xr = rd(buf, pos, drc);
        yl = lb0 * xl + lb1 * hl1 - la1 * hly;
        hl1 = xl;
        hly = yl;
        yr = rb0 * xr + rb1 * hr1 - ra1 * hry;
        hr1 = xr;
        hry = yr;
        return z;
    }

    double yl = 0.0, yr = 0.0;

private:
    static double rd(const double* buf, int pos, double d)
    {
        const double i = std::floor(d);
        const double fr = d - i;
        int p0 = pos - int(i);
        if (p0 < 0)
            p0 += kBL;
        int p1 = p0 - 1;
        if (p1 < 0)
            p1 += kBL;
        return buf[p0] + (buf[p1] - buf[p0]) * fr;
    }

    double ac = 0.0, gd = 1.0, sb0 = 1.0, sb1 = 0.0, sb2 = 0.0, sa1 = 0.0, sa2 = 0.0;
    double nb0 = 1.0, nb1 = 0.0, nb2 = 0.0, na1 = 0.0, na2 = 0.0, dlt = 0.0, drt = 0.0;
    double lb0 = 1.0, lb1 = 0.0, la1 = 0.0, rb0 = 1.0, rb1 = 0.0, ra1 = 0.0;
    double lp = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0, n1 = 0.0, n2 = 0.0, m1 = 0.0, m2 = 0.0;
    double dlc = 0.0, drc = 0.0, hl1 = 0.0, hly = 0.0, hr1 = 0.0, hry = 0.0;
};

class BinauralPanner {
public:
    BinauralPanner() : fBufA(kBL), fBufB(kBL) {}

    // @init (the reflection buffer: 30 ms, allocated here, never in process)
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::fill(fBufA.begin(), fBufA.end(), 0.0);
        std::fill(fBufB.begin(), fBufB.end(), 0.0);
        fRL = int(std::ceil(0.03 * sr)) + 2;
        fRef.assign(size_t(fRL), 0.0);
        fPos = fRpos = 0;
        fSk = 1.0 - std::exp(-1.0 / (0.02 * sr));
        static const double kT[6] = { 2.9, 5.3, 8.1, 11.6, 15.2, 19.7 };
        for (int i = 0; i < 6; ++i)
            fRd[i] = int(std::floor(kT[i] / 1000.0 * sr));
        fRlpC = 1.0 - std::exp(-2.0 * M_PI * std::min(5000.0, 0.45 * sr) / sr);
        fRlpL = fRlpR = 0.0;
        fA.clear();
        fB.clear();
        fFirst = true;
    }

    // @slider (Distance and Head Size as the JSFX sliders hold them)
    void setParams(double az, double el, double dist, double roomPct, double headCm, bool stereo, double width,
                   double outDb)
    {
        fRoom = roomPct / 100.0;
        const double hrad = headCm / 200.0;
        const double beta = 2.0 * 343.0 / hrad;
        fStereo = stereo;
        fOut = std::pow(10.0, outDb / 20.0);
        if (stereo)
        {
            fA.setup(az - width, el, dist, hrad, beta, fSr);
            fB.setup(az + width, el, dist, hrad, beta, fSr);
        }
        else
            fA.setup(az, el, dist, hrad, beta, fSr);
        // a new setting glides from where the ears are; the very first one starts there
        if (fFirst)
        {
            fA.snap();
            fB.snap();
            fFirst = false;
        }
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        static const double kG[6] = { 0.62, -0.55, 0.47, -0.41, 0.34, -0.28 };
        static const double kL[6] = { 1.0, 0.35, 0.8, 0.6, 1.0, 0.2 };
        static const double kR[6] = { 0.35, 1.0, 0.6, 0.8, 0.2, 1.0 };
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double l = inL[n], r = inR[n];
            double oL, oR, xm;
            if (fStereo)
            {
                const double xa = fA.run(fBufA.data(), fPos, l, fSk);
                const double xb = fB.run(fBufB.data(), fPos, r, fSk);
                oL = fA.yl + fB.yl;
                oR = fA.yr + fB.yr;
                xm = (xa + xb) * 0.5;
            }
            else
            {
                xm = fA.run(fBufA.data(), fPos, (l + r) * 0.5, fSk);
                oL = fA.yl;
                oR = fA.yr;
            }
            if (++fPos >= kBL)
                fPos = 0;

            // early reflections of the source before its distance level
            fRef[size_t(fRpos)] = xm;
            if (fRoom > 0.0)
            {
                double rl = 0.0, rr = 0.0;
                for (int i = 0; i < 6; ++i)
                {
                    int q = fRpos - fRd[i];
                    if (q < 0)
                        q += fRL;
                    const double v = fRef[size_t(q)] * kG[i];
                    rl += v * kL[i];
                    rr += v * kR[i];
                }
                fRlpL += fRlpC * (rl - fRlpL);
                fRlpR += fRlpC * (rr - fRlpR);
                oL += fRlpL * fRoom * 0.5;
                oR += fRlpR * fRoom * 0.5;
            }
            if (++fRpos >= fRL)
                fRpos = 0;

            outL[n] = static_cast<T>(oL * fOut);
            outR[n] = static_cast<T>(oR * fOut);
        }
    }

private:
    std::vector<double> fBufA, fBufB, fRef;
    Source fA, fB;
    double fSr = 48000.0, fSk = 0.0, fRlpC = 0.0, fRlpL = 0.0, fRlpR = 0.0, fRoom = 0.2, fOut = 1.0;
    int fPos = 0, fRpos = 0, fRL = 1, fRd[6] = {};
    bool fStereo = false, fFirst = true;
};

} // namespace nullbp
