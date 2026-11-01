/*
 * NULL JSFX - Synth, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/synth.jsfx, computed in double like EEL2 and in the same
 * order: eight voices (Analog or Wavetable engine, filter, two envelopes), the LFO, Glide,
 * Poly / Mono / Legato, Unison, Character, then Chorus, Delay and Reverb, and MPE (per-note
 * bend, pressure and CC74 timbre on channels 2-16). Comments name the JSFX variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI messages, and process.
 */

#pragma once

#include "SynthTables.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullsynth {

static constexpr int kNumSliders = 47;
static constexpr int kMaxEvents = 256;
static constexpr int kVoices = 8;

struct MidiEv { uint32_t frame; uint8_t st, d1, d2; };

inline double blep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1;
    }
    if (t > 1 - dt)
    {
        t = (t - 1) / dt;
        return t * t + t + t + 1;
    }
    return 0;
}

inline double osc(int w, double p, double dt)
{
    if (w == 0)
        return 2 * p - 1 - blep(p, dt);
    if (w == 1)
    {
        double q = p + 0.5;
        q -= std::floor(q);
        return (p < 0.5 ? 1 : -1) + blep(p, dt) - blep(q, dt);
    }
    if (w == 2)
        return p < 0.5 ? 4 * p - 1 : 3 - 4 * p;
    return std::sin(2 * kPi * p);
}

inline double sat(double x)
{
    const double u = std::max(-3.0, std::min(3.0, x));
    return u * (27 + u * u) / (27 + 9 * u * u);
}

inline double svfTick(double x, double* s, double a1, double a2, double a3, double k, int mode)
{
    const double v3 = x - s[1];
    const double v1 = a1 * s[0] + a2 * v3;
    const double v2 = s[1] + a2 * s[0] + a3 * v3;
    s[0] = 2 * v1 - s[0];
    s[1] = 2 * v2 - s[1];
    return mode == 0 ? v2 : mode == 1 ? v1 * k : x - k * v1 - v2;
}

class Synth
{
public:
    struct Voice {
        int st = 0, fs = 0;
        double ae = 0, fe = 0;
        bool gate = false, sus = false;
        int note = -1, ch = 0;
        double ord = 0, pt = 0, pc = 0, vg = 0, pp = 0, tt = 0;
        double m[20] = {};
    };

    Synth() : fWt(wavetables()) {}

    // @init
    void setSampleRate(double sr)
    {
        srate = sr;
        DLN = int(std::ceil(3 * srate)) + 2;
        RVN = int(std::ceil(0.07 * srate)) + 2;
        chL.assign(CHN, 0.0); chR.assign(CHN, 0.0);
        dlL.assign(size_t(DLN), 0.0); dlR.assign(size_t(DLN), 0.0);
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
        std::fill(std::begin(keys), std::end(keys), 0.0);
        std::fill(chL.begin(), chL.end(), 0.0); std::fill(chR.begin(), chR.end(), 0.0);
        ch_w = 0;
        std::fill(dlL.begin(), dlL.end(), 0.0); std::fill(dlR.begin(), dlR.end(), 0.0);
        dl_w = 0; dl_ll = dl_lr = 0;
        std::fill(rvB.begin(), rvB.end(), 0.0);
        for (int k = 0; k < 8; ++k)
            RVS[k] = RVP[k] = 0;
        ch_on = dl_on = rv_on = false;
        ord_n = 0; key_n = 0;
        last_p = -1;
        sus_on = false; mwheel = 0; bend = 0;
        std::fill(std::begin(chb), std::end(chb), 0.0);
        std::fill(std::begin(chp), std::end(chp), 0.0);
        std::fill(std::begin(cht), std::end(cht), 0.0);
        std::fill(std::begin(kch), std::end(kch), 0);
        cur_ch = 0; mpe_was = false;
        lfo_ph = 0; lfo_v = 0; lfo_s = 0; lfo_r = 0; lfo_x = 4321;
        ch_ph = 0;
        seed = 22222;
    }

