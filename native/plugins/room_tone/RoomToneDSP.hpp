/*
 * NULL JSFX - Room Tone, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/room_tone.jsfx, computed in double like EEL2 (FFTs with
 * EelFft.hpp, EEL2's fft/ifft/fft_permute semantics). With Learn On, every 512 samples a
 * 2048-point Hann frame of both channels adds its power per bin and its cross spectrum to the
 * profile. Otherwise, with a profile, every 512 samples a frame of fill is written: random phases
 * from the JSFX's Park-Miller sequence (exact in double), the learnt magnitudes, the right channel
 * as coherent with the left as in the recording; inverse FFT, Hann window, overlap-add. Fill gaps
 * tops the input up to the room tone after Hold below Threshold, Constant bed adds the fill all
 * the time, both through a one-pole fade. The profile survives activate() and is only cleared by
 * a new sample rate, like the JSFX's @init. No latency.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullrt {

static constexpr int kN = 2048, kH = kN / 4, kNB = kN / 2 + 1;

// what the interface draws: the learnt profile per bin (dB, 0 = a full-scale sine) and the fill
struct View {
    float profile[kNB] = {};
    std::atomic<float> sampleRate { 48000.0f };
    std::atomic<float> learnt { 0.0f }; // seconds of learnt profile
    std::atomic<float> fill { 0.0f };   // the fill's gain, 0..1
    std::atomic<int> learning { 0 };
};

class RoomTone {
public:
    RoomTone()
        : fFftL(2 * kN), fFftR(2 * kN), fWin(kN), fInL(kN), fInR(kN), fOutL(kN), fOutR(kN), fPpl(kNB), fPpr(kNB),
          fPcr(kNB), fPci(kNB)
    {
    }

    void setView(View* v) noexcept { fView = v; }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (std::vector<double>* v : { &fWin, &fInL, &fInR, &fOutL, &fOutR })
            std::fill(v->begin(), v->end(), 0.0);
        // the profile survives @init (playback start); only a new sample rate clears it
        if (!(fProfSr == sr))
        {
            for (std::vector<double>* v : { &fPpl, &fPpr, &fPcr, &fPci })
                std::fill(v->begin(), v->end(), 0.0);
            fLcount = 0.0;
            fLearning = false;
            fProfSr = sr;
        }
        for (int i = 0; i < kN; ++i)
            fWin[size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
        fOscale = 4.0 / (3.0 * kN);
        fDi = std::exp(-1.0 / (0.01 * sr));
        fDf = std::exp(-1.0 / (0.05 * sr));
        fSeed = 12345.0;
        fIpos = fOpos = fCnt = fFilled = 0;
        fEin = fEfl = fHc = fG = 0.0;
        if (fView)
        {
            fView->sampleRate.store(float(sr), std::memory_order_relaxed);
            publish();
        }
    }

    // @slider (Fill Level as the JSFX slider holds it, a multiple of 0.5)
    void setParams(bool learn, double thresholdDb, double fillDb, double fadeMs, double holdMs, bool bed,
                   bool listenFill)
    {
        fLearn = learn;
        fThp = std::pow(10.0, thresholdDb / 10.0);
        fLvl = std::pow(10.0, fillDb / 20.0);
        fFc = std::exp(-1.0 / (fadeMs / 1000.0 * fSr));
        fHold = std::floor(holdMs / 1000.0 * fSr);
        fBed = bed;
        fListen = listenFill;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            const double l = inL[n], r = inR[n];
            fInL[size_t(fIpos)] = l;
            fInR[size_t(fIpos)] = r;
            if (++fIpos >= kN)
                fIpos = 0;
            if (fFilled < kN)
                ++fFilled;

            if (++fCnt >= kH)
            {
                fCnt = 0;
                frame();
            }

            const double fl = fOutL[size_t(fOpos)] * fLvl, fr = fOutR[size_t(fOpos)] * fLvl;
            fOutL[size_t(fOpos)] = 0.0;
            fOutR[size_t(fOpos)] = 0.0;
            if (++fOpos >= kN)
                fOpos = 0;

            // the input's power (10 ms) and the fill's (50 ms); a gap starts after Hold below Threshold
            fEin = fEin * fDi + (l * l + r * r) * 0.5 * (1.0 - fDi);
            fEfl = fEfl * fDf + (fl * fl + fr * fr) * 0.5 * (1.0 - fDf);
            if (fEin < fThp)
            {
                if (fHc < fHold)
                    fHc += 1.0;
            }
            else
                fHc = 0.0;
            // the share of the fill that brings the level up to the room tone: sqrt(1 - input / fill)
            double target;
            if (fLearning || fLcount == 0.0)
                target = 0.0;
            else if (fBed)
                target = 1.0;
            else if (fEin < fThp && fHc >= fHold && fEfl > 0.0)
                target = std::sqrt(std::max(0.0, 1.0 - fEin / fEfl));
            else
                target = 0.0;
            fG = fG * fFc + target * (1.0 - fFc);

            if (fListen)
            {
                outL[n] = static_cast<T>(fl * fG);
                outR[n] = static_cast<T>(fr * fG);
            }
            else
            {
                outL[n] = static_cast<T>(l + fl * fG);
                outR[n] = static_cast<T>(r + fr * fG);
            }
        }
        if (fView)
            fView->fill.store(float(fG), std::memory_order_relaxed);
    }

private:
    double rnd()
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    void frame()
    {
        double* bl = fFftL.data();
        double* br = fFftR.data();
        // Learn switched on: start a new profile
        if (fLearn && !fLearning)
        {
            for (std::vector<double>* v : { &fPpl, &fPpr, &fPcr, &fPci })
                std::fill(v->begin(), v->end(), 0.0);
            fLcount = 0.0;
        }
        fLearning = fLearn;
        if (fLearning)
        {
            if (fFilled >= kN)
            {
                for (int i = 0; i < kN; ++i)
                {
                    const int p = (fIpos + i) % kN;
                    const double w = fWin[size_t(i)];
                    bl[2 * i] = fInL[size_t(p)] * w;
                    bl[2 * i + 1] = 0.0;
                    br[2 * i] = fInR[size_t(p)] * w;
                    br[2 * i + 1] = 0.0;
                }
                eelfft::fft(bl, kN);
                eelfft::fftPermute(bl, kN);
                eelfft::fft(br, kN);
                eelfft::fftPermute(br, kN);
                for (int k = 0; k < kNB; ++k)
                {
                    const size_t K = size_t(k);
                    const double rl = bl[2 * k], il = bl[2 * k + 1], rr = br[2 * k], ir = br[2 * k + 1];
                    fPpl[K] += rl * rl + il * il;
                    fPpr[K] += rr * rr + ir * ir;
                    fPcr[K] += rl * rr + il * ir;
                    fPci[K] += il * rr - rl * ir;
                }
                fLcount += 1.0;
                if (fView)
                    publish();
            }
        }
        else if (fLcount > 0.0)
        {
            std::fill(fFftL.begin(), fFftL.end(), 0.0);
            std::fill(fFftR.begin(), fFftR.end(), 0.0);
            for (int k = 1; k < kN / 2; ++k)
            {
                const size_t K = size_t(k);
                const double pl = fPpl[K] / fLcount, pr = fPpr[K] / fLcount;
                const double d = std::sqrt(pl * pr);
                const double gr = d > 0.0 ? fPcr[K] / fLcount / d : 0.0;
                const double gi = d > 0.0 ? fPci[K] / fLcount / d : 0.0;
                const double s = std::sqrt(std::max(0.0, 1.0 - gr * gr - gi * gi));
                const double a = 2.0 * M_PI * rnd();
                const double b = 2.0 * M_PI * rnd();
                const double ar = std::cos(a), ai = std::sin(a), cr = std::cos(b), ci = std::sin(b);
                const double ml = std::sqrt(pl), mr = std::sqrt(pr);
                // left: the learnt magnitude, a random phase; right: conj(coherence) x the left's
                // phase plus the rest of its power on a second random phase
                bl[2 * k] = ml * ar;
                bl[2 * k + 1] = ml * ai;
                br[2 * k] = mr * (gr * ar + gi * ai + s * cr);
                br[2 * k + 1] = mr * (gr * ai - gi * ar + s * ci);
                bl[2 * (kN - k)] = bl[2 * k];
                bl[2 * (kN - k) + 1] = -bl[2 * k + 1];
                br[2 * (kN - k)] = br[2 * k];
                br[2 * (kN - k) + 1] = -br[2 * k + 1];
            }
            eelfft::fftIpermute(bl, kN);
            eelfft::ifft(bl, kN);
            eelfft::fftIpermute(br, kN);
            eelfft::ifft(br, kN);
            for (int i = 0; i < kN; ++i)
            {
                const size_t p = size_t((fOpos + i) % kN);
                const double w = fWin[size_t(i)] * fOscale;
                fOutL[p] += bl[2 * i] * w;
                fOutR[p] += br[2 * i] * w;
            }
        }
        if (fView)
        {
            fView->learnt.store(float(fLcount * kH / fSr), std::memory_order_relaxed);
            fView->learning.store(fLearning ? 1 : 0, std::memory_order_relaxed);
        }
    }

    // the profile for the display: mean power of both channels per bin, in dB of a full-scale sine
    void publish()
    {
        const double ref = 20.0 * std::log10(kN / 4.0);
        for (int k = 0; k < kNB; ++k)
        {
            const double m = fLcount > 0.0 ? (fPpl[size_t(k)] + fPpr[size_t(k)]) * 0.5 / fLcount : 0.0;
            fView->profile[k] = float(10.0 * std::log10(std::max(m, 0.000000000001)) - ref);
        }
        fView->learnt.store(float(fLcount * kH / fSr), std::memory_order_relaxed);
    }

    View* fView = nullptr;
    double fSr = 48000.0, fProfSr = 0.0;
    std::vector<double> fFftL, fFftR, fWin, fInL, fInR, fOutL, fOutR, fPpl, fPpr, fPcr, fPci;
    int fIpos = 0, fOpos = 0, fCnt = 0, fFilled = 0;
    double fLcount = 0.0, fOscale = 0.0, fDi = 0.0, fDf = 0.0, fSeed = 12345.0;
    double fEin = 0.0, fEfl = 0.0, fHc = 0.0, fG = 0.0;
    bool fLearning = false, fLearn = false, fBed = false, fListen = false;
    double fThp = 0.0, fLvl = 1.0, fFc = 0.0, fHold = 0.0;
};

} // namespace nullrt
