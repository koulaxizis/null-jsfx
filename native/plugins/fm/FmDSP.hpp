/*
 * NULL JSFX - FM, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/fm.jsfx, computed in double like EEL2 and in the same
 * order: eight voices of four sine operators (Ratio, Level, ADSR, Velocity; feedback on
 * operator 4) routed by one of eight algorithms, then Chorus and Reverb. Comments name the
 * JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI messages, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullfm {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 37;
static constexpr int kMaxEvents = 256;
static constexpr int kVoices = 8;

struct MidiEv { uint32_t frame; uint8_t st, d1, d2; };

// the algorithms: mod[a][i * 4 + j] = operator j+1 modulates operator i+1; carriers
inline void routing(int alg, double* MM, double* CR)
{
    for (int k = 0; k < 16; ++k)
        MM[k] = 0;
    switch (alg)
    {
    case 0: MM[11] = 1; MM[6] = 1; MM[1] = 1; break;
    case 1: MM[6] = 1; MM[7] = 1; MM[1] = 1; break;
    case 2: MM[6] = 1; MM[1] = 1; MM[3] = 1; break;
    case 3: MM[11] = 1; MM[2] = 1; MM[1] = 1; break;
    case 4: MM[1] = 1; MM[11] = 1; break;
    case 5: MM[3] = 1; MM[7] = 1; MM[11] = 1; break;
    case 6: MM[11] = 1; break;
    default: break;
    }
    CR[0] = 1;
    CR[1] = alg >= 5;
    CR[2] = alg >= 4;
    CR[3] = alg >= 7;
}

class Fm
{
public:
    struct Voice {
        int st = 0;
        bool gate = false, sus = false;
        int note = -1;
        double ord = 0, pt = 0;
        double m[20] = {};
    };

    // @init
    void setSampleRate(double sr)
    {
        srate = sr;
        RVN = int(std::ceil(0.07 * srate)) + 2;
        chL.assign(CHN, 0.0); chR.assign(CHN, 0.0);
        rvB.assign(size_t(8 * RVN), 0.0);
        static const double ms[8] = { 31.3, 37.9, 41.1, 43.7, 47.9, 53.3, 59.1, 67.7 };
        for (int k = 0; k < 8; ++k)
            RVL[k] = std::floor(ms[k] * srate / 1000);
        reset();
    }

    void reset()
    {
        for (Voice& v : voices)
            v = Voice();
        std::fill(chL.begin(), chL.end(), 0.0); std::fill(chR.begin(), chR.end(), 0.0);
        ch_w = 0;
        std::fill(rvB.begin(), rvB.end(), 0.0);
        for (int k = 0; k < 8; ++k)
            RVS[k] = RVP[k] = 0;
        ch_on = rv_on = false;
        ord_n = 0;
        sus_on = false; mwheel = 0; bend = 0;
        vib_ph = 0; ch_ph = 0;
    }

    // @slider (s: slider1..slider37 on the JSFX grid)
    void setParams(const double* s)
    {
        alg = int(std::floor(s[0] + 0.5));
        fb_k = s[1] / 100 * 1.5;
        bright = std::pow(2.0, s[30] / 100 * (s[30] < 0 ? 2 : 1));
        routing(alg, MM, CR);
        const double ncar = CR[0] + CR[1] + CR[2] + CR[3];
        out_g = 0.25 / std::sqrt(ncar);
        for (int k = 0; k < 4; ++k)
        {
            const double* p = s + 2 + k * 7;
            double* o = OPS + k * 8;
            o[0] = p[0];
            const double lv = p[1] / 100;
            o[1] = CR[k] > 0 ? lv * lv : lv * lv * 8 * bright;
            o[2] = 1000 / (p[2] * srate);
            o[3] = std::exp(-4.6 * 1000 / (p[3] * srate));
            o[4] = p[4] / 100;
            o[5] = std::exp(-4.6 * 1000 / (p[5] * srate));
            o[6] = p[6] / 100;
        }
        ch_mix = s[31] / 100;
        rv_mix = s[32] / 100;
        const double rv_size = s[33] / 100;
        const double rv_t60 = 0.3 + 7.7 * rv_size * rv_size;
        rv_dc = 1 - std::exp(-2 * kPi * (10000 - 6000 * rv_size) / srate);
        for (int k = 0; k < 8; ++k)
            RVG[k] = std::pow(10.0, -3 * RVL[k] / (srate * rv_t60));
        oct = 12 * std::floor(s[34] + 0.5);
        bend_r = s[35];
        vol = std::pow(10.0, s[36] / 20);
        if (ch_mix > 0 && !ch_on)
        {
            std::fill(chL.begin(), chL.end(), 0.0);
            std::fill(chR.begin(), chR.end(), 0.0);
        }
        if (rv_mix > 0 && !rv_on)
        {
            std::fill(rvB.begin(), rvB.end(), 0.0);
            for (int k = 0; k < 8; ++k)
                RVS[k] = 0;
        }
        ch_on = ch_mix > 0; rv_on = rv_mix > 0;
    }

    // @block
    void beginBlock(const MidiEv* ev, uint32_t count)
    {
        mq_n = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + mq_n, mq);
        mq_i = 0;
        si = 0;
        vib_inc = 5.5 / srate;
        ch_inc = 0.45 / srate;
    }

    void process(float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            while (mq_i < mq_n && mq[mq_i].frame <= si)
            {
                midiIn(mq[mq_i].st, mq[mq_i].d1, mq[mq_i].d2);
                ++mq_i;
            }
            ++si;
            double l, r;
            tick(l, r);
            outL[i] = float(l);
            outR[i] = float(r);
        }
    }

    const Voice& voice(int i) const { return voices[i]; }

private:
    void vstart(Voice& v, int n, double vel)
    {
        double* m = v.m;
        if (v.st == 0)
            for (int k = 0; k < 20; ++k)
                m[k] = 0;
        for (int k = 0; k < 4; ++k)
        {
            const double s = OPS[k * 8 + 6];
            m[12 + k] = 1 - s + s * vel;
            m[8 + k] = 1;
        }
        v.note = n;
        v.pt = n + oct;
        v.gate = true; v.sus = false;
        v.st = 1;
        ord_n += 1; v.ord = ord_n;
    }

    static void vrelease(Voice& v)
    {
        double* m = v.m;
        if (v.st > 0)
            for (int k = 8; k < 12; ++k)
                m[k] = m[k] > 0 ? 4 : 0;
        v.gate = false;
        v.sus = false;
    }

    void voff(Voice& v, int n)
    {
        if (v.gate && v.note == n)
        {
            if (sus_on)
            {
                v.gate = false;
                v.sus = true;
            }
            else
                vrelease(v);
        }
    }

    double opEnv(double* m, int k)
    {
        const double* o = OPS + k * 8;
        double st = m[8 + k], e = m[4 + k];
        if (st == 1)
        {
            e += o[2];
            if (e >= 1)
            {
                e = 1;
                st = 2;
            }
        }
        else if (st == 2)
        {
            e = o[4] + (e - o[4]) * o[3];
            if (o[4] <= 0 && e < 0.00001)
            {
                e = 0;
                st = 0;
            }
        }
        else if (st == 4)
        {
            e *= o[5];
            if (e < 0.00001)
            {
                e = 0;
                st = 0;
            }
        }
        m[8 + k] = st;
        m[4 + k] = e;
        return e * m[12 + k];
    }

    void vproc(Voice& v)
    {
        if (v.st <= 0)
            return;
        double* m = v.m;
        const double e4 = opEnv(m, 3), e3 = opEnv(m, 2), e2 = opEnv(m, 1), e1 = opEnv(m, 0);
        const double f = 440 * std::pow(2.0, (v.pt + pmod - 69) / 12) / srate;
        const double y4 = std::sin(2 * kPi * m[3] + fb_k * (m[16] + m[17])) * e4;
        m[17] = m[16];
        m[16] = y4 * 0.5;
        const double y3 = std::sin(2 * kPi * m[2] + MM[11] * y4 * OPS[25]) * e3;
        const double y2 = std::sin(2 * kPi * m[1] + MM[6] * y3 * OPS[17] + MM[7] * y4 * OPS[25]) * e2;
        const double y1 = std::sin(2 * kPi * m[0] + MM[1] * y2 * OPS[9] + MM[2] * y3 * OPS[17] + MM[3] * y4 * OPS[25]) * e1;
        m[0] += f * OPS[0]; m[0] -= std::floor(m[0]);
        m[1] += f * OPS[8]; m[1] -= std::floor(m[1]);
        m[2] += f * OPS[16]; m[2] -= std::floor(m[2]);
        m[3] += f * OPS[24]; m[3] -= std::floor(m[3]);
        outv += (CR[0] * y1 * OPS[1] + CR[1] * y2 * OPS[9] + CR[2] * y3 * OPS[17] + CR[3] * y4 * OPS[25]) * out_g;
        const bool on = (CR[0] > 0 && m[8] > 0) || (CR[1] > 0 && m[9] > 0) || (CR[2] > 0 && m[10] > 0) || (CR[3] > 0 && m[11] > 0);
        if (!on)
            v.st = 0;
    }

    void noteOn(int n, double vel)
    {
        int best = -1;
        for (int i = 0; i < kVoices; ++i)
            if (best < 0 && voices[i].st > 0 && voices[i].note == n)
                best = i;
        for (int i = 0; i < kVoices; ++i)
            if (best < 0 && voices[i].st == 0)
                best = i;
        if (best < 0)
        {
            double bo = -1;
            for (int i = 0; i < kVoices; ++i)
                if (voices[i].st > 0 && !voices[i].gate && (bo < 0 || voices[i].ord < bo))
                {
                    bo = voices[i].ord;
                    best = i;
                }
        }
        if (best < 0)
        {
            best = 0;
            double bo = voices[0].ord;
            for (int i = 1; i < kVoices; ++i)
                if (voices[i].ord < bo)
                {
                    bo = voices[i].ord;
                    best = i;
                }
        }
        vstart(voices[best], n, vel);
    }

    void midiIn(int st, int d1, int d2)
    {
        const int c = st & 0xF0;
        if (c == 0x90 && d2 > 0)
            noteOn(d1, d2 / 127.0);
        else if (c == 0x80 || c == 0x90)
        {
            for (Voice& v : voices)
                voff(v, d1);
        }
        else if (c == 0xB0)
        {
            if (d1 == 64)
            {
                sus_on = d2 >= 64;
                if (!sus_on)
                    for (Voice& v : voices)
                        if (v.sus)
                            vrelease(v);
            }
            else if (d1 == 1)
                mwheel = d2 / 127.0;
            else if (d1 == 120 || d1 == 123)
            {
                sus_on = false;
                for (Voice& v : voices)
                    vrelease(v);
            }
        }
        else if (c == 0xE0)
            bend = (d2 * 128 + d1 - 8192) / 8192.0;
    }

    void tick(double& l, double& r)
    {
        vib_ph += vib_inc;
        vib_ph -= std::floor(vib_ph);
        pmod = bend * bend_r + 0.5 * mwheel * std::sin(2 * kPi * vib_ph);

        outv = 0;
        for (Voice& v : voices)
            vproc(v);
        double outl = outv, outr = outv;

        if (ch_on)
        {
            ch_ph += ch_inc;
            ch_ph -= std::floor(ch_ph);
            chL[size_t(ch_w)] = outl;
            chR[size_t(ch_w)] = outr;
            double d = (0.012 + 0.003 * std::sin(2 * kPi * ch_ph)) * srate;
            double rp = ch_w - d;
            if (rp < 0)
                rp += CHN;
            double i0 = std::floor(rp), fr = rp - i0, i1 = i0 + 1;
            if (i1 >= CHN)
                i1 -= CHN;
            const double yl = chL[size_t(i0)] + (chL[size_t(i1)] - chL[size_t(i0)]) * fr;
            d = (0.012 + 0.003 * std::cos(2 * kPi * ch_ph)) * srate;
            rp = ch_w - d;
            if (rp < 0)
                rp += CHN;
            i0 = std::floor(rp); fr = rp - i0; i1 = i0 + 1;
            if (i1 >= CHN)
                i1 -= CHN;
            const double yr = chR[size_t(i0)] + (chR[size_t(i1)] - chR[size_t(i0)]) * fr;
            ch_w += 1;
            if (ch_w >= CHN)
                ch_w = 0;
            outl = outl * (1 - 0.5 * ch_mix) + yr * 0.5 * ch_mix;
            outr = outr * (1 - 0.5 * ch_mix) + yl * 0.5 * ch_mix;
        }

        if (rv_on)
        {
            double T[8], Y[8];
            for (int k = 0; k < 8; ++k)
            {
                const double y = rvB[size_t(k * RVN + RVP[k])];
                RVS[k] += (y - RVS[k]) * rv_dc;
                T[k] = RVS[k] * RVG[k];
                Y[k] = T[k];
            }
            auto bf = [&T](int a, int b) {
                const double x = T[a] + T[b], y = T[a] - T[b];
                T[a] = x;
                T[b] = y;
            };
            bf(0, 1); bf(2, 3); bf(4, 5); bf(6, 7);
            bf(0, 2); bf(1, 3); bf(4, 6); bf(5, 7);
            bf(0, 4); bf(1, 5); bf(2, 6); bf(3, 7);
            for (int k = 0; k < 8; ++k)
            {
                rvB[size_t(k * RVN + RVP[k])] = T[k] * 0.35355339059327373 + ((k & 1) ? outr : outl) * 0.3;
                RVP[k] += 1;
                if (RVP[k] >= RVL[k])
                    RVP[k] = 0;
            }
            outl += (Y[0] + Y[2] + Y[4] + Y[6]) * 0.5 * rv_mix;
            outr += (Y[1] + Y[3] + Y[5] + Y[7]) * 0.5 * rv_mix;
        }

        l = outl * vol;
        r = outr * vol;
    }

    double srate = 44100;
    Voice voices[kVoices];

    int alg = 0;
    double fb_k = 0, bright = 1, out_g = 0.25, OPS[32] = {}, MM[16] = {}, CR[4] = {};
    double ch_mix = 0, rv_mix = 0, rv_dc = 0, RVG[8] = {}, oct = 0, bend_r = 2, vol = 1;

    MidiEv mq[kMaxEvents];
    uint32_t mq_n = 0, mq_i = 0, si = 0;
    double ord_n = 0;
    bool sus_on = false;
    double mwheel = 0, bend = 0, pmod = 0, vib_ph = 0, vib_inc = 0, outv = 0;

    static constexpr int CHN = 16384;
    int RVN = 0;
    std::vector<double> chL, chR, rvB;
    double ch_ph = 0, ch_inc = 0, ch_w = 0;
    double RVL[8] = {}, RVS[8] = {}, RVP[8] = {};
    bool ch_on = false, rv_on = false;
};

} // namespace nullfm
