/*
 * NULL JSFX - Half Speed, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/half_speed.jsfx, computed in double like EEL2. A cycle lasts
 * Length / Speed beats: from its start the input is recorded into the buffer and played back
 * from the cycle's start at Speed (linear interpolation), or with Pitch Keep by two Hann grains
 * (~40 ms, half a grain apart) that start where the slowed read point is. While the transport
 * plays the cycle is locked to the song position (re-read every block, as the JSFX's @block
 * does; small drift corrected, a jump starts a new cycle); stopped, it free-runs at the tempo.
 * A new cycle crossfades from the old playback over 5 ms. Tone: a one-pole high cut on the
 * slowed signal. Tempo clamped to 40..300 bpm.
 */

#pragma once

#include "NullLazyClear.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace nullhalfspeed {

struct Params {
    int length = 2;          // 0..3: 1/4, 1/2, 1 Bar, 2 Bars
    int speed = 0;           // 0..2: 1/2, 1/4, 2/3
    bool keep = false;       // Pitch Keep
    double toneHz = 20000.0; // 1000..20000, off at 20000
    double mixPct = 100.0;
    double offset = 0.0;     // Sync Offset in beats
};

inline double lengthBeats(int l) { return l == 0 ? 1.0 : l == 1 ? 2.0 : l == 2 ? 4.0 : 8.0; }
inline double speedRatio(int s) { return s == 0 ? 0.5 : s == 1 ? 0.25 : 2.0 / 3.0; }

class HalfSpeedProcessor {
public:
    static constexpr int kBins = 128; // the display's waveform: peaks of 1/128 of the Length

    // @init: the buffer is allocated here (cleared), never on the audio thread
    void setSampleRate(double sr)
    {
        fSr = sr;
        fXf = std::fmax(1.0, std::floor(0.005 * sr));
        fGl = 2.0 * std::fmax(8.0, std::floor(0.02 * sr));
        fMargin = std::ceil(0.1 * sr);
        fBsize = std::ceil(sr * 12.0) + fMargin;
        fL.allocate(size_t(fBsize));
        fR.allocate(size_t(fBsize));
        reset();
    }