    // @slider (v: slider1..slider47 on the JSFX grid)
    void setParams(const double* s)
    {
        engine = s[0] >= 0.5;
        w1 = int(std::floor(s[1] + 0.5));
        w2 = int(std::floor(s[2] + 0.5));
        char_a = s[34] / 100;
        char_g = 1 + 4 * char_a;
        o2r = std::pow(2.0, (s[3] + (s[4] + 8 * char_a) / 100) / 12);
        m2 = s[5] / 100; m1 = 1 - m2;
        sub_a = s[6] / 100 * 0.8;
        noise_a = s[7] / 100 * 0.5;
        tbl = int(std::floor(s[8] + 0.5));
        wt_pos = s[9] / 100;
        fmode = int(std::floor(s[10] + 0.5));
        slope = s[11] >= 0.5;
        cutoff = s[12];
        const double res = s[13] / 100;
        q_main = slope ? 1.31 + 10.7 * res * res : 0.707 + 11.3 * res * res;
        fenv_o = s[14] / 100 * 6;
        kt = s[15] / 100;
        f_inc = 1000 / (s[16] * srate);
        f_dc = std::exp(-4.6 * 1000 / (s[17] * srate));
        f_sus = s[18] / 100;
        f_rc = std::exp(-4.6 * 1000 / (s[19] * srate));
        a_inc = 1000 / (s[20] * srate);
        a_dc = std::exp(-4.6 * 1000 / (s[21] * srate));
        a_sus = s[22] / 100;
        a_rc = std::exp(-4.6 * 1000 / (s[23] * srate));
        lfo_shape = int(std::floor(s[24] + 0.5));
        lfo_rate = s[25];
        lfo_sync = int(std::floor(s[26] + 0.5));
        lfo_bpc = lfo_sync == 1 ? 4 : lfo_sync == 2 ? 2 : lfo_sync == 3 ? 1 : lfo_sync == 4 ? 0.5 :
                  lfo_sync == 5 ? 0.25 : lfo_sync == 6 ? 2.0 / 3 : lfo_sync == 7 ? 1.0 / 3 : lfo_sync == 8 ? 1.5 : 0.75;
        lfo_p = s[27];
        lfo_f = s[28];
        lfo_w = s[29] / 100;
        glide_on = s[30] > 0;
        gl_c = glide_on ? 1 - std::exp(-2.3 * 1000 / (s[30] * srate)) : 1;
        vmode = int(std::floor(s[31] + 0.5));
        nuni = std::max(1, std::min(4, int(std::floor(s[32] + 0.5))));
        const double spread = s[33] / 100;
        ug = 1 / std::sqrt(double(nuni));
        for (int k = 0; k < 4; ++k)
        {
            const double u = nuni > 1 ? 2.0 * k / (nuni - 1) - 1 : 0;
            UD[k] = std::pow(2.0, (25 * spread + 10 * char_a) * u / 1200);
            UPL[k] = std::sqrt(2.0) * std::cos((spread * u + 1) * kPi / 4);
            UPR[k] = std::sqrt(2.0) * std::sin((spread * u + 1) * kPi / 4);
        }
        ch_mix = s[35] / 100;
        dl_mix = s[36] / 100;
        const int dl_sel = int(std::floor(s[37] + 0.5));
        dl_beats = dl_sel == 0 ? 0.25 : dl_sel == 1 ? 1.0 / 3 : dl_sel == 2 ? 0.5 : dl_sel == 3 ? 0.75 :
                   dl_sel == 4 ? 2.0 / 3 : dl_sel == 5 ? 1 : dl_sel == 6 ? 1.5 : 2;
        dl_fb = s[38] / 100;
        rv_mix = s[39] / 100;
        const double rv_size = s[40] / 100;
        const double rv_t60 = 0.3 + 7.7 * rv_size * rv_size;
        rv_dc = 1 - std::exp(-2 * kPi * (10000 - 6000 * rv_size) / srate);
        for (int k = 0; k < 8; ++k)
            RVG[k] = std::pow(10.0, -3 * RVL[k] / (srate * rv_t60));
        vs = s[41] / 100;
        bend_r = s[42];
        oct = 12 * std::floor(s[43] + 0.5);
        vol = std::pow(10.0, s[44] / 20);
        mpe = s[45] >= 0.5;
        mpe_r = s[46];
        mpe_sm = 1 - std::exp(-1000 / (5 * srate));
        // MPE turned on starts from neutral channels
        if (mpe && !mpe_was)
        {
            std::fill(std::begin(chb), std::end(chb), 0.0);
            std::fill(std::begin(chp), std::end(chp), 0.0);
            std::fill(std::begin(cht), std::end(cht), 0.0);
        }
        mpe_was = mpe;
        if (ch_mix > 0 && !ch_on)
        {
            std::fill(chL.begin(), chL.end(), 0.0);
            std::fill(chR.begin(), chR.end(), 0.0);
        }
        if (dl_mix > 0 && !dl_on)
        {
            std::fill(dlL.begin(), dlL.end(), 0.0);
            std::fill(dlR.begin(), dlR.end(), 0.0);
            dl_ll = dl_lr = 0;
        }
        if (rv_mix > 0 && !rv_on)
        {
            std::fill(rvB.begin(), rvB.end(), 0.0);
            for (int k = 0; k < 8; ++k)
                RVS[k] = 0;
        }
        ch_on = ch_mix > 0; dl_on = dl_mix > 0; rv_on = rv_mix > 0;
    }

