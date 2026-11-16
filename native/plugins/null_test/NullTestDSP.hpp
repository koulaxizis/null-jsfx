/*
 * NULL JSFX - Null Test, framework-independent core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/null_test.jsfx, computed in double in the EEL2 order (EEL2's
 * FFT through EelFft.hpp). The difference is main - gain * sidechain, both lined up in time.
 * Align Auto: every 4096 samples the last 16384 of both (mono), zero padded, go through an FFT;
 * the cross spectrum with each bin scaled to 1 is averaged and transformed back, and its highest
 * peak (either polarity) within Range is the delay, kept while the peak is under a fifth of its
 * maximum; the gain is the least-squares one from sums that forget over half a second (a new
 * delay starts them and the null depth again). Align Manual uses Offset, Trim and Polarity.
 * The side that leads is delayed; Listen plays the lined-up main, sidechain or difference with
 * a 10 ms crossfade. The buffers are members, so nothing is allocated while it plays.
 */

#pragma once

#include "EelFft.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullnulltest {

struct Params {
    int listen = 2, align = 1;
    double range = 20.0, offset = 0.0, trim = 0.0;
    int polarity = 0;
};

class NullTest {
public:
    static constexpr int kW = 16384, kH = 4096, kBands = 10;

    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        reset();
        setParams(fP);
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        fTm = p.listen == 0 ? 1.0 : 0.0;
        fTs = p.listen == 1 ? 1.0 : 0.0;
        fTd = p.listen == 2 ? 1.0 : 0.0;
        fGm = (p.polarity ? -1.0 : 1.0) * std::pow(10.0, p.trim / 20.0);
    }

    void reset() noexcept
    {
        std::memset(fCs, 0, sizeof(fCs));
        std::memset(fMl, 0, sizeof(fMl));
        std::memset(fMr, 0, sizeof(fMr));
        std::memset(fSl, 0, sizeof(fSl));
        std::memset(fSrr, 0, sizeof(fSrr));
        std::memset(fDl, 0, sizeof(fDl));
        std::memset(fDr, 0, sizeof(fDr));
        std::memset(fBm, 0, sizeof(fBm));
        std::memset(fBd, 0, sizeof(fBd));
        fWp = fCnt = 0;
        fLag = fCand = fCn = 0;
        fConf = fSms = fSss = fGa = fG = fEd = fEm = 0.0;
        fZc = fHeard = 0.0;
        fXm = 0.0;
        fXs = 0.0;
        fXd = 1.0;
        fXr = 1.0 / (0.01 * fSr);
        fLa = std::exp(-1.0 / (0.5 * fSr));
        fDa = std::exp(-1.0 / (0.3 * fSr));
        fTh = std::pow(10.0, -70.0 / 20.0);
        fE20 = std::pow(10.0, -20.0);
        fE30 = std::pow(10.0, -30.0);
        fE300 = std::pow(10.0, -300.0);
        for (int i = 0; i < kBands; ++i)
            fBr[i] = -150.0;
    }

    void process(const float* inL, const float* inR, const float* scL, const float* scR, float* outL,
                 float* outR, uint32_t frames) noexcept
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            fMl[fWp] = inL[n];
            fMr[fWp] = inR[n];
            fSl[fWp] = scL[n];
            fSrr[fWp] = scR[n];
            const double d = std::fmax(-16000.0, std::fmin(16000.0, fP.align ? double(fLag) : fP.offset));
            const int pm = (fWp - int(std::fmax(0.0, -d)) + kW) % kW;
            const int ps = (fWp - int(std::fmax(0.0, d)) + kW) % kW;
            const double ml = fMl[pm], mr = fMr[pm], sl = fSl[ps], sr = fSrr[ps];
            if (fP.align)
            {
                fSms = fSms * fLa + (ml * sl + mr * sr);
                fSss = fSss * fLa + (sl * sl + sr * sr);
                if (fSss > fE20)
                    fGa = fSms / fSss;
                fG = fGa;
            }
            else
                fG = fGm;
            const double dl = ml - fG * sl, dr = mr - fG * sr;
            fDl[fWp] = dl;
            fDr[fWp] = dr;
            fEd = fEd * fDa + (dl * dl + dr * dr);
            fEm = fEm * fDa + (ml * ml + mr * mr);
            if (dl == 0.0 && dr == 0.0)
                fZc += 1.0;
            else
                fZc = 0.0;
            if (ml != 0.0 || mr != 0.0)
                fHeard = 0.0;
            else
                fHeard += 1.0;
            if (++fWp >= kW)
                fWp = 0;
            if (++fCnt >= kH)
            {
                fCnt = 0;
                frame();
            }
            fXm = fXm < fTm ? std::fmin(fTm, fXm + fXr) : std::fmax(fTm, fXm - fXr);
            fXs = fXs < fTs ? std::fmin(fTs, fXs + fXr) : std::fmax(fTs, fXs - fXr);
            fXd = fXd < fTd ? std::fmin(fTd, fXd + fXr) : std::fmax(fTd, fXd - fXr);
            outL[n] = float(fXm * ml + fXs * fG * sl + fXd * dl);
            outR[n] = float(fXm * mr + fXs * fG * sr + fXd * dr);
        }
    }

    // for the display
    // the null depth in dB: -300 the difference is zero, -400 the main is silent (as the GFX)
    double depth() const noexcept
    {
        return fEm <= 0.0 || fHeard > fSr ? -400.0 : fEd > 0.0 ? 10.0 * std::log10(fEd / fEm) : -300.0;
    }
    int delay() const noexcept { return int(std::fmax(-16000.0, std::fmin(16000.0, fP.align ? double(fLag) : fP.offset))); }
    double gain() const noexcept { return fG; }
    double confidence() const noexcept { return fConf; }
    bool identical() const noexcept { return fZc >= fSr && fHeard < fZc; }
    double band(int b) const noexcept { return fBr[b]; }