    // @init again (REAPER re-runs it when the transport starts). The buffer reads as cleared at
    // once but is cleared chunk by chunk as it is written (NullLazyClear.hpp), so a transport
    // start costs no multi-millisecond clear on the audio thread.
    void reset()
    {
        fL.clearAll();
        fR.clearAll();
        fN = 0.0;
        fOldRd = 0.0;
        fXc = fXf;
        fG1 = 0.0;
        fG2 = fGl / 2.0;
        fS1 = fS2 = 0.0;
        fLpL = fLpR = 0.0;
        for (double& v : fOv)
            v = 0.0;
        fLb = -1.0;
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    // for the display
    double pos() const noexcept { return fN; }
    double capture() const noexcept { return fLs; }
    double cycle() const noexcept { return fClen; }
    double speed() const noexcept { return fSpeed; }
    double peak(int b) const noexcept { return fOv[b]; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo, bool playing,
                 double beatPos)
    {
        // @block
        double bpm = tempo > 0.0 ? tempo : 120.0;
        bpm = std::fmin(300.0, std::fmax(40.0, bpm));
        const double spb = fSr * 60.0 / bpm;
        fLs = fLenBeats * spb;
        fClen = fCycBeats * spb;
        const double wlim = std::fmin(fBsize - 1.0, std::floor(fLs) + fMargin);
        if (playing)
        {
            const double pos = beatPos - fOffset;
            const double within = pos - fCycBeats * std::floor(pos / fCycBeats);
            const double nt = std::floor(within * spb + 0.5);
            double d = nt - fN;
            if (d > fClen / 2.0)
                d -= fClen;
            else if (d < -fClen / 2.0)
                d += fClen;
            if (std::fabs(d) > 2.0)
                seam(nt);
            else if (std::fabs(nt - fN) <= 2.0)
                fN = nt;
        }

        for (uint32_t s = 0; s < frames; ++s)
        {
            const double x0 = inL[s], x1 = inR[s];
            if (fN >= fClen)
                seam(0.0);
            if (fN < wlim)
            {
                fL.set(size_t(fN), x0);
                fR.set(size_t(fN), x1);
            }
            double wl, wr;
            if (fKeep)
            {
                if (fG1 == 0.0)
                    fS1 = fSpeed * fN;
                if (fG2 == 0.0)
                    fS2 = fSpeed * fN;
                const double w1 = 0.5 - 0.5 * std::cos(2.0 * kPi * fG1 / fGl);
                const double w2 = 0.5 - 0.5 * std::cos(2.0 * kPi * fG2 / fGl);
                wl = rd(fL, fS1 + fG1) * w1 + rd(fL, fS2 + fG2) * w2;
                wr = rd(fR, fS1 + fG1) * w1 + rd(fR, fS2 + fG2) * w2;
                fG1 += 1.0;
                if (fG1 >= fGl)
                    fG1 = 0.0;
                fG2 += 1.0;
                if (fG2 >= fGl)
                    fG2 = 0.0;
            }
            else
            {
                const double rp = fSpeed * fN;
                wl = rd(fL, rp);
                wr = rd(fR, rp);
            }
            if (fXc < fXf)
            {
                if (fKeep)
                    fXc = fXf;
                else
                {
                    const double xk = (fXc + 1.0) / (fXf + 1.0);
                    wl = wl * xk + rd(fL, fOldRd) * (1.0 - xk);
                    wr = wr * xk + rd(fR, fOldRd) * (1.0 - xk);
                    fOldRd += fSpeed;
                    fXc += 1.0;
                }
            }
            if (fHcOn)
            {
                fLpL += fHcA * (wl - fLpL);
                fLpR += fHcA * (wr - fLpR);
                wl = fLpL;
                wr = fLpR;
            }
            outL[s] = static_cast<T>(x0 * (1.0 - fMix) + wl * fMix);
            outR[s] = static_cast<T>(x1 * (1.0 - fMix) + wr * fMix);
            fN += 1.0;

            // display only (as half_speed_gfx.jsfx): the peak of each 1/128 of the Length
            const double i = fN - 1.0;
            if (i < fLs)
            {
                const int b = int(std::floor(i * kBins / fLs));
                if (b != int(fLb))
                {
                    fOv[b] = 0.0;
                    fLb = b;
                }
                const double v = std::fmax(std::fabs(fL.get(size_t(i))), std::fabs(fR.get(size_t(i))));
                if (v > fOv[b])
                    fOv[b] = v;
            }
        }
    }

private:
    static constexpr double kPi = 3.141592653589793;

    double rd(const nulljsfx::LazyClearBuffer& b, double p) const
    {
        const double i = std::fmin(fBsize - 1.0, std::floor(p));
        const double f = p - i;
        const double i2 = std::fmin(fBsize - 1.0, i + 1.0);
        const double v = b.get(size_t(i));
        return v + (b.get(size_t(i2)) - v) * f;
    }

    void seam(double m)
    {
        fOldRd = fSpeed * fN;
        fXc = 0.0;
        fN = m;
    }

    void updateParams()
    {
        fLenBeats = lengthBeats(fP.length);
        fSpeed = speedRatio(fP.speed);
        fCycBeats = fLenBeats / fSpeed;
        fKeep = fP.keep;
        fHcOn = fP.toneHz < 20000.0;
        fHcA = 1.0 - std::exp(-2.0 * kPi * fP.toneHz / fSr);
        fMix = fP.mixPct / 100.0;
        fOffset = fP.offset;
    }

    Params fP;
    double fSr = 48000.0;
    double fXf = 1.0, fGl = 16.0, fMargin = 0.0, fBsize = 1.0;
    nulljsfx::LazyClearBuffer fL, fR;
    double fN = 0.0, fOldRd = 0.0, fXc = 1.0, fG1 = 0.0, fG2 = 0.0, fS1 = 0.0, fS2 = 0.0;
    double fLpL = 0.0, fLpR = 0.0;
    double fLenBeats = 4.0, fSpeed = 0.5, fCycBeats = 8.0, fHcA = 0.0, fMix = 1.0, fOffset = 0.0;
    double fLs = 1.0, fClen = 1.0;
    bool fKeep = false, fHcOn = false;
    double fOv[kBins] = {};
    double fLb = -1.0;
};

} // namespace nullhalfspeed