    // @block
    void beginBlock(const MidiEv* ev, uint32_t count, bool playing, double tempo, double beatPos)
    {
        mq_n = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + mq_n, mq);
        mq_i = 0;
        si = 0;
        const double bpm = tempo > 0 ? tempo : 120;
        if (lfo_sync > 0)
        {
            lfo_inc = bpm / (lfo_bpc * srate * 60);
            if (playing)
            {
                lfo_ph = beatPos / lfo_bpc + 0.000000001;
                lfo_ph -= std::floor(lfo_ph);
            }
        }
        else
            lfo_inc = lfo_rate / srate;
        lfo_sm = 1 - std::exp(-1000 / (2 * srate));
        dl_len = std::max(1.0, std::min(double(DLN - 2), std::floor(dl_beats * 60 / bpm * srate)));
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
    double lfoValue() const { return lfo_v; }

private:
    // ---- voices -----------------------------------------------------------------------------

    void vstart(Voice& v, int n, double vel, double glide_from)
    {
        double* m = v.m;
        if (v.st == 0)
        {
            for (int k = 0; k < 4; ++k)
            {
                m[k] = k * 0.23;
                m[4 + k] = k * 0.31 + 0.11;
            }
            m[8] = 0;
            for (int k = 10; k < 18; ++k)
                m[k] = 0;
            v.ae = 0; v.fe = 0;
        }
        v.note = n;
        v.ch = mpe ? kch[n] : 0;
        v.pp = chp[v.ch];
        v.tt = cht[v.ch];
        v.pt = n + oct;
        v.pc = glide_from >= 0 && glide_on ? glide_from : v.pt;
        v.vg = 1 - vs + vs * vel;
        v.gate = true; v.sus = false;
        v.st = 1; v.fs = 1;
        ord_n += 1; v.ord = ord_n;
    }

    void vmove(Voice& v, int n, bool glide)
    {
        v.note = n;
        v.ch = mpe ? kch[n] : 0;
        v.pt = n + oct;
        if (!glide || !glide_on)
            v.pc = v.pt;
    }

    static void vrelease(Voice& v)
    {
        if (v.st > 0)
        {
            v.st = 4;
            v.fs = 4;
        }
        v.gate = false;
        v.sus = false;
    }

