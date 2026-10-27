/*
 * NULL JSFX - Amp Simulator, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/amp_simulator.jsfx, computed in double like EEL2: a tight
 * high-pass and the Bright shelf at the base rate, then 2x or 4x oversampling (polyphase IIR
 * half-band filters: two all-pass chains per 2x step) around two to four asymmetric, inverting
 * triode stages (each followed by a coupling-cap high-pass and a Miller-cap low-pass), a
 * Fender/Marshall-style passive tone stack (D. Yeh's third-order model, bilinear transform),
 * the power amp's resonance and Presence filters, a push-pull soft clipper with supply sag, and
 * a DC blocker, and a noise gate keyed from the input (off at -90 dB). No cabinet: put the
 * Cabinet Simulator after it.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>

namespace nullamp {

// half-band all-pass coefficients: 8 for the first 2x step (-99 dB), 4 for the second (-76 dB)
static const double kCoefA[8] = { 0.040633460924193, 0.150505129022675, 0.300757055991874, 0.460774504961451,
                                  0.609524314896188, 0.738503841118857, 0.849223810392066, 0.949742783705000 };
static const double kCoefB[4] = { 0.070765948976184, 0.257853078513453, 0.513167574748793, 0.817317354417137 };

static const char* const kModelNames[] = { "Clean", "Crunch", "Lead", "Bass", "Jazz Clean", "British", "Modern High Gain", "Fuzz" };

// the JSFX's tri(): a triode-like asymmetric curve, soft towards +1, harder towards -0.5
inline double tri(double x) noexcept
{
    return x > 0 ? 1 - std::exp(-x) : (std::exp(2 * x) - 1) * 0.5;
}

// the JSFX's @slider maths: every coefficient for the settings (also used by the display)
struct Setup {
    int ns = 3;                      // gain stages
    double g[4] = {}, b[4] = {}, t0[4] = {}, ha[4] = {}, lc[4] = {};
    double preDb = 17;               // stage 1 gain in dB (the Gain knob)
    double tb0 = 0, tb1 = 0, tb2 = 0, tb3 = 0, ta1 = 0, ta2 = 0, ta3 = 0, tsMk = 1;
    double rc[5] = {}, pc[5] = {};   // resonance and presence biquads (b0 b1 b2 a1 a2)
    double tiA = 0, brA = 0, brG = 0, dcA = 0, paD = 1, sag = 0, sgA = 0, sgR = 0, outG = 0.5;
    int os = 4;
    double fsOs = 192000;

    static void peak(double* c, double f, double q, double db, double fs) noexcept
    {
        const double A = std::pow(10.0, db / 40.0), w = 2 * M_PI * std::min(f, 0.45 * fs) / fs;
        const double cs = std::cos(w), al = std::sin(w) / (2 * q);
        const double a0 = 1 + al / A;
        c[0] = (1 + al * A) / a0; c[1] = -2 * cs / a0; c[2] = (1 - al * A) / a0;
        c[3] = -2 * cs / a0; c[4] = (1 - al / A) / a0;
    }

    static void highShelf(double* c, double f, double db, double fs) noexcept
    {
        const double A = std::pow(10.0, db / 40.0), w = 2 * M_PI * std::min(f, 0.45 * fs) / fs;
        const double cs = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0), sa = 2 * std::sqrt(A) * al;
        const double a0 = (A + 1) - (A - 1) * cs + sa;
        c[0] = A * ((A + 1) + (A - 1) * cs + sa) / a0;
        c[1] = -2 * A * ((A - 1) + (A + 1) * cs) / a0;
        c[2] = A * ((A + 1) + (A - 1) * cs - sa) / a0;
        c[3] = 2 * ((A - 1) - (A + 1) * cs) / a0;
        c[4] = ((A + 1) - (A - 1) * cs - sa) / a0;
    }

    void compute(int model, double gain, double bass, double mid, double treble, double presence, double master,
                 bool bright, double sagPct, bool os4, double outDb, double sr) noexcept
    {
        os = os4 ? 4 : 2;
        fsOs = sr * os;
        // per model: stages, Gain range (dB), stage gains, biases, tight / coupling / Miller
        // corners, tone stack parts (nF, kOhm), stack makeup, power amp resonance and drive, output trim (dB)
        double gmin, gmax, tight, C1, C2, C3, R1, R2, R3, R4, mk, resF, resG, paBase, trim;
        double hp[4] = { 0, 0, 0, 0 }, lp[4] = { 0, 0, 0, 0 };
        g[1] = g[2] = g[3] = 0; b[0] = b[1] = b[2] = b[3] = 0;
        if (model == 0)
        {
            ns = 2; gmin = -12; gmax = 18; g[1] = 1.5;
            b[0] = 0.05; b[1] = 0.15;
            tight = 30; hp[0] = 20; hp[1] = 30; lp[0] = 14000; lp[1] = 10000;
            C1 = 0.25; C2 = 20; C3 = 20; R1 = 250; R2 = 1000; R3 = 25; R4 = 56;
            mk = 6; resF = 100; resG = 2; paBase = 1.5; trim = 0;
        }
        else if (model == 1)
        {
            ns = 3; gmin = 0; gmax = 34; g[1] = 3; g[2] = 2.5;
            b[0] = 0.1; b[1] = 0.2; b[2] = 0.3;
            tight = 60; hp[0] = 30; hp[1] = 80; hp[2] = 40; lp[0] = 12000; lp[1] = 9000; lp[2] = 7500;
            C1 = 0.47; C2 = 22; C3 = 22; R1 = 220; R2 = 1000; R3 = 22; R4 = 33;
            mk = 5; resF = 90; resG = 3; paBase = 0.8; trim = 0;
        }
        else if (model == 2)
        {
            ns = 4; gmin = 6; gmax = 46; g[1] = 4; g[2] = 3; g[3] = 2;
            b[0] = 0.1; b[1] = 0.2; b[2] = 0.25; b[3] = 0.3;
            tight = 110; hp[0] = 60; hp[1] = 140; hp[2] = 90; hp[3] = 40;
            lp[0] = 10000; lp[1] = 8000; lp[2] = 7000; lp[3] = 6500;
            C1 = 0.47; C2 = 22; C3 = 22; R1 = 220; R2 = 1000; R3 = 22; R4 = 33;
            mk = 5; resF = 85; resG = 4; paBase = 0.8; trim = -2;
        }
        else if (model == 3)
        {
            ns = 2; gmin = -6; gmax = 28; g[1] = 2.5;
            b[0] = 0.05; b[1] = 0.25;
            tight = 15; hp[0] = 15; hp[1] = 20; lp[0] = 9000; lp[1] = 6000;
            C1 = 0.33; C2 = 47; C3 = 33; R1 = 250; R2 = 1000; R3 = 25; R4 = 47;
            mk = 6; resF = 60; resG = 2; paBase = 0.7; trim = 2;
        }
        else if (model == 4) // Jazz Clean
        {
            ns = 2; gmin = -18; gmax = 10; g[1] = 1.2;
            b[0] = 0.03; b[1] = 0.1;
            tight = 25; hp[0] = 15; hp[1] = 25; lp[0] = 9000; lp[1] = 6500;
            C1 = 0.18; C2 = 22; C3 = 22; R1 = 250; R2 = 1000; R3 = 25; R4 = 100;
            mk = 7; resF = 90; resG = 1.5; paBase = 1.1; trim = 5;
        }
        else if (model == 5) // British
        {
            ns = 3; gmin = -3; gmax = 30; g[1] = 2.5; g[2] = 2;
            b[0] = 0.15; b[1] = 0.25; b[2] = 0.35;
            tight = 40; hp[0] = 20; hp[1] = 50; hp[2] = 30; lp[0] = 14000; lp[1] = 11000; lp[2] = 9000;
            C1 = 0.5; C2 = 22; C3 = 22; R1 = 220; R2 = 1000; R3 = 25; R4 = 47;
            mk = 5; resF = 75; resG = 2; paBase = 1.4; trim = -1;
        }
        else if (model == 6) // Modern High Gain
        {
            ns = 4; gmin = 10; gmax = 52; g[1] = 5; g[2] = 3.5; g[3] = 2.5;
            b[0] = 0.08; b[1] = 0.15; b[2] = 0.2; b[3] = 0.25;
            tight = 150; hp[0] = 80; hp[1] = 180; hp[2] = 120; hp[3] = 60;
            lp[0] = 9000; lp[1] = 7500; lp[2] = 6500; lp[3] = 6000;
            C1 = 0.5; C2 = 22; C3 = 47; R1 = 220; R2 = 1000; R3 = 10; R4 = 33;
            mk = 6; resF = 80; resG = 5; paBase = 0.7; trim = -1.5;
        }
        else // Fuzz
        {
            ns = 2; gmin = 12; gmax = 54; g[1] = 6;
            b[0] = 0.5; b[1] = 0.6;
            tight = 15; hp[0] = 10; hp[1] = 15; lp[0] = 7000; lp[1] = 5000;
            C1 = 0.47; C2 = 47; C3 = 47; R1 = 100; R2 = 1000; R3 = 10; R4 = 22;
            mk = 8; resF = 70; resG = 3; paBase = 0.9; trim = -3;
        }
        preDb = gmin + (gmax - gmin) * gain / 10;
        g[0] = std::pow(10.0, preDb / 20);
        for (int k = 0; k < 4; ++k)
        {
            t0[k] = tri(b[k]);
            ha[k] = std::exp(-2 * M_PI * hp[k] / fsOs);
            lc[k] = 1 - std::exp(-2 * M_PI * lp[k] / fsOs);
        }

        // tone stack (Yeh): treble linear, bass audio taper, mid linear
        const double t = treble / 10, m = mid / 10, l = bass / 10 * (bass / 10);
        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
            + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
            + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
            + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
            - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
            + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + t * C1 * C2 * C3 * R1 * R3 * R4
            - t * m * C1 * C2 * C3 * R1 * R3 * R4 + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
            + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
            + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
            + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
            - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
            + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
            + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;
        // bilinear transform, s = c (1 - 1/z) / (1 + 1/z), in units of nF x kOhm = 1 us
        const double c = 2 * fsOs / 1000000, c2 = c * c, c3 = c2 * c;
        const double A0 = 1 + a1 * c + a2 * c2 + a3 * c3;
        tb0 = (b1 * c + b2 * c2 + b3 * c3) / A0;
        tb1 = (b1 * c - b2 * c2 - 3 * b3 * c3) / A0;
        tb2 = (-b1 * c - b2 * c2 + 3 * b3 * c3) / A0;
        tb3 = (-b1 * c + b2 * c2 - b3 * c3) / A0;
        ta1 = (3 + a1 * c - a2 * c2 - 3 * a3 * c3) / A0;
        ta2 = (3 - a1 * c - a2 * c2 + 3 * a3 * c3) / A0;
        ta3 = (1 - a1 * c + a2 * c2 - a3 * c3) / A0;
        tsMk = std::pow(10.0, mk / 20);

        peak(rc, resF, 0.8, resG, fsOs);
        highShelf(pc, 3000, (presence - 5) * 1.6, fsOs);
        tiA = std::exp(-2 * M_PI * tight / sr);
        brA = std::exp(-2 * M_PI * 1800 / sr);
        brG = bright ? std::pow(10.0, 8 * (1 - 0.085 * gain) / 20) - 1 : 0;
        dcA = std::exp(-2 * M_PI * 10 / sr);
        paD = paBase * std::pow(10.0, (master - 5) * 2.4 / 20);
        sag = sagPct / 100 * 1.5;
        sgA = 1 - std::exp(-1 / (0.003 * fsOs));
        sgR = 1 - std::exp(-1 / (0.12 * fsOs));
        outG = 0.5 * std::pow(10.0, (trim + outDb) / 20);
    }

    // display: the tone shaping (Bright, tone stack with makeup, resonance, presence) at f Hz, dB
    double responseDb(double f, double sr) const noexcept
    {
        const double w = 2 * M_PI * std::min(f, sr * 0.4999) / fsOs;
        const std::complex<double> z1 = std::polar(1.0, -w), z2 = z1 * z1, z3 = z2 * z1;
        std::complex<double> h = (tb0 + tb1 * z1 + tb2 * z2 + tb3 * z3) / (1.0 + ta1 * z1 + ta2 * z2 + ta3 * z3) * tsMk;
        h *= (rc[0] + rc[1] * z1 + rc[2] * z2) / (1.0 + rc[3] * z1 + rc[4] * z2);
        h *= (pc[0] + pc[1] * z1 + pc[2] * z2) / (1.0 + pc[3] * z1 + pc[4] * z2);
        const std::complex<double> b1 = std::polar(1.0, -2 * M_PI * std::min(f, sr * 0.4999) / sr);
        h *= 1.0 + brG * brA * (1.0 - b1) / (1.0 - brA * b1);
        return 10 * std::log10(std::max(std::norm(h), 1e-20));
    }

    // display: the preamp's static curve (stages without their filters) at input x
    double xfer(double x) const noexcept
    {
        double s = x;
        for (int k = 0; k < ns; ++k)
            s = t0[k] - tri(g[k] * s + b[k]);
        return s;
    }
};

class AmpSimulatorProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        reset();
        apply();
    }

    void reset()
    {
        std::memset(fSt, 0, sizeof(fSt));
        fGtEnv = 0; fGtG = 1; fGtOpen = 1; fGtCnt = 0;
    }

    // @slider
    void setParams(int model, double gain, double bass, double mid, double treble, double presence, double master,
                   bool bright, double sagPct, bool os4, double outDb, double gateDb = -90.0)
    {
        fModel = model; fGain = gain; fBass = bass; fMid = mid; fTreble = treble; fPres = presence;
        fMaster = master; fBright = bright; fSag = sagPct; fOs4 = os4; fOut = outDb;
        apply();
        // noise gate (-90 dB = off): 50 ms key release, 1 ms open, 80 ms close, 30 ms hold
        fGateOn = gateDb > -90.0;
        fGtTh = std::pow(10.0, gateDb / 20.0);
        fGtRel = std::exp(-1.0 / (0.05 * fSr));
        fGtGa = 1.0 - std::exp(-1.0 / (0.001 * fSr));
        fGtGr = 1.0 - std::exp(-1.0 / (0.08 * fSr));
        fGtHold = std::floor(0.03 * fSr);
    }

    const Setup& setup() const noexcept { return fS; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double xl = inL[n], xr = inR[n];
            if (fGateOn)
            {
                // noise gate key: the input peak, held and released over 50 ms
                const double x = std::max(std::fabs(xl), std::fabs(xr));
                fGtEnv = std::max(x, fGtEnv * fGtRel);
                if (fGtEnv > fGtTh)
                {
                    fGtOpen = 1;
                    fGtCnt = fGtHold;
                }
                else if (fGtEnv < fGtTh * 0.5)
                {
                    if (fGtCnt > 0)
                        fGtCnt -= 1;
                    else
                        fGtOpen = 0;
                }
                fGtG += (fGtOpen != 0 ? fGtGa : fGtGr) * (fGtOpen - fGtG);
            }
            double yl = chan(xl, fSt[0]), yr = chan(xr, fSt[1]);
            if (fGateOn)
            {
                yl *= fGtG;
                yr *= fGtG;
            }
            outL[n] = static_cast<T>(yl);
            outR[n] = static_cast<T>(yr);
        }
    }

private:
    // per channel state, laid out as in the JSFX's memory block
    enum { kUpA = 0, kDnA = 16, kUpB = 32, kDnB = 40, kTight = 48, kBright = 50, kDc = 52, kStage = 56,
           kStack = 72, kRes = 78, kPres = 82, kEnv = 86, kSize = 96 };

    // hb(): one 2x half-band step; the even and odd inputs run through the two all-pass paths
    void hb(const double* c, int n, double* s, double e, double o) noexcept
    {
        for (int i = 0; i < n; i += 2)
        {
            double t = s[2 * i];
            s[2 * i] = e;
            e = (e - s[2 * i + 1]) * c[i] + t;
            s[2 * i + 1] = e;
            t = s[2 * i + 2];
            s[2 * i + 2] = o;
            o = (o - s[2 * i + 3]) * c[i + 1] + t;
            s[2 * i + 3] = o;
        }
        fE = e;
        fO = o;
    }

    static double stage(double x, double* s, double g, double b, double t0, double ha, double lc) noexcept
    {
        const double v = t0 - tri(g * x + b);
        const double y = ha * (s[1] + v - s[0]);
        s[0] = v;
        s[1] = y;
        s[2] += lc * (y - s[2]);
        return s[2];
    }

    static double bq(double x, double* s, const double* c) noexcept
    {
        const double y = c[0] * x + c[1] * s[0] + c[2] * s[1] - c[3] * s[2] - c[4] * s[3];
        s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
        return y;
    }

    // os_run(): everything that runs at the oversampled rate
    double osRun(double x, double* s) const noexcept
    {
        const Setup& p = fS;
        double v = stage(x, s + kStage, p.g[0], p.b[0], p.t0[0], p.ha[0], p.lc[0]);
        if (p.ns > 1) v = stage(v, s + kStage + 4, p.g[1], p.b[1], p.t0[1], p.ha[1], p.lc[1]);
        if (p.ns > 2) v = stage(v, s + kStage + 8, p.g[2], p.b[2], p.t0[2], p.ha[2], p.lc[2]);
        if (p.ns > 3) v = stage(v, s + kStage + 12, p.g[3], p.b[3], p.t0[3], p.ha[3], p.lc[3]);
        double* ts = s + kStack; // x1 x2 x3 y1 y2 y3
        const double y = p.tb0 * v + p.tb1 * ts[0] + p.tb2 * ts[1] + p.tb3 * ts[2] - p.ta1 * ts[3] - p.ta2 * ts[4] - p.ta3 * ts[5];
        ts[2] = ts[1]; ts[1] = ts[0]; ts[0] = v;
        ts[5] = ts[4]; ts[4] = ts[3]; ts[3] = y;
        v = y * p.tsMk;
        v = bq(v, s + kRes, p.rc);
        v = bq(v, s + kPres, p.pc);
        v *= p.paD / (1 + p.sag * s[kEnv]);
        // push-pull power stage (Pade tanh, flat beyond +-3)
        v = v > 3 ? 1 : v < -3 ? -1 : v * (27 + v * v) / (27 + 9 * v * v);
        const double a = std::fabs(v);
        s[kEnv] += (a > s[kEnv] ? p.sgA : p.sgR) * (a - s[kEnv]);
        return v;
    }

    double chan(double x, double* s) noexcept
    {
        const Setup& p = fS;
        // tight high-pass, then the Bright shelf
        double v = p.tiA * (s[kTight + 1] + x - s[kTight]);
        s[kTight] = x; s[kTight + 1] = v;
        s[kBright + 1] = p.brA * (s[kBright + 1] + v - s[kBright]);
        s[kBright] = v;
        v += p.brG * s[kBright + 1];
        if (p.os == 2)
        {
            hb(kCoefA, 8, s + kUpA, v, v);
            const double u0 = osRun(fE, s), u1 = osRun(fO, s);
            hb(kCoefA, 8, s + kDnA, u1, u0);
            v = 0.5 * (fE + fO);
        }
        else
        {
            hb(kCoefA, 8, s + kUpA, v, v);
            const double u0 = fE, u1 = fO;
            hb(kCoefB, 4, s + kUpB, u0, u0);
            const double w0 = fE, w1 = fO;
            hb(kCoefB, 4, s + kUpB, u1, u1);
            const double w2 = fE, w3 = fO;
            const double y0 = osRun(w0, s), y1 = osRun(w1, s), y2 = osRun(w2, s), y3 = osRun(w3, s);
            hb(kCoefB, 4, s + kDnB, y1, y0);
            const double d0 = 0.5 * (fE + fO);
            hb(kCoefB, 4, s + kDnB, y3, y2);
            const double d1 = 0.5 * (fE + fO);
            hb(kCoefA, 8, s + kDnA, d1, d0);
            v = 0.5 * (fE + fO);
        }
        // DC blocker and output
        const double y = p.dcA * (s[kDc + 1] + v - s[kDc]);
        s[kDc] = v; s[kDc + 1] = y;
        return y * p.outG;
    }

    void apply()
    {
        const int os = fOs4 ? 4 : 2;
        if (os != fS.os)
            std::memset(fSt, 0, sizeof(fSt)); // a new oversampling factor starts from clear filters, as the JSFX does
        fS.compute(fModel, fGain, fBass, fMid, fTreble, fPres, fMaster, fBright, fSag, fOs4, fOut, fSr);
    }

    double fSr = 48000.0;
    int fModel = 1;
    double fGain = 5, fBass = 5, fMid = 5, fTreble = 5, fPres = 5, fMaster = 5, fSag = 30, fOut = 0;
    bool fBright = false, fOs4 = true;
    Setup fS;
    double fSt[2][kSize] = {};
    double fE = 0, fO = 0;
    bool fGateOn = false;
    double fGtTh = 0, fGtRel = 0, fGtGa = 0, fGtGr = 0, fGtHold = 0;
    double fGtEnv = 0, fGtG = 1, fGtOpen = 1, fGtCnt = 0;
};

} // namespace nullamp
