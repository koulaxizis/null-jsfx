/*
 * NULL JSFX - Bitcrusher, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/bitcrusher.jsfx, computed in double in the EEL2 order: Drive,
 * an optional 12 dB/oct low-pass at 0.45 x Sample Rate (Anti Alias), a sample-and-hold whose
 * phase runs at Sample Rate / srate, rounding to 2^(Bits - 1) steps on each side of zero, an
 * optional 12 dB/oct low-pass (Smooth), then Mix and Output.
 */

#pragma once

#include <cmath>

namespace nullbitcrusher {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double bits = 8.0, rate = 8000.0;
    int antiAlias = 0;
    double smooth = 0.0, drive = 0.0, mix = 100.0, output = 0.0;
};

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// bc_lp: RBJ low-pass, Q = 1/sqrt(2)
inline Biquad lowpass(double f, double sr) noexcept
{
    Biquad b;
    const double w0 = 2.0 * kPi * std::fmin(f, 0.45 * sr) / sr;
    const double cw = std::cos(w0);
    const double al = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double a0 = 1.0 + al;
    b.b0 = (1.0 - cw) / 2.0 / a0;
    b.b1 = (1.0 - cw) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - al) / a0;
    return b;
}

class BitcrusherProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        coeffs();
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        coeffs();
    }

    // @init
    void reset() noexcept
    {
        fPh = 1.0;
        fHl = fHr = 0.0;
        fA1l = fA2l = fA1r = fA2r = 0.0;
        fS1l = fS2l = fS1r = fS2r = 0.0;
    }

    // the levels the display shows (2^(Bits - 1)) and the Drive gain
    double steps() const noexcept { return fQ; }
    double driveGain() const noexcept { return fDg; }

    void processSample(double& l, double& r) noexcept
    {
        double xl = l * fDg;
        double xr = r * fDg;
        if (fP.antiAlias)
        {
            double y = fAa.b0 * xl + fA1l;
            fA1l = fAa.b1 * xl - fAa.a1 * y + fA2l;
            fA2l = fAa.b2 * xl - fAa.a2 * y;
            xl = y;
            y = fAa.b0 * xr + fA1r;
            fA1r = fAa.b1 * xr - fAa.a1 * y + fA2r;
            fA2r = fAa.b2 * xr - fAa.a2 * y;
            xr = y;
        }
        fPh += fInc;
        if (fPh >= 1.0)
        {
            fPh -= std::floor(fPh);
            fHl = std::floor(xl * fQ + 0.5) / fQ;
            fHr = std::floor(xr * fQ + 0.5) / fQ;
        }
        double wl = fHl;
        double wr = fHr;
        if (fSmOn)
        {
            double y = fSm.b0 * wl + fS1l;
            fS1l = fSm.b1 * wl - fSm.a1 * y + fS2l;
            fS2l = fSm.b2 * wl - fSm.a2 * y;
            wl = y;
            y = fSm.b0 * wr + fS1r;
            fS1r = fSm.b1 * wr - fSm.a1 * y + fS2r;
            fS2r = fSm.b2 * wr - fSm.a2 * y;
            wr = y;
        }
        l = (l + (wl - l) * fMix) * fOg;
        r = (r + (wr - r) * fMix) * fOg;
    }

private:
    // the @slider code
    void coeffs() noexcept
    {
        fQ = std::pow(2.0, fP.bits - 1.0);
        fInc = fP.rate / fSr;
        fAa = lowpass(0.45 * fP.rate, fSr);
        fSmOn = fP.smooth > 0.0;
        fSm = lowpass(20000.0 * std::pow(std::fmin(0.45 * fP.rate, 20000.0) / 20000.0, fP.smooth / 100.0), fSr);
        fDg = std::pow(10.0, fP.drive / 20.0);
        fMix = fP.mix / 100.0;
        fOg = std::pow(10.0, fP.output / 20.0);
    }

    double fSr = 48000.0;
    Params fP;
    Biquad fAa, fSm;
    bool fSmOn = false;
    double fQ = 128.0, fInc = 1.0 / 6.0, fDg = 1.0, fMix = 1.0, fOg = 1.0;
    double fPh = 1.0, fHl = 0.0, fHr = 0.0;
    double fA1l = 0.0, fA2l = 0.0, fA1r = 0.0, fA2r = 0.0;
    double fS1l = 0.0, fS2l = 0.0, fS1r = 0.0, fS2r = 0.0;
};

} // namespace nullbitcrusher