    void voff(Voice& v, int n)
    {
        if (v.gate && v.note == n && (!mpe || v.ch == cur_ch))
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

    double wtRead(double p, double lv) const
    {
        const double x = p * 1024;
        const double i0 = std::floor(x);
        const double fi = x - i0;
        const int a0 = int(i0), a1 = (a0 + 1) & 1023;
        const double* b = fWt.data() + wt_b + size_t(lv) * 1024;
        const double a = b[a0] + (b[a1] - b[a0]) * fi;
        b += 8192;
        const double c = b[a0] + (b[a1] - b[a0]) * fi;
        return a + (c - a) * wt_f;
    }

    double wtLevel(double f) const
    {
        const double r = f * 512 / (0.45 * srate);
        return r <= 1 ? 0 : std::min(7.0, std::ceil(std::log(r) * 1.4426950408889634));
    }

    void vproc(Voice& v, double& outl, double& outr)
    {
        if (v.st <= 0)
            return;
        double* m = v.m;
        if (v.st == 1)
        {
            v.ae += a_inc;
            if (v.ae >= 1)
            {
                v.ae = 1;
                v.st = 2;
            }
        }
        else if (v.st == 2)
        {
            v.ae = a_sus + (v.ae - a_sus) * a_dc;
            if (a_sus <= 0 && v.ae < 0.00001)
            {
                v.ae = 0;
                v.st = 0;
            }
        }
        else
        {
            v.ae *= a_rc;
            if (v.ae < 0.00001)
            {
                v.ae = 0;
                v.st = 0;
            }
        }
        if (v.fs == 1)
        {
            v.fe += f_inc;
            if (v.fe >= 1)
            {
                v.fe = 1;
                v.fs = 2;
            }
        }
        else if (v.fs == 2)
            v.fe = f_sus + (v.fe - f_sus) * f_dc;
        else
            v.fe *= f_rc;

        v.pc += (v.pt - v.pc) * gl_c;
        // MPE: the note's own channel bend, pressure and timbre (pressure and timbre over 5 ms)
        double f1;
        if (mpe)
        {
            v.pp += (chp[v.ch] - v.pp) * mpe_sm;
            v.tt += (cht[v.ch] - v.tt) * mpe_sm;
            f1 = 440 * std::pow(2.0, ((v.pc + (pmod + mpe_r * chb[v.ch])) - 69) / 12);
        }
        else
            f1 = 440 * std::pow(2.0, (v.pc + pmod - 69) / 12);
        const double f2 = f1 * o2r;
        double l1 = 0, l2 = 0;
        if (engine)
        {
            l1 = wtLevel(f1);
            l2 = wtLevel(f2);
        }

        double sl = 0, sr = 0;
        for (int k = 0; k < nuni; ++k)
        {
            const double d = UD[k];
            const double dt1 = std::min(0.45, f1 * d / srate);
            const double dt2 = std::min(0.45, f2 * d / srate);
            double p = m[k], q = m[4 + k];
            const double s = engine ? m1 * wtRead(p, l1) + m2 * wtRead(q, l2) : m1 * osc(w1, p, dt1) + m2 * osc(w2, q, dt2);
            p += dt1; m[k] = p - std::floor(p);
            q += dt2; m[4 + k] = q - std::floor(q);
            sl += s * UPL[k]; sr += s * UPR[k];
        }
        const double x = sub_a * std::sin(2 * kPi * m[8]) + noise_a * nz;
        m[8] += std::min(0.45, f1 * 0.5 / srate);
        m[8] -= std::floor(m[8]);
        sl = sl * ug + x;
        sr = sr * ug + x;

        if (char_a > 0)
        {
            sl += char_a * (sat(sl * char_g) - sl);
            sr += char_a * (sat(sr * char_g) - sr);
        }
        double fc = cutoff * std::pow(2.0, fenv_o * v.fe * v.vg + kt * (v.pt - 60) / 12 + lfo_f * lfo_v + char_a);
        if (mpe)
            fc *= std::pow(2.0, 2 * v.tt);
        fc = std::max(20.0, std::min(0.45 * srate, fc));
        const double g = std::tan(kPi * fc / srate);
        const double kk = 1 / q_main;
        const double a1 = 1 / (1 + g * (g + kk)), a2 = g * a1, a3 = g * a2;
        if (slope)
        {
            const double a1b = 1 / (1 + g * (g + 1 / 0.54)), a2b = g * a1b, a3b = g * a2b;
            sl = svfTick(sl, m + 10, a1b, a2b, a3b, 1 / 0.54, fmode);
            sl = svfTick(sl, m + 12, a1, a2, a3, kk, fmode);
            if (nuni > 1)
            {
                sr = svfTick(sr, m + 14, a1b, a2b, a3b, 1 / 0.54, fmode);
                sr = svfTick(sr, m + 16, a1, a2, a3, kk, fmode);
            }
            else
                sr = sl;
        }
        else
        {
            sl = svfTick(sl, m + 10, a1, a2, a3, kk, fmode);
            sr = nuni > 1 ? svfTick(sr, m + 14, a1, a2, a3, kk, fmode) : sl;
        }
        double e = v.ae * v.vg * 0.25;
        if (mpe)
            e *= 1 + v.pp;
        outl += sl * e;
        outr += sr * e;
    }

    // ---- MIDI -------------------------------------------------------------------------------

    void noteOn(int n, double vel)
    {
        key_n += 1;
        keys[n] = key_n;
        kch[n] = cur_ch;
        if (vmode == 0)
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
                    if (voices[i].st == 4 && (bo < 0 || voices[i].ord < bo))
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
            vstart(voices[best], n, vel, last_p);
        }
        else
        {
            Voice& v = voices[0];
            if (vmode == 2 && v.gate && v.st > 0)
                vmove(v, n, true);
            else
                vstart(v, n, vel, v.st > 0 ? v.pc : last_p);
        }
        last_p = n + oct;
    }

