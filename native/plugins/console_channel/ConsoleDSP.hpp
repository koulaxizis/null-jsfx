/*
 * NULL JSFX - Console Channel and Console Buss, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/console_channel.jsfx and console_buss.jsfx, computed in double
 * in the EEL2 order: per side a first-order pre-emphasis shelf, a soft saturator (Pade tanh,
 * flat beyond +-3) with headroom, the exact inverse of the shelf and a 5 Hz DC blocker; the
 * Buss also leaks each side, high-passed at 1 kHz, into the other before the DC blocker.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullconsole {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr double kTiny = 0.000000000000001;

enum Character { kClean = 0, kWarm, kPunchy, kBright };

struct Coeffs {
    bool emph = true;
    double b0 = 1.0, b1 = 0.0, sc = 0.0, asym = 0.0, k = 0.5, ik = 2.0, go = 1.0, dc = 1.0;
    double ct = 0.0, hc = 0.0; // Buss only
    double driveDb = 0.0;      // for the display
};

// the @slider code of console_channel.jsfx (buss = false) or console_buss.jsfx (buss = true)
inline Coeffs coeffsFor(bool buss, int ch, double drive, double output, double sr) noexcept
{
    Coeffs c;
    c.emph = ch > 0;
    const double sgDb = buss ? (ch == 2 ? 3.0 : 4.0) : (ch == 2 ? 4.0 : 6.0);
    const double sg = std::pow(10.0, sgDb / 20.0);
    const double f = buss ? (ch == 1 ? 150.0 : ch == 2 ? 60.0 : 5000.0) : (ch == 1 ? 200.0 : ch == 2 ? 80.0 : 3000.0);
    c.sc = std::exp(-2.0 * kPi * std::fmin(f, 0.45 * sr) / sr);
    if (ch == 3)
    {
        c.b0 = 1.0 + c.sc * (sg - 1.0);
        c.b1 = sg * c.sc;
    }
    else
    {
        c.b0 = 1.0 + (sg - 1.0) * (1.0 - c.sc);
        c.b1 = c.sc;
    }
    if (buss)
    {
        c.asym = ch == 1 ? 0.1 : ch == 2 ? 0.03 : 0.0;
        const double rng = (ch == 1 || ch == 2) ? 12.0 : 9.0;
        c.driveDb = drive / 100.0 * rng;
        c.k = std::pow(10.0, c.driveDb / 20.0) / 2.5;
        c.ct = drive / 100.0 * 0.0316;
        c.hc = std::exp(-2.0 * kPi * std::fmin(1000.0, 0.45 * sr) / sr);
    }
    else
    {
        c.asym = ch == 1 ? 0.15 : ch == 2 ? 0.05 : 0.0;
        const double rng = ch == 1 ? 15.0 : ch == 2 ? 18.0 : 12.0;
        c.driveDb = drive / 100.0 * rng;
        c.k = std::pow(10.0, c.driveDb / 20.0) / 2.0;
    }
    c.ik = 1.0 / c.k;
    c.go = std::pow(10.0, output / 20.0);
    c.dc = 1.0 - 2.0 * kPi * 5.0 / sr;
    return c;
}

// the saturator with its even-harmonic term, scaled back (no shelves, no output gain)
inline double saturate(const Coeffs& c, double e) noexcept
{
    double u = e * c.k;
    u = u > 3.0 ? 3.0 : u < -3.0 ? -3.0 : u;
    const double v = u * (27.0 + u * u) / (27.0 + 9.0 * u * u);
    return (v + c.asym * v * v) * c.ik;
}

// the static curve of the display: output for a steady input amplitude x
inline double curve(const Coeffs& c, double x) noexcept { return saturate(c, x) * c.go; }

// one side: emphasis, saturation, de-emphasis (cc_ch / cb_ch), plus the Buss's leak high-pass
// and the DC blocker
struct Side {
    double xp = 0.0, ep = 0.0, sp = 0.0, yp = 0.0, lp = 0.0, dx = 0.0, dy = 0.0;

    double stage(const Coeffs& c, double x) noexcept
    {
        double e;
        if (c.emph)
        {
            e = c.b0 * x - c.b1 * xp + c.sc * ep;
            xp = x;
            ep = std::fabs(e) < kTiny ? 0.0 : e;
        }
        else
            e = x;
        const double s = saturate(c, e);
        double y;
        if (c.emph)
        {
            y = (s - c.sc * sp + c.b1 * yp) / c.b0;
            sp = s;
            yp = std::fabs(y) < kTiny ? 0.0 : y;
        }
        else
            y = s;
        return y;
    }

    double highPass(const Coeffs& c, double x) noexcept
    {
        lp = (1.0 - c.hc) * x + c.hc * lp;
        lp = std::fabs(lp) < kTiny ? 0.0 : lp;
        return x - lp;
    }

    double dcBlock(const Coeffs& c, double x) noexcept
    {
        const double v = x - dx + c.dc * dy;
        dx = x;
        dy = std::fabs(v) < kTiny ? 0.0 : v;
        return v;
    }
};

class ConsoleProcessor {
public:
    explicit ConsoleProcessor(bool buss) noexcept : fBuss(buss) { update(); }

    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        update();
    }

    void setParams(int ch, double drive, double output) noexcept
    {
        fCh = ch;
        fDrive = drive;
        fOut = output;
        update();
    }

    void reset() noexcept { fL = Side(); fR = Side(); }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames) noexcept
    {
        const Coeffs& c = fC;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double x0 = inL[i], x1 = inR[i];
            if (fBuss)
            {
                const double yl = fL.stage(c, x0);
                const double yr = fR.stage(c, x1);
                const double hl = fL.highPass(c, yl);
                const double hr = fR.highPass(c, yr);
                outL[i] = float(fL.dcBlock(c, yl + c.ct * hr) * c.go);
                outR[i] = float(fR.dcBlock(c, yr + c.ct * hl) * c.go);
            }
            else
            {
                outL[i] = float(fL.dcBlock(c, fL.stage(c, x0)) * c.go);
                outR[i] = float(fR.dcBlock(c, fR.stage(c, x1)) * c.go);
            }
        }
    }

private:
    void update() noexcept { fC = coeffsFor(fBuss, fCh, fDrive, fOut, fSr); }

    bool fBuss;
    double fSr = 48000.0, fDrive = 30.0, fOut = 0.0;
    int fCh = 1;
    Coeffs fC;
    Side fL, fR;
};

} // namespace nullconsole
