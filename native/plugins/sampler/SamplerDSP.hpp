/*
 * NULL JSFX - Sampler, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/sampler.jsfx, computed in double like EEL2 and in the same
 * order: 16 voices that read the sample with 4-point Hermite interpolation, an ADSR, a 12 dB
 * low-pass with its own envelope, loop with declick, reverse, pitch bend, sustain pedal.
 * Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init, which also builds the built-in sound), setSample (a new
 * file, or none: the first @slider after it), setParams (@slider), then per block beginBlock
 * (@block) with the block's MIDI, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullsmp {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumParams20 = 22;           // slider2..slider23
static constexpr int kVoices = 16;                // NV
static constexpr int kMaxEvents = 256;
static constexpr size_t kMaxValues = 4194304;     // SMAX
static constexpr int kOverview = 512;             // OVN of the GFX version

struct MidiEv {
    uint32_t offset;
    uint8_t st, d1, d2;
};

// the waveform the display draws: the peak of each of kOverview parts (both channels)
inline void makeOverview(const double* buf, int nch, double nfr, float* ov)
{
    const int n = int(nfr);
    for (int j = 0; j < kOverview; ++j)
    {
        const int a = int(std::floor(double(j) * nfr / kOverview));
        const int b = std::max(a + 1, int(std::floor(double(j + 1) * nfr / kOverview)));
        double m = 0;
        for (int i = a; i < std::min(b, n); ++i)
            for (int c = 0; c < std::min(nch, 2); ++c)
                m = std::max(m, std::fabs(buf[size_t(i) * size_t(nch) + size_t(c)]));
        ov[j] = float(m);
    }
}

class Sampler {
public:
    struct Voice {
        bool on = false;
        double note = 0, gain = 0, pos = 0, rate = 0;
        int st = 0;                       // 1 attack, 2 decay/sustain, 3 release
        double env = 0, fe = 0;
        double s1l = 0, s2l = 0, s1r = 0, s2r = 0;
        double age = 0;
        bool held = false, ped = false;
        double tl = 0, tr = 0, ll = 0, lr = 0;
        double fc = 0, a1 = 0, a2 = 0, a3 = 0;
        bool in = false;
        double kst = 0;
        double end = 0;                   // where it stops: the sample's or its slice's end
    };

    // the JSFX's @init: the voices stop, the built-in sound is made at this rate
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (Voice& v : fV)
            v = Voice();
        fAge = 0;
        fPed = false;
        fBend = 0;
        fTl = fTr = 0;
        fPvLeft = 0;
        makeBuiltin();
        if (!fFileOk)
            useBuiltin();
    }

    // a new sample (interleaved, nch channels at fileRate), or none (empty): the voices stop,
    // the file plays (else the built-in sound). The vector is swapped in; the old data comes
    // back in it, to be freed off the audio thread.
    void setSample(std::vector<double>& data, int nch, double fileRate)
    {
        killAll();
        fFile.swap(data);
        const size_t n = nch > 0 ? fFile.size() - fFile.size() % size_t(nch) : 0;
        if (nch > 0 && fileRate > 0 && n >= size_t(4 * nch))
        {
            fFileOk = true;
            fBuf = fFile.data();
            fNch = nch;
            fFsr = fileRate;
            fNfr = double(n / size_t(nch));
        }
        else
        {
            fFileOk = false;
            useBuiltin();
        }
        ++smpId;
    }

    // slider2..slider21 (the JSFX @slider, without the file)
    void setParams(const double* s)
    {
        std::copy(s, s + kNumParams20, fS);
        fChrom = s[0] >= 0.5;
        fRoot = s[1];
        fBaseR = fFsr / fSr;
        double x = s[3] / 100;
        double y = fBend * s[18];
        fGSt = s[2] + x + y;
        x = s[4] / 1000;
        x *= fSr;
        fAttInc = 1 / std::max(1.0, x);
        x = s[5] / 1000;
        x *= fSr;
        fDecC = std::exp(-6.9 / std::max(1.0, x));
        fSus = s[6] / 100;
        x = s[7] / 1000;
        x *= fSr;
        fRelC = std::exp(-6.9 / std::max(1.0, x));
        fCut = s[8];
        x = s[9] / 100;
        x *= 1.334;
        fFk = 1.414 - x;
        x = s[10] / 100;
        fFamt = x * 6;
        x = s[11] / 1000;
        x *= fSr;
        fFeC = std::exp(-6.9 / std::max(1.0, x));
        fFOn = s[8] < 20000 || s[10] != 0;
        x = s[13] / 100;
        fLs = std::floor(x * fNfr);
        x = s[14] / 100;
        fLe = std::floor(x * fNfr);
        fLpOn = fChrom && s[12] >= 0.5 && fLe - fLs >= 64 && fSlN == 0;
        if (!fLpOn)
            fLe = fNfr + 1;
        x = s[15] / 100;
        fP0 = x * (fNfr - 1);
        fRev = s[16] >= 0.5;
        fSlN = s[20] >= 1 ? std::pow(2.0, s[20] + 1) : 0.0;
        fSKey = s[21];
        fVsens = s[17] / 100;
        fVol = std::pow(10.0, s[19] / 20);
        fFadeN = 0.003 * fSr;
        fKt = std::exp(-1 / (0.002 * fSr));
        rates();
    }

    // the JSFX's @block: the block's MIDI; preview plays the root key for 0.4 s (the
    // interface's click on the waveform)
    void beginBlock(const MidiEv* ev, uint32_t count, bool preview)
    {
        fNumEv = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + fNumEv, fEv);
        fEvIdx = 0;
        fSi = 0;
        if (preview)
        {
            fPvNote = fSlN > 0 ? fSKey : fRoot; // with Slices: the first slice
            noteOn(fPvNote, 0.8);
            fPvLeft = std::floor(0.4 * fSr);
        }
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t k = 0; k < frames; ++k)
        {
            double l, r;
            sample(l, r);
            outL[k] = float(l);
            outR[k] = float(r);
        }
    }

    // what the display reads
    bool fileOk() const { return fFileOk; }
    int channels() const { return fNch; }
    double fileRate() const { return fFsr; }
    double frames() const { return fNfr; }
    const double* buffer() const { return fBuf; }
    const Voice& voice(int i) const { return fV[i]; }
    bool reverse() const { return fRev; }
    const std::vector<double>& builtin() const { return fBuiltin; }
    int smpId = 0;

private:
    // ---- the sample ------------------------------------------------------------------------

    // make_builtin: a soft electric piano at C4 (mono, 1.5 s) at the current rate
    void makeBuiltin()
    {
        const double sr = fSr;
        const double nfr = std::floor(1.5 * sr);
        fBuiltin.assign(size_t(nfr), 0.0);
        const double d1 = 261.6255653005986 / sr;
        const double d2 = d1 * 2;
        const double d3 = d1 * 3;
        double p1 = 0, p2 = 0, p3 = 0;
        double e1 = 1, e2 = 1, e3 = 1, e4 = 1;
        const double c1 = std::exp(-1 / (0.3 * sr)), c2 = std::exp(-1 / (0.35 * sr));
        const double c3 = std::exp(-1 / (0.08 * sr)), c4 = std::exp(-1 / (0.2 * sr));
        const double a = std::floor(0.002 * sr);
        const double fo = std::floor(0.03 * sr);
        const double tp = 2 * kPi;
        for (size_t i = 0; i < size_t(nfr); ++i)
        {
            double s1 = tp * p1;
            s1 = std::sin(s1);
            double m = 1.3 * e3;
            m *= s1;
            double s2 = tp * p2;
            s2 = std::sin(s2 + m);
            double s3 = tp * p3;
            s3 = std::sin(s3);
            s1 *= e1;
            s2 *= e2;
            s3 *= e4;
            s1 *= 0.54;
            s2 *= 0.24;
            s3 *= 0.11;
            double y = s1 + s2 + s3;
            const double di = double(i);
            if (di < a)
            {
                const double x = di / a;
                y *= x;
            }
            const double n = nfr - di;
            if (n < fo)
            {
                const double x = n / fo;
                y *= x;
            }
            fBuiltin[i] = y;
            p1 += d1;
            p1 -= std::floor(p1);
            p2 += d2;
            p2 -= std::floor(p2);
            p3 += d3;
            p3 -= std::floor(p3);
            e1 *= c1;
            e2 *= c2;
            e3 *= c3;
            e4 *= c4;
        }
    }

    void useBuiltin()
    {
        fBuf = fBuiltin.data();
        fNch = 1;
        fFsr = fSr;
        fNfr = double(fBuiltin.size());
    }

    // smp: the value at frame i of channel c (the index kept in the sample)
    double smp(double i, int c) const
    {
        i = std::max(0.0, std::min(fNfr - 1, i));
        return fBuf[size_t(i) * size_t(fNch) + size_t(std::min(c, fNch - 1))];
    }

    // herm: 4-point Hermite at position q of channel c
    double herm(double q, int c) const
    {
        const double i = std::floor(q);
        const double f = q - i;
        const double xm = smp(i - 1, c), x0 = smp(i, c), x1 = smp(i + 1, c), x2 = smp(i + 2, c);
        const double c1 = 0.5 * (x1 - xm);
        const double c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    double rd(double p, int c) const { return herm(fRev ? fNfr - 1 - p : p, c); }

    // ---- the voices ------------------------------------------------------------------------

    void setRate(Voice& v)
    {
        const double r = std::pow(2.0, (v.kst + fGSt) / 12);
        v.rate = r * fBaseR;
    }

    void rates()
    {
        for (Voice& v : fV)
            setRate(v);
    }

    // v_kill: what the voice played last fades out (the steal tail)
    void kill(Voice& v)
    {
        if (v.on)
        {
            fTl += v.ll;
            fTr += v.lr;
        }
        v.on = false;
    }

    void killAll()
    {
        for (Voice& v : fV)
            kill(v);
    }

    void noteOn(double n, double vel)
    {
        // where the voice plays: from Start to the end, or its slice (Slices)
        double vs = fP0, ve = fNfr - 1;
        const double k = n - fSKey;
        if (fSlN > 0)
        {
            if (!(k >= 0 && k < fSlN))
                return; // no slice on this key: no voice
            vs = std::floor(k * fNfr / fSlN);
            ve = std::floor((k + 1) * fNfr / fSlN) - 1;
            if (fRev)
            {
                const double a = vs;
                vs = (fNfr - 1) - ve;
                ve = (fNfr - 1) - a;
            }
        }
        int b = -1;
        for (int i = 0; i < kVoices; ++i)
            if (b < 0 && !fV[i].on)
                b = i;
        if (b < 0)
        {
            double ba = -1;
            for (int i = 0; i < kVoices; ++i)
                if (fV[i].st == 3 && (ba < 0 || fV[i].age < ba))
                {
                    ba = fV[i].age;
                    b = i;
                }
            if (b < 0)
                for (int i = 0; i < kVoices; ++i)
                    if (ba < 0 || fV[i].age < ba)
                    {
                        ba = fV[i].age;
                        b = i;
                    }
        }
        Voice& v = fV[b];
        kill(v);
        fAge += 1;
        if (vs < ve)
        {
            double bv = fVsens * vel;
            bv += 1 - fVsens;
            v.on = true;
            v.note = n;
            v.gain = bv * 0.5;
            v.pos = vs;
            v.st = 1;
            v.env = 0;
            v.fe = 1;
            v.s1l = v.s2l = v.s1r = v.s2r = 0;
            v.age = fAge;
            v.held = true;
            v.ped = false;
            v.tl = v.tr = 0;
            v.ll = v.lr = 0;
            v.fc = 0;
            v.in = vs < fLe;
            v.kst = fChrom && fSlN == 0 ? n - fRoot : 0;
            v.end = ve;
            setRate(v);
        }
    }

    void noteOff(double n)
    {
        for (Voice& v : fV)
            if (v.on && v.held && v.note == n)
            {
                v.held = false;
                if (fChrom)
                {
                    if (fPed)
                        v.ped = true;
                    else
                        v.st = 3;
                }
            }
    }

    // release_free: the pedal let go (or all notes off): the notes no key holds release
    void releaseFree(bool all)
    {
        for (Voice& v : fV)
            if (v.on && fChrom && (v.ped || all))
            {
                v.ped = false;
                v.held = false;
                v.st = 3;
            }
    }

    // v_filter: the voice's filter coefficients from its filter envelope
    void filter(Voice& v)
    {
        double fc = fFamt * v.fe;
        fc = std::pow(2.0, fc);
        fc *= fCut;
        fc = std::max(20.0, std::min(0.45 * fSr, fc));
        fc /= fSr;
        const double g = std::tan(kPi * fc);
        fc = g + fFk;
        fc *= g;
        v.a1 = 1 / (1 + fc);
        v.a2 = g * v.a1;
        v.a3 = g * v.a2;
    }

    void midi(const MidiEv& e)
    {
        const int s = e.st & 0xF0;
        const double n = e.d1, vel = e.d2;
        if (s == 0x90 && vel > 0)
            noteOn(n, vel / 127);
        else if (s == 0x80 || s == 0x90)
            noteOff(n);
        else if (s == 0xE0)
        {
            fBend = vel * 128;
            fBend = (fBend + n - 8192) / 8192;
            const double x = fS[3] / 100;
            const double y = fBend * fS[18];
            fGSt = fS[2] + x + y;
            rates();
        }
        else if (s == 0xB0)
        {
            if (e.d1 == 64)
            {
                fPed = vel >= 64;
                if (!fPed)
                    releaseFree(false);
            }
            else if (e.d1 == 123)
                releaseFree(true);
            else if (e.d1 == 120)
                killAll();
        }
    }

    // the JSFX's @sample
    void sample(double& outL, double& outR)
    {
        while (fEvIdx < fNumEv && double(fEv[fEvIdx].offset) <= fSi)
        {
            midi(fEv[fEvIdx]);
            ++fEvIdx;
        }
        if (fPvLeft > 0)
        {
            fPvLeft -= 1;
            if (fPvLeft < 0.5)
                noteOff(fPvNote);
        }
        fSi += 1;

        double ol = 0, orr = 0;
        for (Voice& v : fV)
        {
            if (!v.on)
                continue;
            const int st = v.st;
            double e = v.env;
            if (st == 1)
            {
                e += fAttInc;
                if (e >= 1)
                {
                    e = 1;
                    v.st = 2;
                }
            }
            else if (st == 2)
                e = fSus + (e - fSus) * fDecC;
            else
                e *= fRelC;
            v.env = e;

            double p = v.pos;
            double xl = rd(p, 0) + v.tl;
            double xr = rd(p, 1) + v.tr;
            v.tl *= fKt;
            v.tr *= fKt;
            double g = e * v.gain;
            if (!fLpOn || !v.in)
            {
                double x = v.end - p;
                x /= v.rate;
                if (x < fFadeN)
                    g *= x / fFadeN;
            }
            xl *= g;
            xr *= g;

            if (fFOn)
            {
                if (v.fc <= 0)
                {
                    filter(v);
                    v.fc = 16;
                }
                v.fc -= 1;
                double v3 = xl - v.s2l;
                double v1 = v.a1 * v.s1l + v.a2 * v3;
                double v2 = v.s2l + v.a2 * v.s1l + v.a3 * v3;
                v.s1l = 2 * v1 - v.s1l;
                v.s2l = 2 * v2 - v.s2l;
                xl = v2;
                v3 = xr - v.s2r;
                v1 = v.a1 * v.s1r + v.a2 * v3;
                v2 = v.s2r + v.a2 * v.s1r + v.a3 * v3;
                v.s1r = 2 * v1 - v.s1r;
                v.s2r = 2 * v2 - v.s2r;
                xr = v2;
                v.fe *= fFeC;
            }
            v.ll = xl;
            v.lr = xr;
            ol += xl;
            orr += xr;

            p += v.rate;
            if (fLpOn && v.in && p >= fLe)
            {
                const double x = p - (fLe - fLs);
                v.tl += rd(p, 0) - rd(x, 0);
                v.tr += rd(p, 1) - rd(x, 1);
                p = x;
            }
            v.pos = p;
            if (p >= v.end || (st == 3 && e < 0.0001) || (st == 2 && fSus == 0 && e < 0.0001))
                v.on = false;
        }

        ol += fTl;
        orr += fTr;
        fTl *= fKt;
        fTr *= fKt;
        outL = ol * fVol;
        outR = orr * fVol;
    }

    double fSr = 44100;
    std::vector<double> fBuiltin, fFile;
    const double* fBuf = nullptr;
    int fNch = 1;
    double fFsr = 44100, fNfr = 0;
    bool fFileOk = false;

    double fS[kNumParams20] = {};
    bool fChrom = true, fFOn = false, fLpOn = false, fRev = false, fPed = false;
    double fRoot = 60, fBaseR = 1, fGSt = 0, fBend = 0;
    double fAttInc = 1, fDecC = 0, fSus = 1, fRelC = 0, fCut = 20000, fFk = 1.414, fFamt = 0, fFeC = 0;
    double fLs = 0, fLe = 0, fP0 = 0, fVsens = 1, fVol = 1, fFadeN = 0, fKt = 0, fSlN = 0, fSKey = 36;
    double fAge = 0, fTl = 0, fTr = 0, fPvLeft = 0, fPvNote = 60, fSi = 0;

    Voice fV[kVoices];
    MidiEv fEv[kMaxEvents];
    uint32_t fNumEv = 0, fEvIdx = 0;
};

} // namespace nullsmp