    void noteOff(int n)
    {
        keys[n] = 0;
        if (vmode == 0)
        {
            for (Voice& v : voices)
                voff(v, n);
        }
        else if (voices[0].gate && voices[0].note == n && (!mpe || voices[0].ch == cur_ch))
        {
            int best = -1;
            double bo = 0;
            for (int i = 0; i < 128; ++i)
                if (keys[i] > bo)
                {
                    bo = keys[i];
                    best = i;
                }
            if (best >= 0)
                vmove(voices[0], best, true);
            else
                voff(voices[0], n);
        }
    }

    void allOff()
    {
        std::fill(std::begin(keys), std::end(keys), 0.0);
        sus_on = false;
        for (Voice& v : voices)
            vrelease(v);
    }

    void midiIn(int st, int d1, int d2)
    {
        const int c = st & 0xF0;
        cur_ch = st & 0x0F;
        if (mpe && cur_ch > 0 && (c == 0xE0 || c == 0xD0 || (c == 0xB0 && d1 == 74)))
        {
            // MPE: a member channel's bend, pressure and timbre belong to its note
            if (c == 0xE0)
                chb[cur_ch] = (d2 * 128 + d1 - 8192) / 8192.0;
            else if (c == 0xD0)
                chp[cur_ch] = d1 / 127.0;
            else
                cht[cur_ch] = (d2 - 64) / 64.0;
        }
        else if (c == 0x90 && d2 > 0)
            noteOn(d1, d2 / 127.0);
        else if (c == 0x80 || c == 0x90)
            noteOff(d1);
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
                allOff();
        }
        else if (c == 0xE0)
            bend = (d2 * 128 + d1 - 8192) / 8192.0;
    }

    // ---- one sample ---------------------------------------------------------------------------

    void tick(double& l, double& r)
    {
        seed *= 16807;
        seed -= std::floor(seed / 2147483647) * 2147483647;
        nz = seed / 1073741824 - 1;

        lfo_ph += lfo_inc;
        if (lfo_ph >= 1)
        {
            lfo_ph -= std::floor(lfo_ph);
            lfo_x *= 16807;
            lfo_x -= std::floor(lfo_x / 2147483647) * 2147483647;
            lfo_r = lfo_x / 1073741824 - 1;
        }
        const double raw = lfo_shape == 0 ? std::sin(2 * kPi * lfo_ph) : lfo_shape == 1 ? 1 - 4 * std::fabs(lfo_ph - 0.5) :
                           lfo_shape == 2 ? (lfo_ph < 0.5 ? 1 : -1) : lfo_shape == 3 ? 1 - 2 * lfo_ph : lfo_r;
        lfo_s += (raw - lfo_s) * lfo_sm;
        lfo_v = lfo_s;
        pmod = bend * bend_r + lfo_v * (lfo_p + 0.5 * mwheel);

        if (engine)
        {
            const double x = std::max(0.0, std::min(1.0, wt_pos + lfo_w * lfo_v)) * 7;
            const double i = std::min(6.0, std::floor(x));
            wt_f = x - i;
            wt_b = size_t(tbl * 8 + int(i)) * 8192;
        }

        double outl = 0, outr = 0;
        for (Voice& v : voices)
            vproc(v, outl, outr);

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

        if (dl_on)
        {
            double rp = dl_w - dl_len;
            if (rp < 0)
                rp += DLN;
            const double yl = dlL[size_t(rp)], yr = dlR[size_t(rp)];
            dl_ll += (yl - dl_ll) * 0.35;
            dl_lr += (yr - dl_lr) * 0.35;
            dlL[size_t(dl_w)] = (outl + outr) * 0.5 + dl_lr * dl_fb;
            dlR[size_t(dl_w)] = dl_ll * dl_fb;
            dl_w += 1;
            if (dl_w >= DLN)
                dl_w = 0;
            outl += yl * dl_mix;
            outr += yr * dl_mix;
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

    const std::vector<double>& fWt;
    double srate = 44100;
    Voice voices[kVoices];
    double keys[128] = {};

    // parameters
    bool engine = false, slope = true, glide_on = false;
    int w1 = 0, w2 = 0, tbl = 0, fmode = 0, lfo_shape = 0, lfo_sync = 0, vmode = 0, nuni = 1;
    double char_a = 0, char_g = 1, o2r = 1, m1 = 0.5, m2 = 0.5, sub_a = 0, noise_a = 0, wt_pos = 0;
    double cutoff = 2500, q_main = 1, fenv_o = 0, kt = 0;
    double f_inc = 0, f_dc = 0, f_sus = 0, f_rc = 0, a_inc = 0, a_dc = 0, a_sus = 0, a_rc = 0;
    double lfo_rate = 5, lfo_bpc = 1, lfo_p = 0, lfo_f = 0, lfo_w = 0, gl_c = 1, ug = 1;
    double UD[4] = {}, UPL[4] = {}, UPR[4] = {};
    double ch_mix = 0, dl_mix = 0, dl_beats = 0.75, dl_fb = 0, rv_mix = 0, rv_dc = 0, RVG[8] = {};
    double vs = 0, bend_r = 2, oct = 0, vol = 1;
    bool mpe = false, mpe_was = false;
    double mpe_r = 48, mpe_sm = 0;

    // state
    MidiEv mq[kMaxEvents];
    uint32_t mq_n = 0, mq_i = 0, si = 0;
    double ord_n = 0, key_n = 0, last_p = -1;
    bool sus_on = false;
    double mwheel = 0, bend = 0, pmod = 0;
    double chb[16] = {}, chp[16] = {}, cht[16] = {}; // MPE: CHB, CHP, CHT
    int kch[128] = {}, cur_ch = 0;                   // KCH, cur_ch
    double lfo_ph = 0, lfo_v = 0, lfo_s = 0, lfo_r = 0, lfo_x = 4321, lfo_inc = 0, lfo_sm = 0;
    double seed = 22222, nz = 0;
    double wt_f = 0;
    size_t wt_b = 0;

    // effects
    static constexpr int CHN = 16384;
    int DLN = 0, RVN = 0;
    std::vector<double> chL, chR, dlL, dlR, rvB;
    double ch_ph = 0, ch_inc = 0, ch_w = 0;
    double dl_w = 0, dl_len = 1, dl_ll = 0, dl_lr = 0;
    double RVL[8] = {}, RVS[8] = {}, RVP[8] = {};
    bool ch_on = false, dl_on = false, rv_on = false;
};

} // namespace nullsynth
