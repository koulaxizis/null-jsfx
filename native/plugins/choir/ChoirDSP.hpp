/*
 * NULL JSFX - Choir, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/choir.jsfx, computed in double in the EEL2 order: a voice per key
 * with 1 to 8 singers (PolyBLEP sawtooths with detune, drift and a delayed vibrato, breath noise
 * pulsed by the glottal cycle, an exact Park-Miller step), a one-pole tilt per key, then seven
 * banks of five formant resonators per channel (soprano, alto, tenor, bass and three between)
 * for the vowel. A control tick every 16 samples sets the singers' pitch, the tilt, the MPE
 * pressure and timbre and the vowel. MIDI: notes, sustain (CC 64), mod wheel (CC 1, the vowel
 * towards U), pitch bend (+-2 semitones), all notes off (CC 123/120), MPE on channels 2-16.
 * Everything lives in fixed arrays: nothing is allocated in process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullchoir {

static constexpr int kMaxEvents = 512;
static constexpr int kBanks = 7;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

// the formant tables: soprano, alto, tenor, bass x A, E, I, O, U x (5 frequencies in Hz, 5 levels
// in dB, 5 bandwidths in Hz), as in the JSFX
static const double kFT[300] = {
    800, 1150, 2900, 3900, 4950, 0, -6, -32, -20, -50, 80, 90, 120, 130, 140,
    350, 2000, 2800, 3600, 4950, 0, -20, -15, -40, -56, 60, 100, 120, 150, 200,
    270, 2140, 2950, 3900, 4950, 0, -12, -26, -26, -44, 60, 90, 100, 120, 120,
    450, 800, 2830, 3800, 4950, 0, -11, -22, -22, -50, 70, 80, 100, 130, 135,
    325, 700, 2700, 3800, 4950, 0, -16, -35, -40, -60, 50, 60, 170, 180, 200,
    800, 1150, 2800, 3500, 4950, 0, -4, -20, -36, -60, 80, 90, 120, 130, 140,
    400, 1600, 2700, 3300, 4950, 0, -24, -30, -35, -60, 60, 80, 120, 150, 200,
    350, 1700, 2700, 3700, 4950, 0, -20, -30, -36, -60, 50, 100, 120, 150, 200,
    450, 800, 2830, 3500, 4950, 0, -9, -16, -28, -55, 70, 80, 100, 130, 135,
    325, 700, 2530, 3500, 4950, 0, -12, -30, -40, -64, 50, 60, 170, 180, 200,
    650, 1080, 2650, 2900, 3250, 0, -6, -7, -8, -22, 80, 90, 120, 130, 140,
    400, 1700, 2600, 3200, 3580, 0, -14, -12, -14, -20, 70, 80, 100, 120, 120,
    290, 1870, 2800, 3250, 3540, 0, -15, -18, -20, -30, 40, 90, 100, 120, 120,
    400, 800, 2600, 2800, 3000, 0, -10, -12, -12, -26, 40, 80, 100, 120, 120,
    350, 600, 2700, 2900, 3300, 0, -20, -17, -14, -26, 40, 60, 100, 120, 120,
    600, 1040, 2250, 2450, 2750, 0, -7, -9, -9, -20, 60, 70, 110, 120, 130,
    400, 1620, 2400, 2800, 3100, 0, -12, -9, -12, -18, 40, 80, 100, 120, 120,
    250, 1750, 2600, 3050, 3340, 0, -30, -16, -22, -28, 60, 90, 100, 120, 120,
    400, 750, 2400, 2600, 2900, 0, -11, -21, -20, -40, 40, 80, 100, 120, 120,
    350, 600, 2400, 2675, 2950, 0, -20, -32, -28, -36, 40, 80, 100, 120, 120,
};

// a formant value (o: 0 frequency, 5 level, 10 bandwidth) of bank s for vowel v0..v1 (ftv)
inline double ftv(int s, int o, int v0, int v1, double vf)
{
    const int t0 = s / 2, t1 = std::min(3, t0 + (s & 1));
    const double tf = (s & 1) * 0.5;
    const double a = kFT[t0 * 75 + v0 * 15 + o] * (1.0 - vf) + kFT[t0 * 75 + v1 * 15 + o] * vf;
    const double b = kFT[t1 * 75 + v0 * 15 + o] * (1.0 - vf) + kFT[t1 * 75 + v1 * 15 + o] * vf;
    return a * (1.0 - tf) + b * tf;
}

// the resonators of the seven banks for vowel v (0..4) and width wd (0..1): per bank, formant and
// channel b, c1, c2, gain (coefs); returns v clamped
inline double makeCoefs(double* co, double v, double wd, double srate)
{
    v = std::max(0.0, std::min(4.0, v));
    const int v0 = int(std::floor(v)), v1 = std::min(4, v0 + 1);
    const double vf = v - v0;
    for (int s = 0; s < kBanks; ++s)
        for (int k = 0; k < 5; ++k)
        {
            const double f = ftv(s, k, v0, v1, vf);
            const double db = ftv(s, 5 + k, v0, v1, vf);
            const double bw = ftv(s, 10 + k, v0, v1, vf);
            const double r = std::exp(-M_PI * bw / srate);
            const double g = std::pow(10.0, db / 20.0) * (1.0 + k * 0.5);
            for (int c = 0; c < 2; ++c)
            {
                const double w = 2.0 * M_PI * std::min(f * (c ? 1.0 + 0.006 * wd : 1.0), 0.45 * srate) / srate;
                double* i = co + ((s * 5 + k) * 2 + c) * 4;
                i[0] = (1.0 - r * r) * 0.5;
                i[1] = 2.0 * r * std::cos(w);
                i[2] = r * r;
                i[3] = g;
            }
        }
    return v;
}

// the response of bank s (left channel) at w (the display)
inline double bankMag(const double* co, int s, double w)
{
    double hr = 0.0, hi = 0.0;
    for (int k = 0; k < 5; ++k)
    {
        const double* c = co + (s * 10 + k * 2) * 4;
        const double nr = c[0] * (1.0 - std::cos(2.0 * w)), ni = c[0] * std::sin(2.0 * w);
        const double dr = 1.0 - c[1] * std::cos(w) + c[2] * std::cos(2.0 * w), di = c[1] * std::sin(w) - c[2] * std::sin(2.0 * w);
        const double d2 = dr * dr + di * di;
        hr += c[3] * (nr * dr + ni * di) / d2;
        hi += c[3] * (ni * dr - nr * di) / d2;
    }
    return std::sqrt(hr * hr + hi * hi);
}

class Choir {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::memset(fKD, 0, sizeof(fKD)); std::memset(fKg, 0, sizeof(fKg)); std::memset(fKe, 0, sizeof(fKe));
        std::memset(fKch, 0, sizeof(fKch)); std::memset(fKv, 0, sizeof(fKv)); std::memset(fKt, 0, sizeof(fKt));
        std::memset(fKtl, 0, sizeof(fKtl)); std::memset(fKtr, 0, sizeof(fKtr)); std::memset(fKta, 0, sizeof(fKta));
        std::memset(fKs0, 0, sizeof(fKs0)); std::memset(fKsf, 0, sizeof(fKsf));
        std::memset(fKpp, 0, sizeof(fKpp)); std::memset(fKtt, 0, sizeof(fKtt));
        std::memset(fSph, 0, sizeof(fSph)); std::memset(fSdet, 0, sizeof(fSdet)); std::memset(fSvr, 0, sizeof(fSvr));
        std::memset(fSvp, 0, sizeof(fSvp)); std::memset(fSdr, 0, sizeof(fSdr)); std::memset(fSinc, 0, sizeof(fSinc));
        std::memset(fCO, 0, sizeof(fCO)); std::memset(fST, 0, sizeof(fST)); std::memset(fBus, 0, sizeof(fBus));
        std::memset(fSact, 0, sizeof(fSact));
        std::memset(fCHB, 0, sizeof(fCHB)); std::memset(fCHP, 0, sizeof(fCHP)); std::memset(fCHT, 0, sizeof(fCHT));
        std::memset(fPanL, 0, sizeof(fPanL)); std::memset(fPanR, 0, sizeof(fPanR));
        for (int k = 0; k < 128; ++k)
            fKdt[k] = 440.0 * std::pow(2.0, (k - 69) / 12.0) / sr;
        fSus = false;
        fBendv = 0.0; fMwheel = 0.0; fVw = 0.0; fCurCh = 0; fMpeWas = false;
        fKlo = 128; fKhi = -1;
        fCtl = 0.0;
        fVowelC = -1.0; fWidthC = -1.0;
        fSeed = 12345.0;
    }

    // @slider (values as the JSFX sliders hold them)
    void setParams(const double* v)
    {
        fVowel = std::max(0.0, std::min(4.0, v[0]));
        fNv = int(std::max(1.0, std::min(8.0, std::floor(v[1] + 0.5))));
        fDetune = v[2];
        fBreath = v[3] / 100.0 * 1.6;
        fVib = v[4] / 100.0;
        fVrate = v[5];
        fSect = int(std::max(0.0, std::min(4.0, std::floor(v[6] + 0.5))));
        fKa = 1.0 - std::exp(-4.6 / (v[7] * 0.001 * fSr));
        fKr = 1.0 - std::exp(-4.6 / (v[8] * 0.001 * fSr));
        fBright = std::pow(2.0, (v[9] - 50.0) / 25.0);
        fVsens = v[10] / 100.0;
        fWidth = v[11] / 100.0;
        fMpe = v[12] >= 0.5;
        fMpeR = v[13];
        fMpeSm = 1.0 - std::exp(-16000.0 / (5.0 * fSr));
        fKw = 1.0 - std::exp(-16000.0 / (20.0 * fSr));
        if (fMpe && !fMpeWas)
        {
            std::memset(fCHB, 0, sizeof(fCHB));
            std::memset(fCHP, 0, sizeof(fCHP));
            std::memset(fCHT, 0, sizeof(fCHT));
        }
        fMpeWas = fMpe;
        fVol = std::pow(10.0, v[14] / 20.0);
        fGnv = 1.0 / std::sqrt(double(fNv));
        for (int i = 0; i < 8; ++i)
        {
            const double p = fNv > 1 ? 0.5 + ((i + 0.5) / fNv - 0.5) * fWidth : 0.5;
            fPanL[i] = std::sqrt(1.0 - p);
            fPanR[i] = std::sqrt(p);
        }
    }

    void beginBlock(const MidiEv* ev, uint32_t count)
    {
        fMqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + fMqN, fMq);
        fMqI = 0;
        fSi = 0.0;
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            double l, r;
            sample(l, r);
            outL[n] = float(l);
            outR[n] = float(r);
        }
    }

    // the keys sounding, 16 per word (the display)
    void soundingBits(uint32_t* bits) const
    {
        for (int i = 0; i < 8; ++i)
            bits[i] = 0;
        for (int k = 0; k < 128; ++k)
            if (fKe[k] > 0.0)
                bits[k / 16] |= 1u << (k % 16);
    }

    int keys() const
    {
        int c = 0;
        for (int k = 0; k < 128; ++k)
            c += fKe[k] > 0.0 ? 1 : 0;
        return c;
    }

    // the vowel sung now (the mod wheel included), -1 before the first tick
    double vowelNow() const { return fVowelC; }

private:
    double rnd()
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    void sections(int n)
    {
        if (fSect > 0)
        {
            fKs0[n] = (fSect - 1) * 2;
            fKsf[n] = 0.0;
        }
        else
        {
            const double ti = n >= 70 ? 0.0 : n <= 45 ? 6.0 : n >= 62 ? (70 - n) / 4.0 : n >= 53 ? 2.0 + (62 - n) / 4.5 : 4.0 + (53 - n) / 4.0;
            fKs0[n] = std::min(6.0, std::floor(ti));
            fKsf[n] = ti - fKs0[n];
        }
    }

    void noteOn(int n, double vel, int ch)
    {
        fKD[n] = 1.0;
        fKlo = std::min(fKlo, n);
        fKhi = std::max(fKhi, n);
        if (fKe[n] <= 0.0)
        {
            for (int i = 0; i < 8; ++i)
            {
                const int j = n * 8 + i;
                fSph[j] = rnd();
                fSdet[j] = 2.0 * rnd() - 1.0;
                fSvr[j] = 0.9 + 0.2 * rnd();
                fSvp[j] = rnd();
                fSdr[j] = 0.0;
                fSinc[j] = fKdt[n];
            }
            fKt[n] = 0.0; fKtl[n] = 0.0; fKtr[n] = 0.0;
            fKpp[n] = 0.0; fKtt[n] = 0.0;
        }
        fKv[n] = 1.0 - fVsens * (1.0 - vel);
        fKch[n] = ch;
        sections(n);
        fKg[n] = 1.0;
    }

    void noteOff(int n, int ch)
    {
        if (!fMpe || fKch[n] == ch)
        {
            fKD[n] = 0.0;
            if (!fSus)
                fKg[n] = 0.0;
        }
    }

    void susOff()
    {
        fSus = false;
        for (int k = 0; k < 128; ++k)
            if (fKD[k] == 0.0)
                fKg[k] = 0.0;
    }

    void allOff()
    {
        std::memset(fKD, 0, sizeof(fKD));
        std::memset(fKg, 0, sizeof(fKg));
        fSus = false;
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        fCurCh = int(st) & 0x0F;
        const int n = int(d1) & 127;
        if (fMpe && fCurCh > 0 && (t == 0xE0 || t == 0xD0 || (t == 0xB0 && d1 == 74.0)))
        {
            if (t == 0xE0)
                fCHB[fCurCh] = (d2 * 128.0 + d1 - 8192.0) / 8192.0;
            else if (t == 0xD0)
                fCHP[fCurCh] = d1 / 127.0;
            else
                fCHT[fCurCh] = (d2 - 64.0) / 64.0;
        }
        else if (t == 0x90 && d2 > 0.0)
            noteOn(n, d2 / 127.0, fCurCh);
        else if (t == 0x80 || t == 0x90)
            noteOff(n, fCurCh);
        else if (t == 0xE0)
            fBendv = (d2 * 128.0 + d1 - 8192.0) / 8192.0;
        else if (t == 0xB0)
        {
            if (d1 == 64.0)
            {
                if (d2 >= 64.0)
                    fSus = true;
                else if (fSus)
                    susOff();
            }
            else if (d1 == 1.0)
                fMwheel = d2 / 127.0;
            else if (d1 == 123.0 || d1 == 120.0)
                allOff();
        }
    }

    void tick()
    {
        fVw += (fMwheel - fVw) * fKw;
        const double vt = fVowel + fVw * (4.0 - fVowel);
        if (std::fabs(vt - fVowelC) > 0.0001 || std::fabs(fWidth - fWidthC) > 0.00001)
        {
            fVowelC = makeCoefs(fCO, vt, fWidth, fSr);
            fWidthC = fWidth;
        }
        for (int k = fKlo; k <= fKhi; ++k)
        {
            if (!(fKe[k] > 0.0 || fKg[k] > 0.0))
                continue;
            double bs;
            if (fMpe)
            {
                fKpp[k] += (fCHP[fKch[k]] - fKpp[k]) * fMpeSm;
                fKtt[k] += (fCHT[fKch[k]] - fKtt[k]) * fMpeSm;
                bs = 2.0 * fBendv + (fKch[k] > 0 ? fMpeR * fCHB[fKch[k]] : 0.0);
            }
            else
                bs = 2.0 * fBendv;
            const double vd = fVib * 0.45 * std::max(0.0, std::min(1.0, (fKt[k] / fSr - 0.25) / 0.6));
            for (int i = 0; i < fNv; ++i)
            {
                const int j = k * 8 + i;
                fSdr[j] += 0.0064 * ((2.0 * rnd() - 1.0) * 3.0 - fSdr[j]);
                fSvp[j] += fSvr[j] * fVrate * 16.0 / fSr;
                fSvp[j] -= std::floor(fSvp[j]);
                const double sem = (fNv > 1 ? fSdet[j] * fDetune / 100.0 : 0.0) + fSdr[j] * 0.06 + vd * std::sin(2.0 * M_PI * fSvp[j]) + bs;
                fSinc[j] = std::min(0.45, fKdt[k] * std::pow(2.0, sem / 12.0));
            }
            const double fc = (1500.0 + 3500.0 * fKv[k]) * fBright * (fMpe ? std::pow(2.0, 2.0 * fKtt[k]) : 1.0);
            fKta[k] = std::exp(-2.0 * M_PI * std::min(fc, 0.45 * fSr) / fSr);
        }
    }

    void sample(double& outL, double& outR)
    {
        while (fMqI < fMqN && double(fMq[fMqI].offset) <= fSi)
        {
            midiEv(fMq[fMqI].st, fMq[fMqI].d1, fMq[fMqI].d2);
            ++fMqI;
        }
        fSi += 1.0;
        if (fCtl <= 0.0)
        {
            tick();
            fCtl = 16.0;
        }
        fCtl -= 1.0;

        int nlo = 128, nhi = -1;
        for (int k = fKlo; k <= fKhi; ++k)
        {
            double e = fKe[k];
            if (fKg[k] > 0.0)
            {
                e += fKa * (1.0 - e);
                if (e > 0.9999)
                    e = 1.0;
            }
            else
            {
                e -= fKr * e;
                if (e < 0.0001)
                    e = 0.0;
            }
            fKe[k] = e;
            if (e > 0.0)
            {
                double sl = 0.0, sr = 0.0, pl = 0.0, pr = 0.0;
                int j = k * 8;
                for (int i = 0; i < fNv; ++i, ++j)
                {
                    const double dt = fSinc[j];
                    double p = fSph[j] + dt;
                    if (p >= 1.0)
                        p -= 1.0;
                    fSph[j] = p;
                    double x = 2.0 * p - 1.0;
                    if (p < dt)
                    {
                        const double q = p / dt;
                        x -= q + q - q * q - 1.0;
                    }
                    else if (p > 1.0 - dt)
                    {
                        const double q = (p - 1.0) / dt;
                        x -= q * q + q + q + 1.0;
                    }
                    const double u = p < 0.4 ? 1.0 : 0.35;
                    sl -= x * fPanL[i];
                    sr -= x * fPanR[i];
                    pl += u * u * fPanL[i] * fPanL[i];
                    pr += u * u * fPanR[i] * fPanR[i];
                }
                const double nz = (2.0 * rnd() - 1.0) * fBreath;
                const double a = fKta[k];
                fKtl[k] = (sl + nz * std::sqrt(pl)) * fGnv * (1.0 - a) + a * fKtl[k];
                fKtr[k] = (sr + nz * std::sqrt(pr)) * fGnv * (1.0 - a) + a * fKtr[k];
                const double g = e * (0.35 + 0.65 * fKv[k]) * (fMpe ? 1.0 + fKpp[k] : 1.0) * 0.6;
                const int s0 = int(fKs0[k]);
                const double sf = fKsf[k];
                fBus[s0 * 2] += fKtl[k] * g * (1.0 - sf);
                fBus[s0 * 2 + 1] += fKtr[k] * g * (1.0 - sf);
                fSact[s0] = fSr;
                if (sf > 0.0)
                {
                    fBus[s0 * 2 + 2] += fKtl[k] * g * sf;
                    fBus[s0 * 2 + 3] += fKtr[k] * g * sf;
                    fSact[s0 + 1] = fSr;
                }
                fKt[k] += 1.0;
            }
            if (e > 0.0 || fKg[k] > 0.0)
            {
                nlo = std::min(nlo, k);
                nhi = k;
            }
        }
        fKlo = nlo;
        fKhi = nhi;

        double ol = 0.0, orr = 0.0;
        for (int s = 0; s < kBanks; ++s)
        {
            if (fSact[s] > 0.0)
            {
                fSact[s] -= 1.0;
                for (int k = 0; k < 10; ++k)
                {
                    const double* c = fCO + (s * 10 + k) * 4;
                    double* z = fST + (s * 10 + k) * 4;
                    const double x = fBus[s * 2 + (k & 1)];
                    const double y = c[0] * (x - z[1]) + c[1] * z[2] - c[2] * z[3];
                    z[1] = z[0]; z[0] = x; z[3] = z[2]; z[2] = y;
                    if (k & 1)
                        orr += c[3] * y;
                    else
                        ol += c[3] * y;
                }
                if (fSact[s] <= 0.0)
                    std::memset(fST + s * 40, 0, 40 * sizeof(double));
            }
            fBus[s * 2] = 0.0;
            fBus[s * 2 + 1] = 0.0;
        }
        outL = ol * fVol;
        outR = orr * fVol;
    }

    double fSr = 48000.0;
    double fKD[128], fKg[128], fKe[128], fKv[128], fKt[128], fKtl[128], fKtr[128], fKta[128];
    double fKs0[128], fKsf[128], fKdt[128], fKpp[128], fKtt[128];
    int fKch[128];
    double fSph[1024], fSdet[1024], fSvr[1024], fSvp[1024], fSdr[1024], fSinc[1024];
    double fCO[kBanks * 40], fST[kBanks * 40], fBus[kBanks * 2], fSact[kBanks];
    double fCHB[16], fCHP[16], fCHT[16], fPanL[8], fPanR[8];
    bool fSus = false, fMpe = false, fMpeWas = false;
    double fBendv = 0.0, fMwheel = 0.0, fVw = 0.0;
    int fCurCh = 0, fKlo = 128, fKhi = -1, fNv = 8, fSect = 0;
    double fCtl = 0.0, fVowelC = -1.0, fWidthC = -1.0, fSeed = 12345.0;
    double fVowel = 0.0, fDetune = 11.0, fBreath = 0.4, fVib = 0.35, fVrate = 5.4, fKa = 0.0, fKr = 0.0;
    double fBright = 1.0, fVsens = 1.0, fWidth = 1.0, fMpeR = 48.0, fMpeSm = 0.0, fKw = 0.0, fVol = 1.0, fGnv = 1.0;

    MidiEv fMq[kMaxEvents];
    uint32_t fMqN = 0, fMqI = 0;
    double fSi = 0.0;
};

} // namespace nullchoir
