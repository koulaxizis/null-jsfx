/*
 * NULL JSFX - Harmonizer, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/harmonizer.jsfx, computed in double like EEL2: the mono sum
 * feeds a 0.2 s line read by two crossfading two-tap pitch shifters (voice 1 and 2, detuned
 * and delayed), panned apart by Spread and blended with an equal-power dry/wet Mix.
 * Mode Scale / MIDI and Formant Preserve track the input pitch as pitch_correction does (YIN on
 * a band-limited copy decimated to about 11 kHz): Scale turns the Interval sliders into degrees
 * of Key + Scale from the sung note, MIDI sings the latest two held notes (midi(), called before
 * the sample at the event's frame), and Formant Preserve shifts with TD-PSOLA (pitch-synchronous
 * two-period grains) instead of the two-tap shifters. Mode Semitones with Formant Preserve Off
 * runs the original code only. Voices 3 or 4 adds voice 3 and 4 (Detune +-half, Delay x1.71 and
 * x2.09, panned halfway; in Mode MIDI the third and fourth latest held notes); at Voices 2 they
 * are skipped.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullharmonizer {

class HarmonizerProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fWin = std::floor(sr * 0.05);
        fSize = int(std::ceil(sr * 0.2) + fWin);
        fBuf.assign(size_t(fSize + 2), 0.0); // + the never-written memory after the JSFX's line
        fW = 0;
        fPh1 = fPh2 = fPh3 = fPh4 = 0.0;

        // pitch tracking and PSOLA, as pitch_correction
        fDec = std::max(1.0, std::floor(sr / 11025.0 + 0.5));
        fFa = sr / fDec;
        fTmax = std::ceil(fFa / 70.0);
        fTmin = std::max(2.0, std::floor(fFa / 1000.0));
        fHopS = kHop * fDec / sr;
        fHm = std::ceil(sr / 70.0);
        fMlag = fTmax * fDec + std::floor(kHop * fDec / 2.0);
        fLat = fMlag + std::ceil(1.5 * fHm) + 4.0;
        fBs = int(fLat + std::ceil(sr * 0.1672) + 2.0 * fHm + 20.0);
        fInM.assign(size_t(fBs), 0.0);
        fAcc1.assign(size_t(fBs), 0.0);
        fAcc2.assign(size_t(fBs), 0.0);
        fAw1.assign(size_t(fBs), 0.0);
        fAw2.assign(size_t(fBs), 0.0);
        fAcc3.assign(size_t(fBs), 0.0);
        fAcc4.assign(size_t(fBs), 0.0);
        fAw3.assign(size_t(fBs), 0.0);
        fAw4.assign(size_t(fBs), 0.0);
        fLpb.assign(size_t(fBs), 0.0);
        std::fill(std::begin(fDbuf), std::end(fDbuf), 0.0);
        std::fill(std::begin(fFrm), std::end(fFrm), 0.0);
        std::fill(std::begin(fCmn), std::end(fCmn), 0.0);
        std::fill(std::begin(fAmk), std::end(fAmk), 0.0);
        std::fill(std::begin(fHeld), std::end(fHeld), 0.0);

        fHoldN = std::floor(0.06 / fHopS) + 1.0;
        fTuv = std::floor(sr / 200.0);
        fLn2 = std::log(2.0);
        fGd = std::floor(sr * 0.00035 + 0.5);
        fHpC = std::exp(-2.0 * kPi * 50.0 / sr);
        const double lw = 2.0 * kPi * 1200.0 / sr;
        const double lc = std::cos(lw);
        double la = std::sin(lw) / (2.0 * 0.5411961);
        fB0a = (1.0 - lc) / 2.0 / (1.0 + la); fB1a = (1.0 - lc) / (1.0 + la);
        fA1a = -2.0 * lc / (1.0 + la); fA2a = (1.0 - la) / (1.0 + la);
        la = std::sin(lw) / (2.0 * 1.3065630);
        fB0b = (1.0 - lc) / 2.0 / (1.0 + la); fB1b = (1.0 - lc) / (1.0 + la);
        fA1b = -2.0 * lc / (1.0 + la); fA2b = (1.0 - la) / (1.0 + la);
        fHpX = fHpY = 0.0;
        fXa1 = fXa2 = fYa1 = fYa2 = 0.0;
        fXb1 = fXb2 = fYb1 = fYb2 = 0.0;
        fWp = 0; fN = 0.0;
        fDcnt = 0; fDw = 0; fHcnt = 0;
        fPer = fPerA = fPerI = fTuv; fAmLast = 0.0; fMk = 0; fSn1 = fSn2 = 0.0;
        fVoiced = false; fWasV = false; fUvc = fHoldN + 1.0;
        fTf = fTmax; fDet = 60.0; fCur = -1.0;
        fHt1 = fHt2 = fHs1 = fHs2 = 0.0; fHgt1 = fHgt2 = fHg1 = fHg2 = 0.0; fHsNew = true;
        fHt3 = fHt4 = fHs3 = fHs4 = 0.0; fHgt3 = fHgt4 = fHg3 = fHg4 = 0.0; fSn3 = fSn4 = 0.0;
        fHk = 1.0 - std::exp(-1.0 / (sr * 0.01));
        fStamp = 0.0; fMn1 = fMn2 = fMn3 = fMn4 = -1;
    }

    // @slider; s[0..16] = slider1..slider17
    void setParams(const double* s)
    {
        fR1 = std::pow(2.0, (s[0] + s[4] / 100.0) / 12.0);
        fR2 = std::pow(2.0, (s[1] - s[4] / 100.0) / 12.0);
        fG1 = std::pow(10.0, s[2] / 20.0);
        fG2 = std::pow(10.0, s[3] / 20.0);
        fNv = int(std::lround(s[12])) + 2;
        fR3 = std::pow(2.0, (s[13] + s[4] / 200.0) / 12.0);
        fR4 = std::pow(2.0, (s[14] - s[4] / 200.0) / 12.0);
        fG3 = std::pow(10.0, s[15] / 20.0);
        fG4 = std::pow(10.0, s[16] / 20.0);
        fBase3 = s[5] * 1.71 * fSr / 1000.0;
        fBase4 = s[5] * 2.09 * fSr / 1000.0;
        fFb3 = int(std::floor(fBase3 + 0.5));
        fFb4 = int(std::floor(fBase4 + 0.5));
        fS14 = s[13]; fS15 = s[14];
        fBase1 = s[5] * fSr / 1000.0;
        fBase2 = s[5] * 1.37 * fSr / 1000.0;
        fSp = s[6] / 100.0;
        fDry = std::cos(s[7] / 100.0 * M_PI / 2.0);
        fWet = std::sin(s[7] / 100.0 * M_PI / 2.0);

        fS1 = s[0]; fS2 = s[1]; fS5 = s[4];
        fMode = int(std::lround(s[8]));
        fKey = s[9];
        static constexpr int kMasks[] = { 2741, 1453, 1709, 1717, 2477, 661, 1193 };
        fMask = kMasks[std::max(0, std::min(6, int(std::lround(s[10]))))];
        fFmt = s[11] >= 0.5;
        fAdv = fMode > 0 || fFmt;
        fFb1 = int(std::floor(fBase1 + 0.5));
        fFb2 = int(std::floor(fBase2 + 0.5));
        if (fMode == 1)
        {
            if (fCur < 0.0)
                fCur = 60.0 + fKey;
            setIv();
            fHgt1 = fHgt2 = fHgt3 = fHgt4 = 1.0;
        }
        else if (fMode == 2)
            midiIv();
        else
        {
            fHt1 = s[0] + s[4] / 100.0;
            fHt2 = s[1] - s[4] / 100.0;
            fHt3 = s[13] + s[4] / 200.0;
            fHt4 = s[14] - s[4] / 200.0;
            fHgt1 = fHgt2 = fHgt3 = fHgt4 = 1.0;
        }
    }

    // a MIDI message due now (the JSFX's @sample queue): notes held / released, all notes off
    void midi(uint8_t status, uint8_t d1, uint8_t d2) noexcept
    {
        const int st = status & 0xF0, n = d1 & 0x7F;
        if (st == 0x90 && d2 > 0)
        {
            fStamp += 1.0;
            fHeld[n] = fStamp;
        }
        else if (st == 0x80 || st == 0x90)
            fHeld[n] = 0.0;
        else if (st == 0xB0 && (n == 120 || n == 123))
            std::fill(std::begin(fHeld), std::end(fHeld), 0.0);
        if (fMode == 2)
            midiIv();
    }

    // for the display: the voices' intervals now (semitones, Detune included) and levels
    // (0..1, Mode MIDI fades), whether the pitch-tracking path runs, the note the scale is on
    // (-1 = none yet), whether the input is pitched, the held notes voice 1 to 4 sing, the voice count
    bool advanced() const noexcept { return fAdv; }
    double interval1() const noexcept { return fHs1; }
    double interval2() const noexcept { return fHs2; }
    double gate1() const noexcept { return fHg1; }
    double gate2() const noexcept { return fHg2; }
    double scaleNote() const noexcept { return fCur; }
    bool pitched() const noexcept { return fWasV; }
    int note1() const noexcept { return fMn1; }
    int note2() const noexcept { return fMn2; }
    double interval3() const noexcept { return fHs3; }
    double interval4() const noexcept { return fHs4; }
    double gate3() const noexcept { return fHg3; }
    double gate4() const noexcept { return fHg4; }
    int note3() const noexcept { return fMn3; }
    int note4() const noexcept { return fMn4; }
    int voices() const noexcept { return fNv; }

    // @sample (the MIDI messages due at this sample already given to midi())
    void processSample(double& l, double& r)
    {
        const double xl = l, xr = r;
        fBuf[size_t(fW)] = (xl + xr) * 0.5;
        double v1, v2, v3 = 0.0, v4 = 0.0;
        if (fAdv)
        {
            // pitch tracking on a band-limited mono copy, decimated (as pitch_correction)
            const double ax = (xl + xr) * 0.5;
            fHpY = fHpC * (fHpY + ax - fHpX);
            fHpX = ax;
            if (std::fabs(fHpY) < kTiny)
                fHpY = 0.0;
            double ya = fB0a * fHpY + fB1a * fXa1 + fB0a * fXa2 - fA1a * fYa1 - fA2a * fYa2;
            if (std::fabs(ya) < kTiny)
                ya = 0.0;
            fXa2 = fXa1; fXa1 = fHpY; fYa2 = fYa1; fYa1 = ya;
            double yb = fB0b * ya + fB1b * fXb1 + fB0b * fXb2 - fA1b * fYb1 - fA2b * fYb2;
            if (std::fabs(yb) < kTiny)
                yb = 0.0;
            fXb2 = fXb1; fXb1 = ya; fYb2 = fYb1; fYb1 = yb;
            if (++fDcnt >= int(fDec))
            {
                fDcnt = 0;
                fDbuf[fDw] = yb;
                if (++fDw >= kDbs)
                    fDw = 0;
                if (++fHcnt >= kHop)
                {
                    fHcnt = 0;
                    analyze();
                    control();
                }
            }
            const double hph = (double(fHcnt) * fDec + double(fDcnt)) / (kHop * fDec);
            fPerI = fPerA + (fPer - fPerA) * hph;
            // the voices glide to their intervals (and, in Mode MIDI, fade in and out)
            if (fHsNew)
            {
                fHs1 = fHt1;
                fHs2 = fHt2;
                fHs3 = fHt3;
                fHs4 = fHt4;
                fHsNew = false;
            }
            fHs1 += (fHt1 - fHs1) * fHk;
            fHs2 += (fHt2 - fHs2) * fHk;
            fHg1 += (fHgt1 - fHg1) * fHk;
            fHg2 += (fHgt2 - fHg2) * fHk;
            if (fNv > 2)
            {
                fHs3 += (fHt3 - fHs3) * fHk;
                fHs4 += (fHt4 - fHs4) * fHk;
                fHg3 += (fHgt3 - fHg3) * fHk;
                fHg4 += (fHgt4 - fHg4) * fHk;
            }
            else
            {
                fHs3 = fHt3; fHs4 = fHt4; fHg3 = fHgt3; fHg4 = fHgt4;
            }
            // PSOLA: analysis marks one input period apart, synthesis marks one output period apart
            fInM[size_t(fWp)] = ax;
            fLpb[size_t(fWp)] = yb;
            while (fAmLast <= fN - fMlag)
            {
                fAmLast += fPer;
                if (fWasV)
                    fAmLast = snap(fAmLast);
                if (++fMk >= 64)
                    fMk = 0;
                fAmk[fMk] = fAmLast;
            }
            while (fSn1 - fN <= fHm + 1.0 - fLat)
                fSn1 += place(fSn1, fHs1, fAcc1, fAw1);
            while (fSn2 - fN <= fHm + 1.0 - fLat)
                fSn2 += place(fSn2, fHs2, fAcc2, fAw2);
            // a voice that is off places no grains and starts at voice 1's next mark
            if (fFmt && fNv > 2)
            {
                while (fSn3 - fN <= fHm + 1.0 - fLat)
                    fSn3 += place(fSn3, fHs3, fAcc3, fAw3);
            }
            else
                fSn3 = fSn1;
            if (fFmt && fNv > 3)
            {
                while (fSn4 - fN <= fHm + 1.0 - fLat)
                    fSn4 += place(fSn4, fHs4, fAcc4, fAw4);
            }
            else
                fSn4 = fSn1;
            int ro = fWp - int(fLat) - fFb1;
            if (ro < 0)
                ro += fBs;
            const double f1 = fAcc1[size_t(ro)] / std::max(fAw1[size_t(ro)], 0.5);
            fAcc1[size_t(ro)] = 0.0; fAw1[size_t(ro)] = 0.0;
            ro = fWp - int(fLat) - fFb2;
            if (ro < 0)
                ro += fBs;
            const double f2 = fAcc2[size_t(ro)] / std::max(fAw2[size_t(ro)], 0.5);
            fAcc2[size_t(ro)] = 0.0; fAw2[size_t(ro)] = 0.0;
            double f3 = 0.0, f4 = 0.0;
            if (fNv > 2)
            {
                ro = fWp - int(fLat) - fFb3;
                if (ro < 0)
                    ro += fBs;
                f3 = fAcc3[size_t(ro)] / std::max(fAw3[size_t(ro)], 0.5);
                fAcc3[size_t(ro)] = 0.0; fAw3[size_t(ro)] = 0.0;
                ro = fWp - int(fLat) - fFb4;
                if (ro < 0)
                    ro += fBs;
                f4 = fAcc4[size_t(ro)] / std::max(fAw4[size_t(ro)], 0.5);
                fAcc4[size_t(ro)] = 0.0; fAw4[size_t(ro)] = 0.0;
            }
            if (++fWp >= fBs)
                fWp = 0;
            fN += 1.0;
            if (fFmt)
            {
                v1 = f1 * (fG1 * fHg1);
                v2 = f2 * (fG2 * fHg2);
                if (fNv > 2)
                    v3 = f3 * (fG3 * fHg3);
                if (fNv > 3)
                    v4 = f4 * (fG4 * fHg4);
            }
            else
            {
                v1 = voice(fPh1, std::pow(2.0, fHs1 / 12.0), fBase1) * (fG1 * fHg1);
                v2 = voice(fPh2, std::pow(2.0, fHs2 / 12.0), fBase2) * (fG2 * fHg2);
                if (fNv > 2)
                    v3 = voice(fPh3, std::pow(2.0, fHs3 / 12.0), fBase3) * (fG3 * fHg3);
                if (fNv > 3)
                    v4 = voice(fPh4, std::pow(2.0, fHs4 / 12.0), fBase4) * (fG4 * fHg4);
            }
        }
        else
        {
            v1 = voice(fPh1, fR1, fBase1) * fG1;
            v2 = voice(fPh2, fR2, fBase2) * fG2;
            if (fNv > 2)
                v3 = voice(fPh3, fR3, fBase3) * fG3;
            if (fNv > 3)
                v4 = voice(fPh4, fR4, fBase4) * fG4;
        }
        if (++fW >= fSize)
            fW = 0;
        const double p1 = M_PI / 4.0 * (1.0 - fSp);
        const double p2 = M_PI / 2.0 - p1;
        double wl = v1 * std::cos(p1) + v2 * std::cos(p2);
        double wr = v1 * std::sin(p1) + v2 * std::sin(p2);
        if (fNv > 2)
        {
            // voice 3 halfway left, voice 4 halfway right
            const double p3 = M_PI / 4.0 * (1.0 - fSp / 2.0);
            wl += v3 * std::cos(p3);
            wr += v3 * std::sin(p3);
            if (fNv > 3)
            {
                const double p4 = M_PI / 2.0 - p3;
                wl += v4 * std::cos(p4);
                wr += v4 * std::sin(p4);
            }
        }
        l = xl * fDry + wl * fWet;
        r = xr * fDry + wr * fWet;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            double l = inL[n], r = inR[n];
            processSample(l, r);
            outL[n] = static_cast<T>(l);
            outR[n] = static_cast<T>(r);
        }
    }

private:
    static constexpr double kPi = 3.141592653589793;
    static constexpr double kTiny = 0.000000000000000000000000000001;
    static constexpr int kHop = 64;
    static constexpr int kDbs = 1024;

    double tap(double d) const noexcept
    {
        double rp = double(fW) - d;
        if (rp < 0.0)
            rp += double(fSize);
        const double i = std::floor(rp), f = rp - i;
        double i2 = i + 1.0;
        if (i2 >= double(fSize))
            i2 = 0.0;
        return fBuf[size_t(i)] + (fBuf[size_t(i2)] - fBuf[size_t(i)]) * f;
    }

    // two-tap crossfading pitch shifter (the JSFX's voice())
    double voice(double& ph, double ratio, double base) const noexcept
    {
        ph += (1.0 - ratio) / fWin;
        if (ph >= 1.0)
            ph -= 1.0;
        if (ph < 0.0)
            ph += 1.0;
        double p2 = ph + 0.5;
        if (p2 >= 1.0)
            p2 -= 1.0;
        const double g = 0.5 - 0.5 * std::cos(2.0 * M_PI * ph);
        return tap(base + 1.0 + ph * fWin) * g + tap(base + 1.0 + p2 * fWin) * (1.0 - g);
    }

    // is note k in Key + Scale?
    bool inSc(double k) const noexcept
    {
        const double kk = k - fKey;
        return ((fMask >> int(kk - 12.0 * std::floor(kk / 12.0))) & 1) != 0;
    }

    // the note n scale degrees from note k
    double deg(double k, double n) const noexcept
    {
        double c = std::fabs(n);
        const double d = n < 0.0 ? -1.0 : 1.0;
        while (c > 0.0)
        {
            k += d;
            if (inSc(k))
                c -= 1.0;
        }
        return k;
    }

    // Mode Scale: the voices' intervals (semitones, Detune included) from the current note
    void setIv() noexcept
    {
        fHt1 = (deg(fCur, fS1) - fCur) + fS5 / 100.0;
        fHt2 = (deg(fCur, fS2) - fCur) - fS5 / 100.0;
        fHt3 = (deg(fCur, fS14) - fCur) + fS5 / 200.0;
        fHt4 = (deg(fCur, fS15) - fCur) - fS5 / 200.0;
    }

    // Mode MIDI: voice 1 sings the latest held note, voice 2 the one held before it, voices 3
    // and 4 the two before that
    void midiIv() noexcept
    {
        fMn1 = fMn2 = fMn3 = fMn4 = -1;
        double b1 = 0.0, b2 = 0.0, b3 = 0.0, b4 = 0.0;
        for (int k = 0; k < 128; ++k)
        {
            if (fHeld[k] > b1)
            {
                b4 = b3; fMn4 = fMn3;
                b3 = b2; fMn3 = fMn2;
                b2 = b1; fMn2 = fMn1;
                b1 = fHeld[k]; fMn1 = k;
            }
            else if (fHeld[k] > b2)
            {
                b4 = b3; fMn4 = fMn3;
                b3 = b2; fMn3 = fMn2;
                b2 = fHeld[k]; fMn2 = k;
            }
            else if (fHeld[k] > b3)
            {
                b4 = b3; fMn4 = fMn3;
                b3 = fHeld[k]; fMn3 = k;
            }
            else if (fHeld[k] > b4)
            {
                b4 = fHeld[k]; fMn4 = k;
            }
        }
        fHgt1 = fMn1 >= 0 ? 1.0 : 0.0;
        fHgt2 = fMn2 >= 0 ? 1.0 : 0.0;
        fHgt3 = fMn3 >= 0 ? 1.0 : 0.0;
        fHgt4 = fMn4 >= 0 ? 1.0 : 0.0;
        if (fMn1 >= 0)
            fHt1 = std::max(-36.0, std::min(36.0, double(fMn1) - fDet)) + fS5 / 100.0;
        if (fMn2 >= 0)
            fHt2 = std::max(-36.0, std::min(36.0, double(fMn2) - fDet)) - fS5 / 100.0;
        if (fMn3 >= 0)
            fHt3 = std::max(-36.0, std::min(36.0, double(fMn3) - fDet)) + fS5 / 200.0;
        if (fMn4 >= 0)
            fHt4 = std::max(-36.0, std::min(36.0, double(fMn4) - fDet)) - fS5 / 200.0;
    }

    // YIN on the last 2 * tmax decimated samples: sets voiced and tf
    void analyze()
    {
        const int tmax = int(fTmax);
        int j = fDw - 2 * tmax;
        if (j < 0)
            j += kDbs;
        for (int i = 0; i < 2 * tmax; ++i)
        {
            fFrm[i] = fDbuf[j];
            if (++j >= kDbs)
                j = 0;
        }
        double e = 0.0;
        for (int i = 0; i < tmax; ++i)
            e += fFrm[i] * fFrm[i];
        fCmn[0] = 1.0;
        double s = 0.0;
        for (int t = 1; t <= tmax; ++t)
        {
            double d = 0.0;
            for (int i = 0; i < tmax; ++i)
            {
                const double k = fFrm[i] - fFrm[i + t];
                d += k * k;
            }
            s += d;
            fCmn[t] = s > 0.0 ? d * double(t) / s : 1.0;
        }
        // the first dip under the threshold, followed down to its minimum
        int tau = 0;
        for (int t = int(fTmin); tau == 0 && t < tmax; ++t)
        {
            if (fCmn[t] < kThr)
            {
                while (t + 1 < tmax && fCmn[t + 1] < fCmn[t])
                    ++t;
                tau = t;
            }
        }
        fVoiced = tau > 0 && e > fTmax * 0.00001;
        if (fVoiced)
        {
            const double a = fCmn[tau - 1], b = fCmn[tau], c = fCmn[tau + 1];
            const double den = a - 2.0 * b + c;
            fTf = den > 0.0 ? double(tau) + 0.5 * (a - c) / den : double(tau);
        }
    }

    // pitch -> period for the marks, and the intervals of Mode Scale and MIDI
    void control()
    {
        fPerA = fPer;
        if (fVoiced)
        {
            fPer = std::min(fTf * fDec, fHm);
            fDet = 69.0 + 12.0 * std::log(fFa / fTf / 440.0) / fLn2;
            if (fMode == 1)
            {
                double best = 0.0, bd = 100.0;
                double k = std::floor(fDet + 0.5) - 6.0;
                for (int n = 0; n < 13; ++n)
                {
                    if (inSc(k))
                    {
                        const double dd = std::fabs(k - fDet);
                        if (dd < bd)
                        {
                            bd = dd;
                            best = k;
                        }
                    }
                    k += 1.0;
                }
                // stay on the current note until the pitch is 0.7 semitone from it (chromatic)
                if (fWasV && inSc(fCur) && std::fabs(fCur - fDet) < bd + 0.4)
                    best = fCur;
                fCur = best;
                setIv();
            }
            if (fMode == 2)
                midiIv();
            fWasV = true;
            fUvc = 0.0;
        }
        else
        {
            fUvc += 1.0;
            if (fUvc > fHoldN)
            {
                fWasV = false;
                fPer = fTuv;
            }
        }
    }

    // a voiced analysis mark goes to the highest peak of the low-passed signal within a
    // quarter period of c, refined between samples
    double snap(double c) const
    {
        const double r = std::floor(fPer / 4.0);
        double j = std::floor(c + 0.5) - r + fGd;
        const double j1 = std::min(std::floor(c + 0.5) + r + fGd, fN - 1.0);
        double bj = -1.0, bv = -1.0;
        int bk = 0;
        int k = fWp + int(j - fN);
        if (k < 0)
            k += fBs;
        for (int n = int(j1 - j + 1.0); n > 0; --n)
        {
            if (fLpb[size_t(k)] > bv)
            {
                bv = fLpb[size_t(k)];
                bj = j;
                bk = k;
            }
            if (++k >= fBs)
                k = 0;
            j += 1.0;
        }
        if (bj < 0.0)
            return c;
        const double ym = fLpb[size_t(bk > 0 ? bk - 1 : fBs - 1)];
        const double yp = fLpb[size_t(bk < fBs - 1 ? bk + 1 : 0)];
        const double den = ym - 2.0 * bv + yp;
        return den < 0.0 && ym <= bv && yp <= bv ? (bj - fGd) + (0.5 * (ym - yp)) / den : bj - fGd;
    }

    // adds a voice's grain for synthesis mark s, shifted by sh semitones, into its accumulator
    // and window sum; returns the output period (next mark distance)
    double place(double s, double sh, std::vector<double>& ac, std::vector<double>& aw)
    {
        const double t1 = fPerI / std::pow(2.0, sh / 12.0);
        const double h = std::min(std::max(fPerI, t1), fHm);
        // the analysis mark nearest to s, walking back from the newest
        int k = fMk;
        double c = fAmk[k];
        int cnt = 0;
        while (c > s && cnt < 63)
        {
            if (--k < 0)
                k += 64;
            const double a = fAmk[k];
            if (a <= s)
            {
                if (s - a <= c - s)
                    c = a;
                cnt = 63;
            }
            else
            {
                c = a;
                ++cnt;
            }
        }
        if (std::fabs(c - s) > fHm)
            c = s;
        const double d = std::floor(c - s);
        const double fr = c - s - d;
        const double i0 = std::ceil(s - h), i1 = std::floor(s + h);
        int ri = fWp + int(i0 - fN);
        if (ri < 0)
            ri += fBs;
        int si = ri + int(d);
        if (si < 0)
            si += fBs;
        else if (si >= fBs)
            si -= fBs;
        int s2 = si + 1;
        if (s2 >= fBs)
            s2 = 0;
        double i = i0;
        for (int n = int(i1 - i0 + 1.0); n > 0; --n)
        {
            const double w = 0.5 + 0.5 * std::cos(kPi * (i - s) / h);
            ac[size_t(ri)] += w * (fInM[size_t(si)] + (fInM[size_t(s2)] - fInM[size_t(si)]) * fr);
            aw[size_t(ri)] += w;
            if (++ri >= fBs)
                ri = 0;
            si = s2;
            if (++s2 >= fBs)
                s2 = 0;
            i += 1.0;
        }
        return t1;
    }

    static constexpr double kThr = 0.2; // YIN threshold (pitch_correction's Sensitivity 50%)

    double fSr = 48000.0, fWin = 1.0;
    std::vector<double> fBuf;
    int fSize = 1, fW = 0;
    double fPh1 = 0.0, fPh2 = 0.0, fPh3 = 0.0, fPh4 = 0.0;
    double fR1 = 1.0, fR2 = 1.0, fG1 = 1.0, fG2 = 1.0, fBase1 = 0.0, fBase2 = 0.0, fSp = 0.0, fDry = 1.0, fWet = 0.0;

    // Mode, Key, Scale, Formant Preserve
    double fS1 = 4.0, fS2 = 7.0, fS5 = 8.0, fKey = 0.0;
    int fMode = 0, fMask = 2741, fFb1 = 0, fFb2 = 0;

    // voices 3 and 4
    int fNv = 2, fFb3 = 0, fFb4 = 0, fMn3 = -1, fMn4 = -1;
    double fR3 = 1.0, fR4 = 1.0, fG3 = 1.0, fG4 = 1.0, fBase3 = 0.0, fBase4 = 0.0, fS14 = 12.0, fS15 = -12.0;
    double fHt3 = 0.0, fHt4 = 0.0, fHs3 = 0.0, fHs4 = 0.0, fHgt3 = 0.0, fHgt4 = 0.0, fHg3 = 0.0, fHg4 = 0.0;
    double fSn3 = 0.0, fSn4 = 0.0;
    std::vector<double> fAcc3, fAcc4, fAw3, fAw4;
    bool fFmt = false, fAdv = false;

    // pitch tracking and PSOLA
    double fDec = 4.0, fFa = 12000.0, fTmax = 172.0, fTmin = 12.0, fHopS = 0.0;
    double fHm = 686.0, fMlag = 0.0, fLat = 0.0;
    int fBs = 1;
    std::vector<double> fInM, fAcc1, fAcc2, fAw1, fAw2, fLpb;
    double fDbuf[kDbs] = {}, fFrm[1024] = {}, fCmn[512] = {}, fAmk[64] = {}, fHeld[128] = {};
    double fHoldN = 1.0, fTuv = 240.0, fLn2 = 0.6931471805599453, fGd = 17.0;
    double fHpC = 0.0, fB0a = 0.0, fB1a = 0.0, fA1a = 0.0, fA2a = 0.0, fB0b = 0.0, fB1b = 0.0, fA1b = 0.0, fA2b = 0.0;
    double fHpX = 0.0, fHpY = 0.0, fXa1 = 0.0, fXa2 = 0.0, fYa1 = 0.0, fYa2 = 0.0;
    double fXb1 = 0.0, fXb2 = 0.0, fYb1 = 0.0, fYb2 = 0.0;
    int fWp = 0, fDcnt = 0, fDw = 0, fHcnt = 0, fMk = 0;
    double fN = 0.0, fPer = 240.0, fPerA = 240.0, fPerI = 240.0, fAmLast = 0.0, fSn1 = 0.0, fSn2 = 0.0;
    bool fVoiced = false, fWasV = false, fHsNew = true;
    double fUvc = 0.0, fTf = 0.0, fDet = 60.0, fCur = -1.0;
    double fHt1 = 0.0, fHt2 = 0.0, fHs1 = 0.0, fHs2 = 0.0, fHgt1 = 0.0, fHgt2 = 0.0, fHg1 = 0.0, fHg2 = 0.0, fHk = 0.0;
    double fStamp = 0.0;
    int fMn1 = -1, fMn2 = -1;
};

} // namespace nullharmonizer
