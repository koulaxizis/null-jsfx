/*
 * NULL JSFX - Delay, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/delay.jsfx, computed in double like EEL2: a stereo delay line
 * (up to 6.1 s, a whole note down to 40 bpm) with feedback (up to 95%) and a dry/wet mix. With
 * Tempo Sync the time is a note (straight, dotted or triplet) at the host tempo, re-read every
 * block as the JSFX's @block does. Mode Ping-Pong feeds the mono input to the left line and
 * cross-feeds the lines. Inside the feedback loop each repeat passes a one-pole Low Cut and High
 * Cut (off at 20 Hz / 20 kHz) and a soft saturation (Drive, off at 0%). Modulation reads the line
 * at a fractional point moved by two slow sines (tape wow), Diffusion runs the echoes through 4
 * all-passes per channel, Ducking dips the echoes while the dry input is loud, and Freeze makes
 * the loop repeat unchanged (feedback 1, no input, filters and drive bypassed). Each is skipped
 * at its default, so the defaults are the plain delay.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulldelay {

// the delay time in ms for the settings (the JSFX's update_time)
inline double delayMs(double timeMs, bool sync, int subdivision, double tempo) noexcept
{
    if (sync && tempo > 0.0)
        return 60000.0 / tempo * (subdivision == 0 ? 1.0 : subdivision == 1 ? 0.5 : subdivision == 2 ? 0.25
                                  : subdivision == 3 ? 0.125 : subdivision == 4 ? 2.0 : subdivision == 5 ? 4.0
                                  : subdivision == 6 ? 1.5 : subdivision == 7 ? 0.75 : subdivision == 8 ? 0.375
                                  : subdivision == 9 ? 2.0 / 3.0 : subdivision == 10 ? 1.0 / 3.0 : 1.0 / 6.0);
    return timeMs;
}

// soft saturation: a tanh-like curve, slope 1 at 0, flat at |x| = 3 (the JSFX's dl_sat)
inline double sat(double x) noexcept
{
    x = std::max(-3.0, std::min(3.0, x));
    return x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
}

// Schroeder all-pass (z^-len - g) / (1 - g z^-len) on a buffer of len samples (the JSFX's dl_ap)
struct AllPass {
    std::vector<double> buf;
    int len = 1, p = 0;

    void init(int n, int capacity)
    {
        buf.assign(size_t(std::max(1, capacity)), 0.0);
        len = std::max(1, n);
        p = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0); }
    double run(double x, double g)
    {
        const double d = buf[size_t(p)];
        const double y = d - g * x;
        buf[size_t(p)] = x + g * y;
        if (++p >= len)
            p = 0;
        return y;
    }
};

class DelayProcessor {
public:
    // @init: buffers for 6.1 s and the all-passes, cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = int(std::ceil(sr * 6.1));
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fDelay = 1;
        fLcL = fLcR = fHcL = fHcR = 0.0;
        fMdP1 = fMdP2 = 0.0;
        fDkEnv = 0.0;
        fDiffWas = false;
        fRvC = 0;
        fPsP = 0.0;
        static const double kMs[8] = { 0.00113, 0.00187, 0.00307, 0.00441, 0.00129, 0.00203, 0.00323, 0.00463 };
        const int cap = int(std::ceil(sr * 0.005)) + 1;
        for (int k = 0; k < 8; ++k)
            fAp[k].init(int(std::floor(sr * kMs[k])), cap);
        updateTime();
    }

    // @slider
    void setParams(double timeMs, double feedbackPct, double mixPct, bool sync, int subdivision, bool pingPong,
                   double lowCutHz, double highCutHz, double drivePct, double modPct = 0.0, double diffPct = 0.0,
                   double duckPct = 0.0, bool freeze = false, bool reverse = false, double pitchSt = 0.0)
    {
        fRvOn = reverse;
        fPsOn = std::fabs(pitchSt) >= 0.00001; // EEL2's fuzzy !=
        fPsRatio = std::pow(2.0, pitchSt / 12.0);
        fPsW = std::floor(0.05 * fSr);
        fTimeMs = timeMs;
        fSubdivision = subdivision;
        fFeedback = feedbackPct / 100.0 * 0.95;
        fMix = mixPct / 100.0;
        fSync = sync;
        fPing = pingPong;
        fLcOn = lowCutHz > 20.0;
        fLcA = 1.0 - std::exp(-2.0 * 3.141592653589793 * lowCutHz / fSr);
        fHcOn = highCutHz < 20000.0;
        fHcA = 1.0 - std::exp(-2.0 * 3.141592653589793 * highCutHz / fSr);
        fDrvOn = drivePct > 0.0;
        fDrvG = 1.0 + drivePct / 100.0 * 3.0;
        fModOn = modPct > 0.0;
        fMdDep = modPct / 100.0 * 0.002 * fSr;
        fMdI1 = 0.5 / fSr;
        fMdI2 = 1.3 / fSr;
        fDiffOn = diffPct > 0.0;
        fDiffG = diffPct / 100.0 * 0.75;
        if (fDiffOn && !fDiffWas)
            for (AllPass& a : fAp)
                a.clear();
        fDiffWas = fDiffOn;
        fDuckOn = duckPct > 0.0;
        fDuckAmt = duckPct / 100.0;
        fDkAtt = 1.0 - std::exp(-1.0 / (0.01 * fSr));
        fDkRel = 1.0 - std::exp(-1.0 / (0.25 * fSr));
        fFreeze = freeze;
        updateTime();
    }

    double delayMsNow() const noexcept { return fDelayMs; }
    double feedback() const noexcept { return fFeedback; }
    bool frozen() const noexcept { return fFreeze; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo)
    {
        fTempo = tempo;
        if (fSync)
            updateTime(); // @block
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double xl = inL[i], xr = inR[i];
            double el, er;
            if (fRvOn && !fFreeze)
            {
                // reverse: the window just written, read backwards
                if (fRvC >= fRvLen)
                    fRvC = 0;
                int rp = fW - 2 * fRvC - 1;
                if (rp < 0)
                    rp += fSize;
                const double g = std::min(1.0, std::min(double(fRvC + 1) / fRvFade, double(fRvLen - fRvC) / fRvFade));
                el = fL[size_t(rp)] * g;
                er = fR[size_t(rp)] * g;
                ++fRvC;
            }
            else if (fPsOn && !fFreeze)
            {
                // pitch: two taps sweep through a 50 ms window half a window apart, faded in and out
                double base;
                if (fModOn)
                {
                    fMdP1 += fMdI1;
                    if (fMdP1 >= 1.0)
                        fMdP1 -= 1.0;
                    fMdP2 += fMdI2;
                    if (fMdP2 >= 1.0)
                        fMdP2 -= 1.0;
                    base = fDelay + fMdDep * (1.0 + 0.75 * std::sin(kTwoPi * fMdP1) + 0.25 * std::sin(kTwoPi * fMdP2));
                }
                else
                    base = fDelay;
                fPsP += (1.0 - fPsRatio) / fPsW;
                fPsP -= std::floor(fPsP);
                double q = fPsP + 0.5;
                if (q >= 1.0)
                    q -= 1.0;
                const double o1 = std::min(double(fSize - 2), base + fPsP * fPsW);
                const double o2 = std::min(double(fSize - 2), base + q * fPsW);
                const double g1 = 1.0 - std::fabs(2.0 * fPsP - 1.0);
                const double g2 = 1.0 - std::fabs(2.0 * q - 1.0);
                el = g1 * read(fL, o1) + g2 * read(fL, o2);
                er = g1 * read(fR, o1) + g2 * read(fR, o2);
            }
            else if (fModOn && !fFreeze)
            {
                // tape wow: a fractional read point, linearly interpolated
                fMdP1 += fMdI1;
                if (fMdP1 >= 1.0)
                    fMdP1 -= 1.0;
                fMdP2 += fMdI2;
                if (fMdP2 >= 1.0)
                    fMdP2 -= 1.0;
                double off = fDelay + fMdDep * (1.0 + 0.75 * std::sin(kTwoPi * fMdP1) + 0.25 * std::sin(kTwoPi * fMdP2));
                off = std::min(double(fSize - 2), off);
                double rf = fW - off;
                if (rf < 0.0)
                    rf += fSize;
                double ri = std::floor(rf);
                const double fr = rf - ri;
                if (ri >= fSize)
                    ri -= fSize;
                const size_t a = size_t(ri), b = a + 1 >= size_t(fSize) ? 0 : a + 1;
                el = fL[a] + (fL[b] - fL[a]) * fr;
                er = fR[a] + (fR[b] - fR[a]) * fr;
            }
            else
            {
                int r = fW - fDelay;
                if (r < 0)
                    r += fSize;
                el = fL[size_t(r)];
                er = fR[size_t(r)];
            }
            // diffusion: the all-pass chain smears every repeat a little more
            if (fDiffOn && !fFreeze)
            {
                el = fAp[3].run(fAp[2].run(fAp[1].run(fAp[0].run(el, fDiffG), fDiffG), fDiffG), fDiffG);
                er = fAp[7].run(fAp[6].run(fAp[5].run(fAp[4].run(er, fDiffG), fDiffG), fDiffG), fDiffG);
            }
            if (fFreeze)
            {
                // frozen: the loop repeats unchanged, filters and drive bypassed
                fLcL = fLcR = 0.0;
                fHcL = el;
                fHcR = er;
                if (fPing)
                {
                    fL[size_t(fW)] = er;
                    fR[size_t(fW)] = el;
                }
                else
                {
                    fL[size_t(fW)] = el;
                    fR[size_t(fW)] = er;
                }
                if (++fW >= fSize)
                    fW = 0;
                writeOut(xl, xr, el, er, outL[i], outR[i]);
                continue;
            }
            // inside the feedback loop: every repeat passes the filters and the drive again
            if (fLcOn)
            {
                fLcL += fLcA * (el - fLcL);
                el -= fLcL;
                fLcR += fLcA * (er - fLcR);
                er -= fLcR;
            }
            else
                fLcL = fLcR = 0.0;
            if (fHcOn)
            {
                fHcL += fHcA * (el - fHcL);
                el = fHcL;
                fHcR += fHcA * (er - fHcR);
                er = fHcR;
            }
            else
            {
                fHcL = el;
                fHcR = er;
            }
            if (fDrvOn)
            {
                el = sat(el * fDrvG) / fDrvG;
                er = sat(er * fDrvG) / fDrvG;
            }
            if (fPing)
            {
                fL[size_t(fW)] = (xl + xr) * 0.5 + er * fFeedback;
                fR[size_t(fW)] = el * fFeedback;
            }
            else
            {
                fL[size_t(fW)] = xl + el * fFeedback;
                fR[size_t(fW)] = xr + er * fFeedback;
            }
            if (++fW >= fSize)
                fW = 0;
            writeOut(xl, xr, el, er, outL[i], outR[i]);
        }
    }

private:
    // the dry/wet mix, the echoes ducked by the dry input when Ducking is on
    template <typename T>
    void writeOut(double xl, double xr, double el, double er, T& outL, T& outR)
    {
        if (fDuckOn)
        {
            const double x = std::max(std::fabs(xl), std::fabs(xr));
            fDkEnv += (x > fDkEnv ? fDkAtt : fDkRel) * (x - fDkEnv);
            const double g = 1.0 - fDuckAmt * std::min(1.0, fDkEnv * 4.0);
            outL = static_cast<T>(xl * (1.0 - fMix) + el * g * fMix);
            outR = static_cast<T>(xr * (1.0 - fMix) + er * g * fMix);
            return;
        }
        outL = static_cast<T>(xl * (1.0 - fMix) + el * fMix);
        outR = static_cast<T>(xr * (1.0 - fMix) + er * fMix);
    }
    void updateTime()
    {
        fDelayMs = delayMs(fTimeMs, fSync, fSubdivision, fTempo);
        double d = std::floor(fDelayMs * fSr / 1000.0 + 0.5);
        d = std::max(1.0, std::min(double(fSize - 1), d));
        fDelay = int(d);
        fRvLen = std::max(2, std::min(fDelay, fSize / 2 - 2));
        fRvFade = std::max(1.0, std::min(std::floor(fRvLen / 2.0), std::floor(0.01 * fSr)));
    }

    // the line at off samples behind the write point, linearly interpolated (dl_rd)
    double read(const std::vector<double>& b, double off) const noexcept
    {
        double rf = fW - off;
        if (rf < 0.0)
            rf += fSize;
        double ri = std::floor(rf);
        const double fr = rf - ri;
        if (ri >= fSize)
            ri -= fSize;
        const size_t a = size_t(ri), c = a + 1 >= size_t(fSize) ? 0 : a + 1;
        return b[a] + (b[c] - b[a]) * fr;
    }

    bool fRvOn = false, fPsOn = false;
    int fRvC = 0, fRvLen = 2;
    double fRvFade = 1.0, fPsRatio = 1.0, fPsW = 1.0, fPsP = 0.0;

    double fSr = 48000.0, fTempo = 120.0;
    std::vector<double> fL, fR;
    int fSize = 1, fW = 0, fDelay = 1, fSubdivision = 0;
    double fTimeMs = 250.0, fDelayMs = 250.0, fFeedback = 0.0, fMix = 0.3;
    double fLcA = 0.0, fHcA = 0.0, fDrvG = 1.0, fLcL = 0.0, fLcR = 0.0, fHcL = 0.0, fHcR = 0.0;
    bool fSync = false, fPing = false, fLcOn = false, fHcOn = false, fDrvOn = false;
    static constexpr double kTwoPi = 2.0 * 3.141592653589793;
    AllPass fAp[8];
    double fMdDep = 0.0, fMdI1 = 0.0, fMdI2 = 0.0, fMdP1 = 0.0, fMdP2 = 0.0, fDiffG = 0.0;
    double fDuckAmt = 0.0, fDkAtt = 0.0, fDkRel = 0.0, fDkEnv = 0.0;
    bool fModOn = false, fDiffOn = false, fDiffWas = false, fDuckOn = false, fFreeze = false;
};

} // namespace nulldelay