private:
    void frame() noexcept
    {
        double em = 0.0, es = 0.0;
        for (int i = 0; i < kW; ++i)
        {
            const int p = (fWp + i) % kW;
            fX[2 * i] = (fMl[p] + fMr[p]) * 0.5;
            fX[2 * i + 1] = 0.0;
            fY[2 * i] = (fSl[p] + fSrr[p]) * 0.5;
            fY[2 * i + 1] = 0.0;
            em += fX[2 * i] * fX[2 * i];
            es += fY[2 * i] * fY[2 * i];
        }
        std::memset(fX + 2 * kW, 0, sizeof(double) * 2 * kW);
        std::memset(fY + 2 * kW, 0, sizeof(double) * 2 * kW);
        if (!(std::sqrt(em / kW) > fTh))
            return;
        eelfft::fft(fX, 2 * kW);
        eelfft::fft(fY, 2 * kW);
        if (std::sqrt(es / kW) > fTh)
        {
            // the cross spectrum, each bin divided by its magnitude to the power 0.75, averaged
            for (int i = 0; i < 2 * kW; ++i)
            {
                const double xr = fX[2 * i], xi = fX[2 * i + 1], yr = fY[2 * i], yi = fY[2 * i + 1];
                const double cr = xr * yr + xi * yi;
                const double ci = xi * yr - xr * yi;
                const double mg = std::pow(cr * cr + ci * ci, 0.375) + fE30;
                fCs[2 * i] = fCs[2 * i] * 0.6 + cr / mg;
                fCs[2 * i + 1] = fCs[2 * i + 1] * 0.6 + ci / mg;
            }
            std::memcpy(fY, fCs, sizeof(double) * 4 * kW);
            eelfft::ifft(fY, 2 * kW);
            const int rng = int(std::fmin(std::floor(fP.range * fSr / 1000.0), 12000.0));
            double best = std::fabs(fY[0]);
            int bi = 0;
            for (int j = 1; j <= rng; ++j)
            {
                if (std::fabs(fY[2 * j]) > best)
                {
                    best = std::fabs(fY[2 * j]);
                    bi = j;
                }
                if (std::fabs(fY[2 * (2 * kW - j)]) > best)
                {
                    best = std::fabs(fY[2 * (2 * kW - j)]);
                    bi = -j;
                }
            }
            // the match: the correlation of the two windows at that delay
            double sxy = 0.0, sxx = 0.0, syy = 0.0;
            for (int i = bi > 0 ? bi : 0, c = 0; c < kW - (bi < 0 ? -bi : bi); ++i, ++c)
            {
                const int p = (fWp + i) % kW, q = (fWp + i - bi) % kW;
                const double xr = (fMl[p] + fMr[p]) * 0.5, yr = (fSl[q] + fSrr[q]) * 0.5;
                sxy += xr * yr;
                sxx += xr * xr;
                syy += yr * yr;
            }
            fConf = sxx > 0.0 && syy > 0.0 ? std::fabs(sxy) / std::sqrt(sxx * syy) : 0.0;
            // a new delay must win two frames in a row; it starts the gain's sums and the null depth again
            if (bi == fLag)
                fCn = 0;
            else
            {
                if (bi == fCand)
                    ++fCn;
                else
                {
                    fCand = bi;
                    fCn = 1;
                }
                if (fCn >= 2 && fConf >= 0.2)
                {
                    fLag = bi;
                    fCn = 0;
                    fSms = fSss = fEd = fEm = 0.0;
                }
            }
        }
        // the energy per octave band of the main and of the difference, both channels
        bands(fMl, fMr, fBm);
        bands(fDl, fDr, fBd);
        for (int b = 0; b < kBands; ++b)
        {
            const double lo = 31.25 * std::pow(2.0, b - 0.5);
            fBr[b] = lo < fSr / 2.0 && fBm[b] > 0.0 ? std::fmax(-150.0, 10.0 * std::log10(std::fmax(fBd[b], fE300) / fBm[b])) : -150.0;
        }
    }

    // the energy per octave band of the rings l and r (the last kW samples) into e: left as the
    // real part and right as the imaginary part of one FFT, so bins k and 2W - k hold both
    void bands(const double* l, const double* r, double* e) noexcept
    {
        for (int i = 0; i < kW; ++i)
        {
            const int k = (fWp + i) % kW;
            fX[2 * i] = l[k];
            fX[2 * i + 1] = r[k];
        }
        std::memset(fX + 2 * kW, 0, sizeof(double) * 2 * kW);
        eelfft::fft(fX, 2 * kW);
        eelfft::fftPermute(fX, 2 * kW);
        std::memset(e, 0, sizeof(double) * kBands);
        for (int i = 1; i < kW; ++i)
        {
            const double f = i * fSr / (2 * kW);
            const double b = std::floor(std::log(f / 31.25) / std::log(2.0) + 0.5);
            const int k = 2 * kW - i;
            if (b >= 0.0 && b < kBands)
                e[int(b)] += fX[2 * i] * fX[2 * i] + fX[2 * i + 1] * fX[2 * i + 1] + fX[2 * k] * fX[2 * k] + fX[2 * k + 1] * fX[2 * k + 1];
        }
    }

    double fSr = 48000.0;
    Params fP;
    double fTm = 0.0, fTs = 0.0, fTd = 1.0, fGm = 1.0;
    double fX[4 * kW] = {}, fY[4 * kW] = {}, fCs[4 * kW] = {};
    double fMl[kW] = {}, fMr[kW] = {}, fSl[kW] = {}, fSrr[kW] = {}, fDl[kW] = {}, fDr[kW] = {};
    double fBm[kBands] = {}, fBd[kBands] = {}, fBr[kBands] = {};
    int fWp = 0, fCnt = 0, fLag = 0, fCand = 0, fCn = 0;
    double fConf = 0.0, fSms = 0.0, fSss = 0.0, fGa = 0.0, fG = 0.0, fEd = 0.0, fEm = 0.0;
    double fZc = 0.0, fHeard = 0.0, fXm = 0.0, fXs = 0.0, fXd = 1.0, fXr = 0.0, fLa = 0.0, fDa = 0.0, fTh = 0.0;
    double fE20 = 0.0, fE30 = 0.0, fE300 = 0.0;
};

} // namespace nullnulltest
