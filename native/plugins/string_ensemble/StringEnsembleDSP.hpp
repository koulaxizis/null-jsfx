/*
 * NULL JSFX - String Ensemble, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/string_ensemble.jsfx, computed in double in the EEL2 order: a
 * voice per key (16', 8' and 4' PolyBLEP sawtooths locked an octave apart, free-running from
 * @init, one attack/release envelope), the sum through a 12 dB/oct TPT low-pass (Brightness) and
 * two RBJ peaking filters (the body), then the ensemble: three taps on one delay line moved by a
 * slow and a fast LFO 120 degrees apart. MIDI: notes, sustain (CC 64), mod wheel (CC 1, ensemble
 * depth), pitch bend (+-2 semitones), all notes off (CC 123/120). The delay line is allocated in
 * the constructor, never in process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullstrings {

static constexpr int kMaxEvents = 512;
static constexpr int kDL = 16384;

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

// the values the display needs, from the sliders
struct View {
    double sg = 1.0, eFs = 0.6, eFf = 6.0, eDsMs = 2.0, eDfMs = 0.25;
    Peak pk1, pk2;
    double srate = 48000.0;

    void set(const double* v, double sr)
    {
        srate = sr;
        const double fc = 600.0 * std::pow(16000.0 / 600.0, v[5] / 100.0);
        sg = std::tan(M_PI * std::min(fc, 0.45 * sr) / sr);
        const int mode = int(std::max(0.0, std::min(2.0, std::floor(v[7] + 0.5))));
        static const double kFs[3] = { 0.6, 0.5, 0.4 }, kFf[3] = { 6.0, 6.5, 5.5 }, kDs[3] = { 2.0, 2.4, 3.5 },
                            kDf[3] = { 0.25, 0.25, 0.4 };
        eFs = kFs[mode];
        eFf = kFf[mode];
        eDsMs = kDs[mode];
        eDfMs = kDf[mode];
        pk1.set(450.0, 4.0, 1.2, sr);
        pk2.set(2800.0, 3.0, 1.5, sr);
    }
};

class StringEnsemble {
public:
    StringEnsemble() : fDB(size_t(kDL), 0.0) {}

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        std::fill(fKD, fKD + 128, 0.0);
        std::fill(fKg, fKg + 128, 0.0);
        std::fill(fKe, fKe + 128, 0.0);
        std::fill(fKph, fKph + 128, 0.0);
        std::fill(fDB.begin(), fDB.end(), 0.0);
        for (int k = 0; k < 128; ++k)
            fKdt[k] = 440.0 * std::pow(2.0, (k - 69) / 12.0) / 2.0 / sr;
        fSus = false;
        fBend = 1.0;
        fMod = 0.0;
        fKlo = 128;
        fKhi = -1;
        fTs = 0.0;
        fDpos = 0;
        fPs = fPf = 0.0;
        fLp1 = fLp2 = 0.0;
        f1x1 = f1x2 = f1y1 = f1y2 = 0.0;
        f2x1 = f2x2 = f2y1 = f2y2 = 0.0;
        fPk1.set(450.0, 4.0, 1.2, sr);
        fPk2.set(2800.0, 3.0, 1.5, sr);
    }

    // @slider (values as the JSFX sliders hold them)
    void setParams(const double* v)
    {
        fC4 = v[0] / 100.0;
        fC8 = v[1] / 100.0;
        fC16 = v[2] / 100.0;
        fKa = 1.0 - std::exp(-2.302585 / (v[3] * 0.001 * fSr));
        fKr = std::exp(-6.907755 / (v[4] * 0.001 * fSr));
        const double fc = 600.0 * std::pow(16000.0 / 600.0, v[5] / 100.0);
        fSg = std::tan(M_PI * std::min(fc, 0.45 * fSr) / fSr);
        fSk = 1.4142135623730951;
        fSa1 = 1.0 / (1.0 + fSg * (fSg + fSk));
        fEns = v[6] / 100.0;
        fMode = int(std::max(0.0, std::min(2.0, std::floor(v[7] + 0.5))));
        if (fMode == 0)
        {
            fEfs = 0.6; fEff = 6.0; fEds = 2.0; fEdf = 0.25; fEsp = 0.5;
        }
        else if (fMode == 1)
        {
            fEfs = 0.5; fEff = 6.5; fEds = 2.4; fEdf = 0.25; fEsp = 1.0;
        }
        else
        {
            fEfs = 0.4; fEff = 5.5; fEds = 3.5; fEdf = 0.4; fEsp = 0.7;
        }
        fEbase = 0.008 * fSr;
        fEds *= 0.001 * fSr;
        fEdf *= 0.001 * fSr;
        fVol = std::pow(10.0, v[8] / 20.0);
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

private:
    static double saw(double t, double dt)
    {
        double y = 2.0 * t - 1.0;
        if (t < dt)
        {
            const double x = t / dt;
            y -= 2.0 * x - x * x - 1.0;
        }
        else if (t > 1.0 - dt)
        {
            const double x = (t - 1.0) / dt;
            y -= x * x + 2.0 * x + 1.0;
        }
        return y;
    }

    void noteOn(int n)
    {
        fKD[n] = 1.0;
        fKlo = std::min(fKlo, n);
        fKhi = std::max(fKhi, n);
        if (fKe[n] <= 0.0)
        {
            const double p = fTs * fKdt[n];
            fKph[n] = p - std::floor(p);
        }
        fKg[n] = 1.0;
    }

    void noteOff(int n)
    {
        fKD[n] = 0.0;
        if (!fSus)
            fKg[n] = 0.0;
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
        std::fill(fKD, fKD + 128, 0.0);
        std::fill(fKg, fKg + 128, 0.0);
        fSus = false;
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        const int n = int(d1) & 127;
        if (t == 0x90 && d2 > 0.0)
            noteOn(n);
        else if (t == 0x80 || t == 0x90)
            noteOff(n);
        else if (t == 0xE0)
            fBend = std::pow(2.0, ((d2 * 128.0 + d1) - 8192.0) / 8192.0 * 2.0 / 12.0);
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
                fMod = d2 / 127.0;
            else if (d1 == 123.0 || d1 == 120.0)
                allOff();
        }
    }

    double tap(double d) const
    {
        const double i = std::floor(d);
        const double fr = d - i;
        int p0 = fDpos - int(i);
        if (p0 < 0)
            p0 += kDL;
        int p1 = p0 - 1;
        if (p1 < 0)
            p1 += kDL;
        return fDB[size_t(p0)] + (fDB[size_t(p1)] - fDB[size_t(p0)]) * fr;
    }

    void sample(double& outL, double& outR)
    {
        while (fMqI < fMqN && double(fMq[fMqI].offset) <= fSi)
        {
            midiEv(fMq[fMqI].st, fMq[fMqI].d1, fMq[fMqI].d2);
            ++fMqI;
        }
        fSi += 1.0;

        // only the range that sounds or is held: a silent key adds nothing
        double s = 0.0;
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
                e *= fKr;
                if (e < 0.0001)
                    e = 0.0;
            }
            fKe[k] = e;
            if (e > 0.0)
            {
                const double dt = fKdt[k] * fBend;
                double p = fKph[k] + dt;
                p -= std::floor(p);
                fKph[k] = p;
                double y = 0.0;
                if (dt < 0.45)
                    y += fC16 * saw(p, dt);
                double q = 2.0 * p;
                q -= std::floor(q);
                if (2.0 * dt < 0.45)
                    y += fC8 * saw(q, 2.0 * dt);
                q = 4.0 * p;
                q -= std::floor(q);
                if (4.0 * dt < 0.45)
                    y += fC4 * saw(q, 4.0 * dt);
                s += y * e;
            }
            if (e > 0.0 || fKg[k] > 0.0)
            {
                nlo = std::min(nlo, k);
                nhi = k;
            }
        }
        fKlo = nlo;
        fKhi = nhi;
        fTs += 1.0;
        s *= 0.12;

        const double hp = (s - (fSk + fSg) * fLp1 - fLp2) * fSa1;
        const double bp = fSg * hp + fLp1;
        fLp1 = fSg * hp + bp;
        const double lw = fSg * bp + fLp2;
        fLp2 = fSg * bp + lw;
        const double y = fPk1.b0 * lw + fPk1.b1 * f1x1 + fPk1.b2 * f1x2 - fPk1.a1 * f1y1 - fPk1.a2 * f1y2;
        f1x2 = f1x1;
        f1x1 = lw;
        f1y2 = f1y1;
        f1y1 = y;
        const double z = fPk2.b0 * y + fPk2.b1 * f2x1 + fPk2.b2 * f2x2 - fPk2.a1 * f2y1 - fPk2.a2 * f2y2;
        f2x2 = f2x1;
        f2x1 = y;
        f2y2 = f2y1;
        f2y1 = z;

        fDB[size_t(fDpos)] = z;
        fPs += fEfs / fSr;
        fPs -= std::floor(fPs);
        fPf += fEff / fSr;
        fPf -= std::floor(fPf);
        const double tpi = 2.0 * M_PI;
        const double dm = 1.0 + fMod;
        const double d0 = fEbase + dm * (fEds * std::sin(tpi * fPs) + fEdf * std::sin(tpi * fPf));
        const double d1 = fEbase + dm * (fEds * std::sin(tpi * (fPs + 1.0 / 3.0)) + fEdf * std::sin(tpi * (fPf + 1.0 / 3.0)));
        const double d2 = fEbase + dm * (fEds * std::sin(tpi * (fPs + 2.0 / 3.0)) + fEdf * std::sin(tpi * (fPf + 2.0 / 3.0)));
        const double t0 = tap(d0), t1 = tap(d1), t2 = tap(d2);
        const double wl = (0.5 * t1 + (0.5 + 0.5 * fEsp) * t0 + (0.5 - 0.5 * fEsp) * t2) / 1.5;
        const double wr = (0.5 * t1 + (0.5 + 0.5 * fEsp) * t2 + (0.5 - 0.5 * fEsp) * t0) / 1.5;
        if (++fDpos >= kDL)
            fDpos = 0;

        outL = (z * (1.0 - fEns) + wl * fEns) * fVol;
        outR = (z * (1.0 - fEns) + wr * fEns) * fVol;
    }

    double fSr = 48000.0;
    double fKD[128] = {}, fKg[128] = {}, fKe[128] = {}, fKdt[128] = {}, fKph[128] = {};
    std::vector<double> fDB;
    bool fSus = false;
    double fBend = 1.0, fMod = 0.0, fTs = 0.0;
    int fDpos = 0, fMode = 0, fKlo = 128, fKhi = -1;
    double fPs = 0.0, fPf = 0.0, fLp1 = 0.0, fLp2 = 0.0;
    double f1x1 = 0.0, f1x2 = 0.0, f1y1 = 0.0, f1y2 = 0.0, f2x1 = 0.0, f2x2 = 0.0, f2y1 = 0.0, f2y2 = 0.0;
    Peak fPk1, fPk2;
    double fC4 = 0.8, fC8 = 1.0, fC16 = 0.5, fKa = 0.0, fKr = 0.0, fSg = 0.0, fSk = 1.4142135623730951, fSa1 = 1.0;
    double fEns = 0.7, fEfs = 0.6, fEff = 6.0, fEds = 0.0, fEdf = 0.0, fEsp = 0.5, fEbase = 0.0, fVol = 1.0;
    MidiEv fMq[kMaxEvents];
    uint32_t fMqN = 0, fMqI = 0;
    double fSi = 0.0;
};

} // namespace nullstrings
