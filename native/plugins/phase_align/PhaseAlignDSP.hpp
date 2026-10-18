/*
 * NULL JSFX - Phase Align, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/phase_align.jsfx in double, in the JSFX's order of operations.
 * Source Right to Left moves the right channel against the left (a negative delay delays the
 * left one instead, no latency); Source Track to Sidechain moves the track against the
 * reference on inputs 3/4, through a fixed latency of ceil(10 ms) + 1 samples less the delay
 * (reported). Fractional delays: 4-point Catmull-Rom; Invert per channel; Rotation: two
 * first-order all-passes turning the phase by the set angle at Focus; Listen Mono Sum adds the
 * moved signal to its reference, halved. ca()/cb() are the moved signal and its reference for
 * the correlation meter.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullphase {

static constexpr double kPi = 3.141592653589793;
static constexpr int kRing = 16384;

struct Params {
    bool sidechain = false;
    double delayMs = 0.0, fine = 0.0;
    bool invL = false, invR = false;
    double rotation = 0.0, focus = 100.0;
    bool sum = false;
};

// the @slider values derived from the sliders
struct Derived {
    double d = 0.0, il = 1.0, ir = 1.0, t = 1.0, a = 0.0, dl = 0.0, dr = 0.0, dt = 0.0;
    bool rot = false;
    int lat = 0;

    void compute(const Params& p, double sr)
    {
        lat = int(std::ceil(sr * 0.01)) + 1;
        d = p.delayMs * sr / 1000.0 + p.fine;
        il = p.invL ? -1.0 : 1.0;
        ir = p.invR ? -1.0 : 1.0;
        rot = p.rotation > 0.0 && p.rotation < 360.0;
        if (rot)
        {
            t = std::tan(kPi * p.focus / sr) / std::tan(p.rotation * kPi / 720.0);
            a = (t - 1.0) / (t + 1.0);
        }
        dl = std::max(0.0, -d);
        dr = std::max(0.0, d);
        dt = double(lat) + d;
    }
};

// the phase of the moved signal against its reference at f Hz in degrees, -180..180 (display)
inline double phaseDeg(const Params& p, const Derived& v, double sr, double f)
{
    double ph = -360.0 * f * v.d / sr;
    if (v.rot)
    {
        const double t = std::tan(kPi * std::min(f, sr * 0.4999) / sr);
        ph -= 4.0 * std::atan(t / v.t) * 180.0 / kPi;
    }
    if (p.sidechain ? v.il < 0.0 : v.il != v.ir)
        ph += 180.0;
    return ph - 360.0 * std::floor((ph + 180.0) / 360.0);
}

class PhaseAlignProcessor {
public:
    PhaseAlignProcessor() : fRL(kRing), fRR(kRing), fKL(kRing), fKR(kRing) {}

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        std::fill(fRL.begin(), fRL.end(), 0.0);
        std::fill(fRR.begin(), fRR.end(), 0.0);
        std::fill(fKL.begin(), fKL.end(), 0.0);
        std::fill(fKR.begin(), fKR.end(), 0.0);
        fWp = 0;
        for (Ap* ap : { &fL1, &fL2, &fR1, &fR2 })
            *ap = Ap();
        fV.compute(fP, fSr);
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        fV.compute(p, fSr);
    }

    // pdc_delay
    int latency() const { return fP.sidechain ? fV.lat : 0; }

    // @sample: l, r the track, kl, kr the sidechain
    void processSample(double& l, double& r, double kl, double kr)
    {
        fRL[fWp] = l * fV.il;
        fRR[fWp] = r * fV.ir;
        fKL[fWp] = kl;
        fKR[fWp] = kr;
        double ol, orr;
        if (!fP.sidechain)
        {
            ol = read(fRL, fV.dl);
            orr = read(fRR, fV.dr);
            if (fV.rot)
            {
                orr = fR1.run(orr, fV.a);
                orr = fR2.run(orr, fV.a);
            }
            fCa = orr;
            fCb = ol;
            if (fP.sum)
            {
                ol = (ol + orr) * 0.5;
                orr = ol;
            }
        }
        else
        {
            ol = read(fRL, fV.dt);
            orr = read(fRR, fV.dt);
            if (fV.rot)
            {
                ol = fL1.run(ol, fV.a);
                ol = fL2.run(ol, fV.a);
                orr = fR1.run(orr, fV.a);
                orr = fR2.run(orr, fV.a);
            }
            const int k = (fWp - fV.lat + kRing) & (kRing - 1);
            const double rkl = fKL[k], rkr = fKR[k];
            fCa = (ol + orr) * 0.5;
            fCb = (rkl + rkr) * 0.5;
            if (fP.sum)
            {
                ol = (ol + rkl) * 0.5;
                orr = (orr + rkr) * 0.5;
            }
        }
        fWp = (fWp + 1) & (kRing - 1);
        l = ol;
        r = orr;
    }

    double ca() const { return fCa; }
    double cb() const { return fCb; }
    const Derived& derived() const { return fV; }

private:
    struct Ap {
        double x1 = 0.0, y1 = 0.0;
        double run(double x, double a)
        {
            const double y = a * x + x1 - a * y1;
            x1 = x;
            y1 = y;
            return y;
        }
    };

    // buf delayed by d samples (d >= 0): whole samples exact, fractions by Catmull-Rom
    double read(const std::vector<double>& buf, double d) const
    {
        const double nn = std::floor(d);
        const double f = d - nn;
        const int n = int(nn);
        const double y0 = buf[(fWp - n + 16384) & 16383];
        if (!(f > 0.0))
            return y0;
        const double ym1 = n > 0 ? buf[(fWp - n + 16385) & 16383] : y0;
        const double y1 = buf[(fWp - n + 16383) & 16383];
        const double y2 = buf[(fWp - n + 16382) & 16383];
        const double c1 = 0.5 * (y1 - ym1);
        const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
        const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + y0;
    }

    std::vector<double> fRL, fRR, fKL, fKR;
    Params fP;
    Derived fV;
    Ap fL1, fL2, fR1, fR2;
    double fSr = 48000.0, fCa = 0.0, fCb = 0.0;
    int fWp = 0;
};

// the correlation meter (display only): 300 ms averages, full band and below 150 Hz
struct Correlation {
    double k = 0.0, lk = 0.0, la = 0.0, lb = 0.0, ab = 0.0, aa = 0.0, bb = 0.0, lab = 0.0, laa = 0.0, lbb = 0.0;
    void setup(double sr)
    {
        k = 1.0 - std::exp(-1.0 / (0.3 * sr));
        lk = 1.0 - std::exp(-2.0 * kPi * 150.0 / sr);
        la = lb = ab = aa = bb = lab = laa = lbb = 0.0;
    }
    void push(double a, double b)
    {
        la += lk * (a - la);
        lb += lk * (b - lb);
        ab += k * (a * b - ab);
        aa += k * (a * a - aa);
        bb += k * (b * b - bb);
        lab += k * (la * lb - lab);
        laa += k * (la * la - laa);
        lbb += k * (lb * lb - lbb);
    }
    static double corr(double xy, double xx, double yy)
    {
        const double p = xx * yy;
        return p > 1e-14 ? std::max(-1.0, std::min(1.0, xy / std::sqrt(p))) : 0.0;
    }
    double full() const { return corr(ab, aa, bb); }
    double low() const { return corr(lab, laa, lbb); }
};

} // namespace nullphase
