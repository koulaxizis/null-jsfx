/*
 * NULL JSFX - Pitch to MIDI, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/pitch_to_midi.jsfx, computed in double in the EEL2 order: the
 * mono sum through a 30 Hz high-pass and a 24 dB/oct low-pass at 0.4 x the reduced rate, every
 * D-th sample kept (D = srate / 11025, rounded), and every 64 kept samples a YIN analysis
 * (threshold 0.15, else the deepest dip under 0.3, parabolic interpolation) and the window's RMS.
 * A note starts after Stability on the same nearest note, holds while the pitch stays within
 * 0.8 semitones, and ends after Stability of silence or no pitch. Per sample, the MIDI that came
 * in at that sample goes out first, then what the analysis sends; send(ofs, a, b, c) gets them in
 * that order. The buffers are members, so nothing is allocated while it plays.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullpitchtomidi {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    double threshold = -40.0, lowNote = 36.0, highNote = 84.0, stability = 40.0;
    int velocityMode = 1;
    double fixedVelocity = 100.0, transpose = 0.0;
    int pitchBend = 0, channel = 0;
};

struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

// pm_bq: kind 0 = high-pass, 1 = low-pass
inline Biquad biquad(int kind, double f, double q, double sr) noexcept
{
    Biquad b;
    const double w0 = 2.0 * kPi * std::fmin(f, 0.45 * sr) / sr;
    const double cw = std::cos(w0);
    const double al = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + al;
    b.b0 = (kind ? (1.0 - cw) / 2.0 : (1.0 + cw) / 2.0) / a0;
    b.b1 = (kind ? 1.0 - cw : -(1.0 + cw)) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0 * cw / a0;
    b.a2 = (1.0 - al) / a0;
    return b;
}

class PitchToMidiProcessor {
public:
    static constexpr int kQueue = 256;
    static constexpr int kBuf = 2048;
    static constexpr int kHop = 64;

    // @init's rates, then the @slider code
    void setSampleRate(double sr) noexcept
    {
        fSr = sr > 0.0 ? sr : 48000.0;
        fDn = std::fmax(1.0, std::floor(fSr / 11025.0 + 0.5));
        fDsr = fSr / fDn;
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
        fQn = fQi = 0;
        fDc = 0.0;
        fBn = 0;
        fHc = 0;
        fBs = 0;
        fH1 = fH2 = fA1 = fA2 = fB1 = fB2 = 0.0;
        fCur = fSent = fCand = -1.0;
        fCandT = fQuietT = 0.0;
        fBend = 8192.0;
        fDetF = 0.0;
        fDetLvl = -120.0;
        fChan = 0;
        std::memset(fB, 0, sizeof(fB));
        std::memset(fDf, 0, sizeof(fDf));
    }

    // @block: the messages that came in (offsets clamped to the block, at most kQueue)
    void begin(uint32_t frames) noexcept
    {
        fQn = fQi = 0;
        fFrames = frames;
        fSi = 0;
    }
    void input(uint32_t ofs, int a, int b, int c) noexcept
    {
        if (fQn >= kQueue)
            return;
        Msg& m = fQ[fQn++];
        m.ofs = fFrames > 0 ? (ofs < fFrames ? ofs : fFrames - 1) : 0;
        m.a = a;
        m.b = b;
        m.c = c;
    }

    double detectedPitch() const noexcept { return fDetF; }
    double detectedLevel() const noexcept { return fDetLvl; }
    int sentNote() const noexcept { return int(fSent); }

    // one sample of @sample (the audio passes through untouched)
    template <class Send>
    void processSample(double l, double r, Send&& send) noexcept
    {
        const uint32_t si = fSi;
        if (fP.channel != fChan)
        {
            noteOff(si, send);
            fChan = fP.channel;
        }
        while (fQi < fQn && fQ[fQi].ofs <= si)
        {
            const Msg& m = fQ[fQi];
            if ((m.a & 0xF0) == 0xB0 && (m.b == 120 || m.b == 123))
            {
                noteOff(si, send);
                fCand = -1.0;
                fCandT = 0.0;
            }
            send(si, m.a, m.b, m.c);
            ++fQi;
        }
        const double x = (l + r) * 0.5;
        double y = fHp.b0 * x + fH1;
        fH1 = fHp.b1 * x - fHp.a1 * y + fH2;
        fH2 = fHp.b2 * x - fHp.a2 * y;
        const double z = fLa.b0 * y + fA1;
        fA1 = fLa.b1 * y - fLa.a1 * z + fA2;
        fA2 = fLa.b2 * y - fLa.a2 * z;
        y = fLb.b0 * z + fB1;
        fB1 = fLb.b1 * z - fLb.a1 * y + fB2;
        fB2 = fLb.b2 * z - fLb.a2 * y;
        if (std::fabs(y) < 0.000000000001)
            y = 0.0;
        if (fDc <= 0.0)
        {
            fDc = fDn;
            const int nw = 2 * fTmax + 32;
            if (fBn >= kBuf)
            {
                std::memmove(fB, fB + fBn - nw, sizeof(double) * size_t(nw));
                fBn = nw;
            }
            fB[fBn] = y;
            ++fBn;
            ++fHc;
            if (fHc >= kHop && fBn >= nw)
            {
                fHc = 0;
                fBs = fBn - nw;
                analyse(si, send);
            }
        }
        fDc -= 1.0;
        ++fSi;
    }

private:
    struct Msg { uint32_t ofs; int a, b, c; };

    // the @slider code
    void coeffs() noexcept
    {
        fLo = std::fmin(fP.lowNote, fP.highNote);
        fHi = std::fmax(fP.lowNote, fP.highNote);
        fTmin = int(std::fmax(2.0, std::floor(fDsr / (440.0 * std::pow(2.0, (fHi + 0.5 - 69.0) / 12.0)))));
        fTmax = int(std::fmin(1000.0, std::ceil(fDsr / (440.0 * std::pow(2.0, (fLo - 0.5 - 69.0) / 12.0)))));
        fTmin = fTmin < fTmax - 1 ? fTmin : fTmax - 1;
        fStab = fP.stability / 1000.0;
        fHopS = kHop / fDsr;
        fHp = biquad(0, 30.0, 0.7071067811865476, fSr);
        fLa = biquad(1, 0.4 * fDsr, 0.5411961001461969, fSr);
        fLb = biquad(1, 0.4 * fDsr, 1.3065629648763766, fSr);
    }

    template <class Send>
    void noteOff(uint32_t ofs, Send& send) noexcept
    {
        if (fSent >= 0.0)
        {
            send(ofs, 0x80 | fChan, int(fSent), 0);
            fSent = -1.0;
            if (fBend != 8192.0)
            {
                send(ofs, 0xE0 | fChan, 0, 64);
                fBend = 8192.0;
            }
        }
        fCur = -1.0;
    }

    template <class Send>
    void noteOn(uint32_t ofs, double n, double lv, Send& send) noexcept
    {
        noteOff(ofs, send);
        const double thr = fP.threshold;
        const double fr = thr < 0.0 ? std::fmin(1.0, std::fmax(0.0, (lv - thr) / -thr)) : 1.0;
        const double v = fP.velocityMode ? std::fmax(1.0, std::fmin(127.0, std::floor(1.0 + 126.0 * fr + 0.5))) : fP.fixedVelocity;
        fCur = n;
        fSent = std::fmax(0.0, std::fmin(127.0, n + fP.transpose));
        send(ofs, 0x90 | fChan, int(fSent), int(v));
    }

    // pm_detect: YIN over the window, the pitch in MIDI notes (fractional) or -1
    double detect() noexcept
    {
        const int w = fTmax + 32;
        double cum = 0.0, best = 1000.0;
        int bt = -1;
        for (int tau = 1; tau <= fTmax; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < w; ++j)
            {
                const double d = fB[fBs + j] - fB[fBs + j + tau];
                s += d * d;
            }
            cum += s;
            fDf[tau] = cum > 0.0 ? s * tau / cum : 1.0;
        }
        int tau = fTmin;
        while (tau <= fTmax && bt < 0)
        {
            if (fDf[tau] < 0.15)
            {
                while (tau < fTmax && fDf[tau + 1] < fDf[tau])
                    ++tau;
                bt = tau;
            }
            ++tau;
        }
        if (bt < 0)
        {
            for (tau = fTmin; tau <= fTmax; ++tau)
                if (fDf[tau] < best)
                {
                    best = fDf[tau];
                    bt = tau;
                }
            if (best >= 0.3)
                bt = -1;
        }
        if (bt > 0)
        {
            const double x0 = fDf[bt - 1 > 1 ? bt - 1 : 1], x1 = fDf[bt], x2 = fDf[bt + 1 < fTmax ? bt + 1 : fTmax];
            const double den = x0 - 2.0 * x1 + x2;
            const double tp = bt + (den > 0.0 ? 0.5 * (x0 - x2) / den : 0.0);
            return 69.0 + 12.0 * std::log(fDsr / tp / 440.0) / std::log(2.0);
        }
        return -1.0;
    }

    // pm_analyse: one analysis, at sample offset ofs of the block
    template <class Send>
    void analyse(uint32_t ofs, Send& send) noexcept
    {
        const int w = fTmax + 32;
        double e = 0.0;
        for (int j = 0; j < w; ++j)
            e += fB[fBs + fTmax + j] * fB[fBs + fTmax + j];
        const double lv = 10.0 * std::log10(std::fmax(e / w, 0.000000000001));
        fDetLvl = lv;
        const double m = lv >= fP.threshold ? detect() : -1.0;
        fDetF = m;
        double n = m >= 0.0 ? std::floor(m + 0.5) : -1.0;
        if (n >= 0.0 && (n < fLo || n > fHi))
            n = -1.0;
        if (fCur >= 0.0 && m >= 0.0 && std::fabs(m - fCur) < 0.8)
            n = fCur;
        if (n >= 0.0)
        {
            fQuietT = 0.0;
            if (n == fCand)
                fCandT += fHopS;
            else
            {
                fCand = n;
                fCandT = fHopS;
            }
            if (fCand != fCur && fCandT >= fStab)
                noteOn(ofs, fCand, lv, send);
        }
        else
        {
            fCand = -1.0;
            fCandT = 0.0;
            if (fCur >= 0.0 && (lv < fP.threshold - 3.0 || m < 0.0))
            {
                fQuietT += fHopS;
                if (fQuietT >= fStab)
                    noteOff(ofs, send);
            }
        }
        if (fP.pitchBend && fSent >= 0.0 && m >= 0.0 && n == fCur)
        {
            const double j = std::floor(8192.0 + std::fmax(-1.0, std::fmin(1.0, (m - fCur) / 2.0)) * 8191.0 + 0.5);
            if (j != fBend)
            {
                send(ofs, 0xE0 | fChan, int(j) & 127, int(std::floor(j / 128.0)));
                fBend = j;
            }
        }
    }

    double fSr = 48000.0, fDn = 4.0, fDsr = 12000.0;
    Params fP;
    Biquad fHp, fLa, fLb;
    double fLo = 36.0, fHi = 84.0, fStab = 0.04, fHopS = 0.005;
    int fTmin = 2, fTmax = 100;
    double fH1 = 0.0, fH2 = 0.0, fA1 = 0.0, fA2 = 0.0, fB1 = 0.0, fB2 = 0.0;
    double fDc = 0.0;
    int fBn = 0, fHc = 0, fBs = 0;
    double fCur = -1.0, fSent = -1.0, fCand = -1.0, fCandT = 0.0, fQuietT = 0.0, fBend = 8192.0;
    double fDetF = 0.0, fDetLvl = -120.0;
    int fChan = 0;
    double fB[kBuf] = {};
    double fDf[1002] = {};
    Msg fQ[kQueue] = {};
    int fQn = 0, fQi = 0;
    uint32_t fSi = 0, fFrames = 0;
};

} // namespace nullpitchtomidi
