/*
 * NULL JSFX - Modal Piano, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/modal_piano.jsfx, computed in double in the EEL2 order: 16
 * voices of up to 24 stiff-string partials, each a two-pole resonator started by the hammer (the
 * first six as a prompt and an aftersound pair, a little apart in pitch), a damper per key, the
 * sustain and soft pedals, a hammer thump of Park-Miller noise and the body (two RBJ peaking
 * filters per side). Resonators that fall 100 dB are dropped, one voice every 16 samples, in the
 * JSFX's order. MIDI: notes, CC 64, CC 67, all notes off (CC 123/120). All memory is a member,
 * nothing is allocated in process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullpiano {

static constexpr int kMaxEvents = 512;
static constexpr int kNV = 16;          // voices
static constexpr int kNP = 24;          // partials per voice
static constexpr int kRS = kNP * 2 * 7; // resonator memory per voice

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

struct Peak {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    void set(double f, double gdb, double q, double srate)
    {
        const double A = std::pow(10.0, gdb / 40.0);
        const double w = 2.0 * M_PI * std::min(f, 0.45 * srate) / srate;
        const double cw = std::cos(w);
        const double al = std::sin(w) / (2.0 * q);
        const double a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0;
        b1 = -2.0 * cw / a0;
        b2 = (1.0 - al * A) / a0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - al / A) / a0;
    }

    // the magnitude at w (the display)
    double mag(double w) const
    {
        const double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.0 * w), s2 = std::sin(2.0 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -b1 * s1 - b2 * s2;
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -a1 * s1 - a2 * s2;
        return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

// the model of one key, shared by the DSP and the display
struct Key {
    double f0, B, T0;
    static Key of(int n, double inh, double dec)
    {
        Key k;
        k.f0 = 440.0 * std::pow(2.0, (n - 69) / 12.0);
        k.B = 0.00025 * std::pow(10.0, std::max(0.0, double(n - 60)) / 30.0) * inh;
        k.T0 = 22.0 * std::pow(2.0, -(n - 21) / 17.0) * dec;
        return k;
    }
};

class ModalPiano {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::fill(fKD, fKD + 128, 0.0);
        std::fill(fKV, fKV + 128, -1);
        for (int v = 0; v < kNV; ++v)
        {
            fVkey[v] = -1;
            fVn[v] = 0;
            fVlv[v] = fVlr[v] = fVat[v] = fVta[v] = fVgl[v] = fVgr[v] = fVbt[v] = fVbv[v] = 0.0;
            fVdm[v] = false;
        }
        std::fill(fRB, fRB + kNV * kRS, 0.0);
        fSus = fSoft = false;
        fPc = 0;
        fPv = 0;
        fSeed = 4711.0;
        fThLp = 0.0;
        for (double* z : { &b1x1, &b1x2, &b1y1, &b1y2, &b2x1, &b2x2, &b2y1, &b2y2, &b3x1, &b3x2, &b3y1, &b3y2,
                           &b4x1, &b4x2, &b4y1, &b4y2 })
            *z = 0.0;
    }

    // @slider (values as the JSFX sliders hold them)
    void setParams(const double* v)
    {
        fHard = std::pow(2.0, (v[0] - 50.0) / 40.0);
        fDec = v[1] / 100.0;
        fUni = v[2];
        fInh = v[3] / 100.0;
        fNoi = v[4] / 100.0;
        fVelo = v[5] / 100.0;
        fDmp = std::exp(-6.907755 / (v[6] * 0.001 * fSr));
        fBod = v[7] / 100.0;
        fWid = v[8] / 100.0;
        fVol = std::pow(10.0, v[9] / 20.0);
        fBl.set(110.0, 6.0 * fBod, 0.9, fSr);
        fBh.set(2500.0, 3.0 * fBod, 1.1, fSr);
        fThC = std::exp(-2.0 * M_PI * 2500.0 / fSr);
        for (int i = 0; i < kNV; ++i)
            if (fVkey[i] >= 0 && fVdm[i])
                damp(i, true);
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
        for (int v = 0; v < kNV; ++v)
            if (fVkey[v] >= 0)
                bits[fVkey[v] / 16] |= 1u << (fVkey[v] % 16);
    }

    int voices() const
    {
        int c = 0;
        for (int v = 0; v < kNV; ++v)
            c += fVkey[v] >= 0 ? 1 : 0;
        return c;
    }

    int pedals() const { return (fSus ? 1 : 0) | (fSoft ? 2 : 0); }

private:
    double* res(int v) { return fRB + v * kRS; }

    void damp(int v, bool d)
    {
        fVdm[v] = d;
        const double g1 = d ? fDmp : 1.0;
        const double g2 = g1 * g1;
        double* p = res(v);
        for (int i = 0, n = fVn[v]; i < n; ++i, p += 7)
        {
            p[2] = p[0] * g1;
            p[3] = p[1] * g2;
        }
    }

    int voiceFor(int n)
    {
        int v = fKV[n];
        if (v < 0)
        {
            for (int i = 0; i < kNV; ++i)
                if (v < 0 && fVkey[i] < 0)
                    v = i;
            if (v < 0)
            {
                v = 0;
                double lo = fVlv[0];
                for (int i = 1; i < kNV; ++i)
                    if (fVlv[i] < lo)
                    {
                        v = i;
                        lo = fVlv[i];
                    }
                fKV[fVkey[v]] = -1;
            }
            std::fill(res(v), res(v) + kRS, 0.0);
            fVkey[v] = n;
            fVn[v] = 0;
            fVlv[v] = 0.0;
            fVdm[v] = false;
            fKV[n] = v;
        }
        return v;
    }

    void strike(int n, double vel)
    {
        const int v = voiceFor(n);
        const double vn = vel / 127.0;
        double vh = 0.65 + (vn - 0.65) * fVelo;
        vh = std::max(0.05, std::min(1.0, vh));
        const Key key = Key::of(n, fInh, fDec);
        const double f0 = key.f0, B = key.B, T0 = key.T0;
        double fh = (900.0 + 7000.0 * vh * vh) * fHard;
        double g = std::pow(vh, 0.6) * (0.25 + 0.75 * vh);
        if (fSoft)
        {
            fh *= 0.6;
            g *= 0.7;
        }
        const double ns = n < 32 ? 0.2 : n < 44 ? 0.7 : 1.0;
        const double dc = fUni * ns / 2.0;
        const double fmax = std::min(14000.0, 0.45 * fSr);
        const double tpi = 2.0 * M_PI;
        for (int i = 0; i < kNP * 2; ++i)
            fMP[i] = -1;
        {
            double* p = res(v);
            for (int i = 0, cnt = fVn[v]; i < cnt; ++i, p += 7)
                fMP[int(p[6])] = i;
        }
        double lv = 0.0;
        for (int k = 1;; ++k)
        {
            const double fk = k * f0 * std::sqrt(1.0 + B * k * k);
            if (!(fk < fmax && k <= kNP))
                break;
            const double comb = std::fabs(std::sin(M_PI * k / 8.0));
            const double amp = comb / std::pow(double(k), 0.9) * std::exp(-fk / fh) * g;
            const double T = T0 / (1.0 + std::pow(fk / 1800.0, 1.4));
            if (!(amp > 0.000001))
                continue;
            const int reps = k <= 6 ? 2 : 1;
            for (int j = 0; j < reps; ++j)
            {
                const int id = (k - 1) * 2 + j;
                if (fMP[id] < 0)
                {
                    fMP[id] = fVn[v];
                    double* q = res(v) + fVn[v] * 7;
                    q[4] = q[5] = 0.0;
                    q[6] = id;
                    fVn[v] += 1;
                }
                double* p = res(v) + fMP[id] * 7;
                double f, r, a;
                if (k <= 6)
                {
                    f = fk * std::pow(2.0, (j ? dc : -dc) / 1200.0);
                    r = std::exp(-6.907755 / ((j ? T : T / 5.0) * fSr));
                    a = (j ? 0.35 : 0.65) * amp;
                }
                else
                {
                    f = fk;
                    r = std::exp(-6.907755 / (T / 2.5 * fSr));
                    a = amp;
                }
                const double w = tpi * f / fSr;
                p[0] = 2.0 * r * std::cos(w);
                p[1] = r * r;
                p[4] -= a * std::sin(w) / r;
                p[5] -= a * std::sin(2.0 * w) / (r * r);
                if (k == 1 && j)
                    fVlr[v] = r;
            }
            lv += amp;
        }
        fVlv[v] += lv;
        damp(v, false);
        fVat[v] = 0.0;
        fVta[v] = std::max(1.0, (0.0015 - 0.0011 * vh) * fSr);
        const double pan = std::max(-1.0, std::min(1.0, (n - 60) / 36.0));
        fVgl[v] = std::cos((1.0 + pan * fWid) * M_PI / 4.0);
        fVgr[v] = std::sin((1.0 + pan * fWid) * M_PI / 4.0);
        fVbt[v] = 0.0;
        fVbv[v] = 0.08 * fNoi * vh;
    }

    void prune(int v)
    {
        double* p = res(v);
        int i = 0;
        while (i < fVn[v])
        {
            const double e = p[4] * p[4] + p[1] * p[5] * p[5] - p[0] * p[4] * p[5];
            if (e < 0.0000000001 * (1.0 - p[0] * p[0] / (4.0 * p[1])))
            {
                fVn[v] -= 1;
                const double* q = res(v) + fVn[v] * 7;
                std::copy(q, q + 7, p);
            }
            else
            {
                p += 7;
                ++i;
            }
        }
    }

    bool lifted(int n) const { return fKD[n] != 0.0 || fSus || n > 90; }

    void noteOn(int n, double vel)
    {
        fKD[n] = 1.0;
        strike(n, vel);
    }

    void noteOff(int n)
    {
        fKD[n] = 0.0;
        if (fKV[n] >= 0 && !lifted(n))
            damp(fKV[n], true);
    }

    void susOff()
    {
        fSus = false;
        for (int i = 0; i < kNV; ++i)
            if (fVkey[i] >= 0 && !fVdm[i] && !lifted(fVkey[i]))
                damp(i, true);
    }

    void allOff()
    {
        std::fill(fKD, fKD + 128, 0.0);
        fSus = false;
        for (int i = 0; i < kNV; ++i)
            if (fVkey[i] >= 0 && !fVdm[i] && fVkey[i] <= 90)
                damp(i, true);
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        const int n = int(d1) & 127;
        if (t == 0x90 && d2 > 0.0)
            noteOn(n, d2);
        else if (t == 0x80 || t == 0x90)
            noteOff(n);
        else if (t == 0xB0)
        {
            if (d1 == 64.0)
            {
                if (d2 >= 64.0)
                    fSus = true;
                else if (fSus)
                    susOff();
            }
            else if (d1 == 67.0)
                fSoft = d2 >= 64.0;
            else if (d1 == 123.0 || d1 == 120.0)
                allOff();
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

        const double tpi = 2.0 * M_PI;
        const double nb = 0.006 * fSr;
        double sl = 0.0, sr = 0.0, th = 0.0;
        for (int v = 0; v < kNV; ++v)
        {
            if (fVkey[v] < 0)
                continue;
            double s = 0.0;
            double* p = res(v);
            for (int i = 0, cnt = fVn[v]; i < cnt; ++i, p += 7)
            {
                const double y = p[2] * p[4] - p[3] * p[5];
                p[5] = p[4];
                p[4] = y;
                s += y;
            }
            if (fVat[v] < fVta[v])
            {
                s *= fVat[v] / fVta[v];
                fVat[v] += 1.0;
            }
            if (fVbt[v] < nb)
            {
                th += fVbv[v] * (0.5 - 0.5 * std::cos(tpi * fVbt[v] / nb));
                fVbt[v] += 1.0;
            }
            sl += s * fVgl[v];
            sr += s * fVgr[v];
            fVlv[v] *= fVdm[v] ? fVlr[v] * fDmp : fVlr[v];
            if (fVn[v] == 0 && fVbt[v] >= nb)
            {
                fKV[fVkey[v]] = -1;
                fVkey[v] = -1;
            }
        }

        if (++fPc >= 16)
        {
            fPc = 0;
            if (fVkey[fPv] >= 0)
                prune(fPv);
            if (++fPv >= kNV)
                fPv = 0;
        }

        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        fThLp = (1.0 - fThC) * (fSeed / 1073741824.0 - 1.0) * th + fThC * fThLp;
        sl = (sl + fThLp * 0.7071) * 0.25;
        sr = (sr + fThLp * 0.7071) * 0.25;

        double y = fBl.b0 * sl + fBl.b1 * b1x1 + fBl.b2 * b1x2 - fBl.a1 * b1y1 - fBl.a2 * b1y2;
        b1x2 = b1x1; b1x1 = sl; b1y2 = b1y1; b1y1 = y;
        const double zl = fBh.b0 * y + fBh.b1 * b2x1 + fBh.b2 * b2x2 - fBh.a1 * b2y1 - fBh.a2 * b2y2;
        b2x2 = b2x1; b2x1 = y; b2y2 = b2y1; b2y1 = zl;
        y = fBl.b0 * sr + fBl.b1 * b3x1 + fBl.b2 * b3x2 - fBl.a1 * b3y1 - fBl.a2 * b3y2;
        b3x2 = b3x1; b3x1 = sr; b3y2 = b3y1; b3y1 = y;
        const double zr = fBh.b0 * y + fBh.b1 * b4x1 + fBh.b2 * b4x2 - fBh.a1 * b4y1 - fBh.a2 * b4y2;
        b4x2 = b4x1; b4x1 = y; b4y2 = b4y1; b4y1 = zr;

        outL = zl * fVol;
        outR = zr * fVol;
    }

    double fSr = 48000.0;
    double fKD[128] = {};
    int fKV[128] = {};
    int fVkey[kNV] = {}, fVn[kNV] = {};
    bool fVdm[kNV] = {};
    double fVlv[kNV] = {}, fVlr[kNV] = {}, fVat[kNV] = {}, fVta[kNV] = {}, fVgl[kNV] = {}, fVgr[kNV] = {},
           fVbt[kNV] = {}, fVbv[kNV] = {};
    double fRB[kNV * kRS] = {};
    int fMP[kNP * 2] = {};
    bool fSus = false, fSoft = false;
    int fPc = 0, fPv = 0;
    double fSeed = 4711.0, fThLp = 0.0, fThC = 0.0;
    double b1x1 = 0, b1x2 = 0, b1y1 = 0, b1y2 = 0, b2x1 = 0, b2x2 = 0, b2y1 = 0, b2y2 = 0;
    double b3x1 = 0, b3x2 = 0, b3y1 = 0, b3y2 = 0, b4x1 = 0, b4x2 = 0, b4y1 = 0, b4y2 = 0;
    Peak fBl, fBh;
    double fHard = 1.0, fDec = 1.0, fUni = 1.2, fInh = 1.0, fNoi = 0.3, fVelo = 0.7, fDmp = 0.0, fBod = 0.5,
           fWid = 0.6, fVol = 1.0;
    MidiEv fMq[kMaxEvents];
    uint32_t fMqN = 0, fMqI = 0;
    double fSi = 0.0;
};

} // namespace nullpiano
