/*
 * NULL JSFX - Granular, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/granular.jsfx, computed in double in the EEL2 order: 8 key
 * voices, each starting Density Hann-windowed grains a second from Position (+ Scan, scattered
 * by Spray) of the sound, at the key's pitch from Root Key (+ Pitch Spread), panned by Stereo
 * Spread and backwards with the chance Reverse; up to 64 grains, read linearly, wrapping. The
 * random draws come from one Park-Miller generator in the JSFX's order. Without a file the
 * built-in sound (eight swelling harmonics of C4, 2 s) plays.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullgrn {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kVoices = 8;                 // NV
static constexpr int kGrains = 64;                // NG
static constexpr int kMaxEvents = 512;
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

class Granular {
public:
    struct Voice {
        double on = 0, note = 0, gain = 0, env = 0, st = 0, cnt = 0, scan = 0, age = 0;
    };
    struct Grain {
        double on = 0, pos = 0, rate = 0, t = 0, len = 0, gl = 0, gr = 0;
        int voice = 0;
    };

    // @init at a new rate: the voices and grains stop, the generator starts again, the
    // built-in sound is made at this rate
    void setSampleRate(double sr)
    {
        fSr = sr;
        killAll();
        fSeed = 12345.0;
        makeBuiltin();
        if (!fFileOk)
            useBuiltin();
        derive();
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
        derive();
    }

    // slider2..slider14 (the JSFX @slider, without the file)
    void setParams(const double* v)
    {
        for (int i = 0; i < 13; ++i)
            fS[i] = v[i];
        derive();
    }

    void beginBlock(const MidiEv* ev, uint32_t count)
    {
        fMqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + fMqN, fMq);
        fMqI = 0;
        fSi = 0;
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t s = 0; s < frames; ++s)
        {
            while (fMqI < fMqN && fMq[fMqI].offset <= fSi)
            {
                const int st = fMq[fMqI].st & 0xF0;
                const double n = fMq[fMqI].d1, v = fMq[fMqI].d2;
                if (st == 0x90 && v > 0)
                    noteOn(n, v / 127.0);
                else if (st == 0x80 || st == 0x90)
                    noteOff(n);
                else if (st == 0xB0)
                {
                    if (n == 123)
                        releaseAll();
                    else if (n == 120)
                        killAll();
                }
                ++fMqI;
            }
            ++fSi;

            // the voices: envelope, scan, and a grain when it is due
            for (int i = 0; i < kVoices; ++i)
            {
                Voice& vb = fV[i];
                if (vb.on == 0)
                    continue;
                if (vb.st == 1)
                {
                    vb.env += fAttInc;
                    if (vb.env >= 1)
                    {
                        vb.env = 1;
                        vb.st = 2;
                    }
                }
                else if (vb.st == 3)
                {
                    vb.env *= fRelC;
                    if (vb.env < 0.0001)
                        vb.on = 0;
                }
                if (vb.on != 0)
                {
                    vb.scan += fScanInc;
                    vb.scan -= std::floor(vb.scan);
                    vb.cnt -= 1;
                    if (vb.cnt <= 0)
                    {
                        spawn(i);
                        vb.cnt += fGap;
                    }
                }
            }

            // the grains
            double outl = 0, outr = 0;
            for (Grain& g : fG)
            {
                if (g.on == 0)
                    continue;
                const Voice& vb = fV[g.voice];
                const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * g.t / g.len);
                double a = w * vb.env;
                a *= vb.gain;
                outl += rd(g.pos, 0) * g.gl * a;
                outr += rd(g.pos, 1) * g.gr * a;
                g.pos += g.rate;
                g.pos -= std::floor(g.pos / fNfr) * fNfr;
                g.t += 1;
                if (g.t >= g.len)
                    g.on = 0;
            }
            const double x = fNorm * fOg;
            outL[s] = float(outl * x);
            outR[s] = float(outr * x);
        }
    }

    // for the display
    bool fileOk() const { return fFileOk; }
    int channels() const { return fNch; }
    double fileRate() const { return fFsr; }
    double frames() const { return fNfr; }
    const std::vector<double>& builtin() const { return fBuiltin; }
    const Grain& grain(int i) const { return fG[i]; }

private:
    void derive()
    {
        fBaseR = fFsr / fSr;
        fPos0 = fS[0] / 100.0;
        fSpray = fS[1] / 200.0;
        fScanInc = fS[2] / 100.0 / fSr;
        fGlen = std::max(16.0, std::floor(fS[3] / 1000.0 * fSr));
        fGap = fSr / fS[4];
        fPspread = fS[5];
        fSspread = fS[6] / 100.0;
        fRev = fS[7] / 100.0;
        fRoot = fS[8];
        fAttInc = 1.0 / std::max(1.0, fS[9] / 1000.0 * fSr);
        fRelC = std::exp(-6.907755278982137 / std::max(1.0, fS[10] / 1000.0 * fSr));
        fVs = fS[11] / 100.0;
        fNorm = 1.0 / std::sqrt(std::max(1.0, fS[4] * fS[3] / 1000.0));
        fOg = std::pow(10.0, fS[12] / 20.0);
    }

    // make_builtin of the JSFX
    void makeBuiltin()
    {
        const double nfr = std::floor(2.0 * fSr);
        const double f0 = 261.6255653005986;
        const double fo = std::floor(0.01 * fSr);
        const double tp = 2.0 * kPi;
        fBuiltin.assign(size_t(nfr), 0.0);
        for (double i = 0; i < nfr; i += 1)
        {
            const double t = i / fSr;
            double y = 0;
            double k = 1;
            for (int h = 0; h < 8; ++h)
            {
                double x = tp * k;
                x *= f0;
                x *= t;
                double a = tp * (0.3 + 0.17 * k);
                a *= t;
                a = 0.6 + 0.4 * std::sin(a + k);
                a /= k;
                y += std::sin(x) * a;
                k += 1;
            }
            y *= 0.35;
            if (i < fo)
                y *= i / fo;
            const double r = nfr - i;
            if (r < fo)
                y *= r / fo;
            fBuiltin[size_t(i)] = y;
        }
    }

    void useBuiltin()
    {
        fBuf = fBuiltin.data();
        fNch = 1;
        fFsr = fSr;
        fNfr = double(fBuiltin.size());
    }

    // rd: the sound at position p (0..nfr, wrapping) of channel c, read linearly
    double rd(double p, int c) const
    {
        const double i = std::floor(p);
        const double f = p - i;
        double j = i + 1;
        if (j >= fNfr)
            j = 0;
        c = std::min(c, fNch - 1);
        const double a = fBuf[size_t(i) * size_t(fNch) + size_t(c)];
        const double b = fBuf[size_t(j) * size_t(fNch) + size_t(c)];
        return a + (b - a) * f;
    }

    double rnd()
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    void spawn(int vi)
    {
        const double r1 = rnd(), r2 = rnd(), r3 = rnd(), r4 = rnd();
        Grain* gb = nullptr;
        for (Grain& g : fG)
            if (g.on == 0)
            {
                gb = &g;
                break;
            }
        if (!gb)
            return;
        const Voice& vb = fV[vi];
        double p = fPos0 + vb.scan;
        p += (r1 * 2 - 1) * fSpray;
        p -= std::floor(p);
        gb->pos = p * fNfr;
        double st = vb.note - fRoot;
        st += (r2 * 2 - 1) * fPspread;
        gb->rate = fBaseR * std::pow(2.0, st / 12.0);
        if (r4 < fRev)
            gb->rate = -gb->rate;
        const double pan = (r3 * 2 - 1) * fSspread;
        gb->gl = std::min(1.0, 1 - pan);
        gb->gr = std::min(1.0, 1 + pan);
        gb->t = 0;
        gb->len = fGlen;
        gb->voice = vi;
        gb->on = 1;
    }

    void noteOn(double n, double v)
    {
        int bv = -1, bo = 0;
        double ba = -1;
        for (int i = 0; i < kVoices; ++i)
        {
            Voice& vb = fV[i];
            vb.age += 1;
            if (bv < 0 && vb.on == 0)
                bv = i;
            if (vb.age > ba)
            {
                ba = vb.age;
                bo = i;
            }
        }
        if (bv < 0)
            bv = bo;
        Voice& vb = fV[bv];
        vb.on = 1;
        vb.note = n;
        vb.gain = 1 - fVs + fVs * v;
        vb.env = 0;
        vb.st = 1;
        vb.cnt = 0;
        vb.scan = 0;
        vb.age = 0;
    }

    void noteOff(double n)
    {
        for (Voice& vb : fV)
            if (vb.on != 0 && vb.note == n && vb.st < 3)
                vb.st = 3;
    }

    void releaseAll()
    {
        for (Voice& vb : fV)
            if (vb.on != 0)
                vb.st = 3;
    }

    void killAll()
    {
        for (Voice& v : fV)
            v = Voice();
        for (Grain& g : fG)
            g = Grain();
    }

    double fSr = 48000.0;
    double fS[13] = { 30, 10, 0, 80, 30, 0, 50, 0, 60, 200, 1000, 50, 0 };
    double fBaseR = 1, fPos0 = 0.3, fSpray = 0.05, fScanInc = 0, fGlen = 3840, fGap = 1600, fPspread = 0,
           fSspread = 0.5, fRev = 0, fRoot = 60, fAttInc = 0, fRelC = 0, fVs = 0.5, fNorm = 1, fOg = 1;
    double fSeed = 12345.0;
    Voice fV[kVoices];
    Grain fG[kGrains];
    std::vector<double> fFile, fBuiltin;
    const double* fBuf = nullptr;
    bool fFileOk = false;
    int fNch = 1;
    double fFsr = 48000.0, fNfr = 0;
    MidiEv fMq[kMaxEvents];
    uint32_t fMqN = 0, fMqI = 0, fSi = 0;
};

} // namespace nullgrn
