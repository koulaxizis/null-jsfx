/*
 * NULL JSFX - Maximizer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/maximizer.jsfx, computed in double like EEL2: Input Gain into
 * a look-ahead limiter holding the peaks at Ceiling (one gain for both channels with Stereo
 * Link, else one per channel), then an Output Gain trim (cut only). Last, optional TPDF dither
 * to 16 or 24 bits (exact Park-Miller steps in double, as the JSFX), with first-order
 * error-feedback noise shaping.
 *
 * Appended in 2.4: Character (CLEAN leaves the signal alone, PUNCHY soft-clips the peaks above
 * Ceiling, WARM saturates gently) before the limiter, and Multiband: a Linkwitz-Riley split
 * at Low / High Crossover into three bands, each limited at Ceiling and a quarter linked (in dB)
 * to the band that reduces most, with the look-ahead limiter on their sum. At CLEAN and OFF
 * every operation is the original's. ShortTermLoudness is the display's 3 s K-weighted meter.
 *
 * Appended in 2.7: True Peak (the look-ahead limiter sees the true peak from TruePeak, the
 * BS.1770-4 interpolator, 6 samples late, and the audio waits 6 samples more) and Listen DELTA
 * (the limiter's input minus its output, after the Output Gain, undithered). Off, every
 * operation is the original's.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "NullTruePeak.hpp"

namespace nullmaximizer {

// biquad, transposed direct form II (the JSFX's bq())
struct Biquad {
    double z1 = 0.0, z2 = 0.0;
    double run(double x, double b0, double b1, double b2, double a1, double a2) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// one channel of the 3-band split: low (with the all-pass at the high crossover), mid, high
struct BandSplit {
    Biquad l1a, l1b, h1a, h1b, l2a, l2b, h2a, h2b, ap2;
};

// a Linkwitz-Riley crossover's coefficients (RBJ Butterworth sections, Q = 0.7071)
struct Crossover {
    double lb0 = 0.0, lb1 = 0.0, hb0 = 0.0, hb1 = 0.0, a1 = 0.0, a2 = 0.0;
    void set(double hz, double sr) noexcept
    {
        const double w = 2.0 * 3.141592653589793 * std::min(hz, sr * 0.45) / sr;
        const double cs = std::cos(w);
        const double al = std::sin(w) / (2.0 * std::sqrt(0.5));
        const double a0 = 1.0 + al;
        lb0 = (1.0 - cs) / 2.0 / a0;
        lb1 = (1.0 - cs) / a0;
        hb0 = (1.0 + cs) / 2.0 / a0;
        hb1 = -(1.0 + cs) / a0;
        a1 = -2.0 * cs / a0;
        a2 = (1.0 - al) / a0;
    }
};

// Short-term loudness (EBU R128: 3 s, K-weighted, BS.1770 filters as loudness_meter.jsfx) of a
// stereo signal, updated every 100 ms; for the display only
class ShortTermLoudness {
public:
    void setup(double sr)
    {
        double k = std::tan(3.141592653589793 * 1681.974450955533 / sr);
        const double vh = std::pow(10.0, 3.999843853973347 / 20.0);
        const double vb = std::pow(vh, 0.4996667741545416);
        double q = 0.7071752369554196;
        const double a0 = 1.0 + k / q + k * k;
        fB0 = (vh + vb * k / q + k * k) / a0;
        fB1 = 2.0 * (k * k - vh) / a0;
        fB2 = (vh - vb * k / q + k * k) / a0;
        fA1 = 2.0 * (k * k - 1.0) / a0;
        fA2 = (1.0 - k / q + k * k) / a0;
        k = std::tan(3.141592653589793 * 38.13547087602444 / sr);
        q = 0.5003270373238773;
        const double c0 = 1.0 + k / q + k * k;
        fC1 = 2.0 * (k * k - 1.0) / c0;
        fC2 = (1.0 - k / q + k * k) / c0;
        fN = std::max(1, int(std::floor(sr * 0.1 + 0.5)));
        for (double& z : fZ)
            z = 0.0;
        for (double& b : fBlocks)
            b = 0.0;
        fAcc = 0.0;
        fCnt = fW = 0;
        fLufs = -200.0;
    }

    void process(double l, double r) noexcept
    {
        const double kl = kw(l, fZ[0], fZ[1], fZ[2], fZ[3]);
        const double kr = kw(r, fZ[4], fZ[5], fZ[6], fZ[7]);
        fAcc += kl * kl + kr * kr;
        if (++fCnt >= fN)
        {
            fBlocks[fW] = fAcc;
            fW = (fW + 1) % 30;
            fAcc = 0.0;
            fCnt = 0;
            double st = 0.0;
            for (double b : fBlocks)
                st += b;
            st /= 30.0 * fN;
            fLufs = st > 0.0 ? -0.691 + 10.0 * std::log10(st) : -200.0;
        }
    }

    double lufs() const noexcept { return fLufs; }

private:
    double kw(double x, double& z1, double& z2, double& z3, double& z4) const noexcept
    {
        const double y = fB0 * x + z1;
        z1 = fB1 * x - fA1 * y + z2;
        z2 = fB2 * x - fA2 * y;
        const double k = y + z3;
        z3 = -2.0 * y - fC1 * k + z4;
        z4 = y - fC2 * k;
        return k;
    }

    double fB0 = 1.0, fB1 = 0.0, fB2 = 0.0, fA1 = 0.0, fA2 = 0.0, fC1 = 0.0, fC2 = 0.0;
    double fZ[8] = {}, fBlocks[30] = {}, fAcc = 0.0, fLufs = -200.0;
    int fN = 4800, fCnt = 0, fW = 0;
};

class MaximizerProcessor {
public:
    // @init: the look-ahead buffers (6 ms + 2), cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fMaxLook = int(std::ceil(sr * 0.006)) + 2 + 6;
        fL.assign(size_t(fMaxLook), 0.0);
        fR.assign(size_t(fMaxLook), 0.0);
        fML.assign(size_t(fMaxLook), 0.0);
        fMR.assign(size_t(fMaxLook), 0.0);
        fHL.assign(size_t(fMaxLook), 0.0);
        fHR.assign(size_t(fMaxLook), 0.0);
        for (int b = 0; b < 3; ++b)
        {
            fBHv[b][0] = fBHv[b][1] = 0.0;
            fBHc[b][0] = fBHc[b][1] = 0.0;
            fBG[b][0] = fBG[b][1] = 1.0;
            fBE[b][0] = fBE[b][1] = 1.0;
        }
        fSplitL = BandSplit();
        fSplitR = BandSplit();
        fW = 0;
        fHvL = fHvR = 0.0;
        fHcL = fHcR = 0.0;
        fGL = fGR = 1.0;
        fSeed = 12345.0;
        fEL = fER = 0.0;
        fTp.reset();
    }

    // @slider
    void setParams(double inputDb, double ceilingDb, double releaseMs, double lookaheadMs, double outputDb, bool link, int dither,
                   bool noiseShaping, int character = 0, bool multiband = false, double lowHz = 150.0,
                   double highHz = 4000.0, bool truePeak = false, bool delta = false)
    {
        fTpk = truePeak;
        fDelta = delta;
        fInGain = std::pow(10.0, inputDb / 20.0);
        fCeiling = std::pow(10.0, ceilingDb / 20.0);
        fRel = std::exp(-1.0 / (releaseMs * fSr / 1000.0));
        fLook = int(std::floor(lookaheadMs * fSr / 1000.0 + 0.5));
        fAtt = fLook < 1 ? 1.0 : 1.0 - std::exp(-5.0 / double(fLook));
        fOutGain = std::pow(10.0, std::min(outputDb, 0.0) / 20.0);
        fLink = link;
        fDither = dither;
        fQ = dither == 1 ? 32768.0 : 8388608.0;
        fShape = noiseShaping;
        fCharacter = character;
        fWarmK = 1.0 / (4.0 * fCeiling * fCeiling);
        fMb = multiband;
        fX1.set(lowHz, fSr);
        fX2.set(highHz, fSr);
    }

    int latency() const noexcept { return fLook + (fTpk ? 6 : 0); } // pdc_delay
    // the lowest gain now (Multiband: of the most reduced band, band and sum limiters together)
    double gain() const noexcept
    {
        double g = std::min(fGL, fGR);
        if (fMb)
            g *= std::min(std::min(std::min(fBE[0][0], fBE[1][0]), std::min(fBE[2][0], fBE[0][1])),
                          std::min(fBE[1][1], fBE[2][1]));
        return g;
    }

    // @sample
    void processSample(double& l, double& r) noexcept
    {
        double il = l * fInGain, ir = r * fInGain;
        if (fCharacter != 0)
        {
            il = shape(il);
            ir = shape(ir);
        }
        int rp;
        if (fMb)
        {
            double lo[2], mi[2], hi[2];
            split(fSplitL, il, lo[0], mi[0], hi[0]);
            split(fSplitR, ir, lo[1], mi[1], hi[1]);
            // each band's own limiter at Ceiling, then a quarter linked (in dB) to the lowest band gain
            double* const band[3] = { lo, mi, hi };
            if (fLink)
            {
                for (int b = 0; b < 3; ++b)
                {
                    detect(std::max(std::fabs(band[b][0]), std::fabs(band[b][1])), fBHv[b][0], fBHc[b][0], fBG[b][0]);
                }
                for (int b = 0; b < 3; ++b)
                    fBG[b][1] = fBG[b][0];
            }
            else
            {
                for (int c = 0; c < 2; ++c)
                    for (int b = 0; b < 3; ++b)
                        detect(std::fabs(band[b][c]), fBHv[b][c], fBHc[b][c], fBG[b][c]);
            }
            for (int c = 0; c < 2; ++c)
            {
                const double m = std::pow(std::min(fBG[0][c], std::min(fBG[1][c], fBG[2][c])), 0.25);
                for (int b = 0; b < 3; ++b)
                    fBE[b][c] = std::pow(fBG[b][c], 0.75) * m;
            }
            // the look-ahead limiter on the sum the bands will make
            const double sl = lo[0] * fBE[0][0] + mi[0] * fBE[1][0] + hi[0] * fBE[2][0];
            const double sr = lo[1] * fBE[0][1] + mi[1] * fBE[1][1] + hi[1] * fBE[2][1];
            if (fTpk)
                tpDetect(sl, sr);
            else if (fLink)
            {
                detect(std::max(std::fabs(sl), std::fabs(sr)), fHvL, fHcL, fGL);
                fGR = fGL;
            }
            else
            {
                detect(std::fabs(sl), fHvL, fHcL, fGL);
                detect(std::fabs(sr), fHvR, fHcR, fGR);
            }
            const size_t w = size_t(fW);
            fL[w] = lo[0];
            fR[w] = lo[1];
            fML[w] = mi[0];
            fMR[w] = mi[1];
            fHL[w] = hi[0];
            fHR[w] = hi[1];
        }
        else
        {
            if (fTpk)
                tpDetect(il, ir);
            else if (fLink)
            {
                detect(std::max(std::fabs(il), std::fabs(ir)), fHvL, fHcL, fGL);
                fGR = fGL;
            }
            else
            {
                detect(std::fabs(il), fHvL, fHcL, fGL);
                detect(std::fabs(ir), fHvR, fHcR, fGR);
            }
            fL[size_t(fW)] = il;
            fR[size_t(fW)] = ir;
        }
        rp = fW - fLook - (fTpk ? 6 : 0);
        if (rp < 0)
            rp += fMaxLook;
        if (++fW >= fMaxLook)
            fW = 0;
        const size_t k = size_t(rp);
        if (fMb)
        {
            l = std::max(-fCeiling, std::min(fCeiling, (fL[k] * fBE[0][0] + fML[k] * fBE[1][0] + fHL[k] * fBE[2][0]) * fGL)) * fOutGain;
            r = std::max(-fCeiling, std::min(fCeiling, (fR[k] * fBE[0][1] + fMR[k] * fBE[1][1] + fHR[k] * fBE[2][1]) * fGR)) * fOutGain;
            if (fDelta)
            {
                l = (fL[k] + fML[k] + fHL[k]) * fOutGain - l;
                r = (fR[k] + fMR[k] + fHR[k]) * fOutGain - r;
            }
        }
        else
        {
            l = std::max(-fCeiling, std::min(fCeiling, fL[k] * fGL)) * fOutGain;
            r = std::max(-fCeiling, std::min(fCeiling, fR[k] * fGR)) * fOutGain;
            if (fDelta)
            {
                l = fL[k] * fOutGain - l;
                r = fR[k] * fOutGain - r;
            }
        }
        if (fDither != 0 && !fDelta)
        {
            l = quantise(l, fEL);
            r = quantise(r, fER);
        }
    }

private:
    // Character: PUNCHY (1) soft-clips above Ceiling (up to 6 dB more), WARM (2) saturates gently
    double shape(double x) const noexcept
    {
        if (fCharacter == 1)
        {
            const double a = std::fabs(x);
            if (a > fCeiling)
            {
                const double u = (a - fCeiling) / fCeiling;
                x = (x > 0.0 ? 1.0 : -1.0) * (fCeiling + fCeiling * u / (1.0 + u));
            }
            return x;
        }
        return x / std::sqrt(1.0 + x * x * fWarmK);
    }

    void split(BandSplit& s, double x, double& lo, double& mi, double& hi) noexcept
    {
        const Crossover& a = fX1;
        const Crossover& b = fX2;
        lo = s.ap2.run(s.l1b.run(s.l1a.run(x, a.lb0, a.lb1, a.lb0, a.a1, a.a2), a.lb0, a.lb1, a.lb0, a.a1, a.a2),
                       b.a2, b.a1, 1.0, b.a1, b.a2);
        const double t = s.h1b.run(s.h1a.run(x, a.hb0, a.hb1, a.hb0, a.a1, a.a2), a.hb0, a.hb1, a.hb0, a.a1, a.a2);
        mi = s.l2b.run(s.l2a.run(t, b.lb0, b.lb1, b.lb0, b.a1, b.a2), b.lb0, b.lb1, b.lb0, b.a1, b.a2);
        hi = s.h2b.run(s.h2a.run(t, b.hb0, b.hb1, b.hb0, b.a1, b.a2), b.hb0, b.hb1, b.hb0, b.a1, b.a2);
    }

    // one exact Park-Miller step, a uniform value in -1..1 (as riser.jsfx)
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 1073741824.0 - 1.0;
    }

    // TPDF dither (half the sum of two uniform values = +-1 LSB triangular), rounding to the
    // bit depth; with Noise Shaping the last error is subtracted first (1 - z^-1)
    double quantise(double x, double& err) noexcept
    {
        const double n1 = rnd();
        const double n2 = rnd();
        const double v = fShape ? x - err : x;
        const double q = std::floor(v * fQ + (n1 + n2) * 0.5 + 0.5) / fQ;
        err = fShape ? q - v : 0.0;
        return q;
    }

    // True Peak: the look-ahead limiter on the true peak (6 samples late), as the JSFX
    void tpDetect(double l, double r) noexcept
    {
        detect(fTp.peak(l, r, fLink), fHvL, fHcL, fGL);
        if (fLink)
            fGR = fGL;
        else
            detect(fTp.right(), fHvR, fHcR, fGR);
    }

    void detect(double peak, double& hv, double& hc, double& g) const noexcept
    {
        if (peak >= hv)
        {
            hv = peak;
            hc = fLook;
        }
        else if (hc > 0.0)
            hc -= 1.0;
        else
            hv = std::max(peak, hv * fRel);
        const double target = hv > fCeiling ? fCeiling / hv : 1.0;
        g = target < g ? g + (target - g) * fAtt : target + (g - target) * fRel;
    }

    double fSr = 48000.0;
    std::vector<double> fL, fR, fML, fMR, fHL, fHR;
    BandSplit fSplitL, fSplitR;
    Crossover fX1, fX2;
    double fBHv[3][2] = {}, fBHc[3][2] = {}, fBG[3][2] = {}, fBE[3][2] = {};
    double fWarmK = 0.25;
    int fCharacter = 0;
    bool fMb = false;
    int fMaxLook = 1, fW = 0, fLook = 0;
    double fHvL = 0.0, fHvR = 0.0, fHcL = 0.0, fHcR = 0.0, fGL = 1.0, fGR = 1.0;
    double fInGain = 1.0, fCeiling = 1.0, fRel = 0.0, fAtt = 1.0, fOutGain = 1.0;
    double fSeed = 12345.0, fQ = 32768.0, fEL = 0.0, fER = 0.0;
    int fDither = 0;
    bool fLink = true, fShape = false;
    nulljsfx::TruePeak fTp;
    bool fTpk = false, fDelta = false;
};

} // namespace nullmaximizer
