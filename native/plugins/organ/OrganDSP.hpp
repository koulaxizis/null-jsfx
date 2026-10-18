/*
 * NULL JSFX - Organ, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A port of DATA/Effects/null_jsfx/organ.jsfx, computed in double like EEL2 and in the same
 * order: 16 tonewheel voices (nine drawbars from one sine and cosine per key), percussion, key
 * click, the vibrato scanner, the preamp and the rotary speaker. Comments name the JSFX
 * variables.
 *
 * Call setSampleRate (the JSFX @init), setParams (@slider), then per block beginBlock (@block)
 * with the block's MIDI, and process.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullorgan {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi
static constexpr int kNumSliders = 20;
static constexpr int kMaxEvents = 512;
static constexpr int NV = 16;

struct MidiEv {
    uint32_t offset;
    double st, d1, d2;
};

class Organ {
public:
    void setSampleRate(double sr)
    {
        fSr = sr;
        reset();
    }

    // what @init sets
    void reset()
    {
        for (int v = 0; v < NV; ++v)
        {
            VN[v] = -1; VG[v] = 0; VE[v] = 0; VPEND[v] = -1;
            VPH[v] = VDT[v] = VPE[v] = VPK[v] = VCE[v] = VAGE[v] = VKP[v] = VPP[v] = 0;
            VKM[v] = 0;
        }
        std::fill(KE, KE + NV * 9, 0.0);
        std::fill(H, H + 17, 0.0);
        std::fill(KD, KD + 128, 0.0);
        kdN = 0; age = 0;
        ex = exT = 1; wheel = false; percK = 6; percOn = false;
        vbSize = int(std::ceil(fSr * 0.004)) + 4;
        VB.assign(size_t(vbSize), 0.0);
        vbW = 0; lfoPh = 0;
        rbSize = int(std::ceil(fSr * 0.02));
        for (auto* b : { &hl, &hr, &dl, &dr })
            b->assign(size_t(rbSize), 0.0);
        rbW = 0;
        hornPh = 0; drumPh = 0.25;
        hornF = 0.8; drumF = 0.7;
        lp1 = lp2 = 0;
        seed = 22222;
        cS1 = cS2 = 0;
    }

    // @slider, from the 20 slider values in order
    void setParams(const double* v)
    {
        const double* s = v - 1; // s[n] = slider n
        const double sr = fSr;
        for (int d = 0; d < 9; ++d)
        {
            const double b = std::floor(s[d + 1] + 0.5);
            DG[d] = b > 0 ? std::pow(2.0, (b - 8) / 2) : 0;
        }
        percOn = s[10] >= 0.5;
        percK = s[11] >= 0.5 ? 6 : 4;
        if (percOn)
            DG[8] = 0;
        pC = std::exp(-1 / ((s[12] >= 0.5 ? 0.36 : 0.11) * sr));
        percG = s[13] >= 0.5 ? 0.6 : 1.2;
        click = s[14] / 100;
        cC = std::exp(-1 / (0.002 * sr));
        vib = std::floor(s[15] + 0.5);
        vibD = vib == 1 || vib == 4 ? 0.0003 : vib == 2 || vib == 5 ? 0.0006 : 0.001;
        lfoDt = 6.9 / sr;
        drive = 1 + s[16] / 100 * 6;
        drvN = 1 / std::sqrt(drive);
        rotMode = std::floor(s[17] + 0.5);
        rotSet();
        acc = s[18] / 100;
        hornC = std::exp(-1 / (0.7 * acc * sr));
        drumC = std::exp(-1 / (3.0 * acc * sr));
        bal = s[19] / 100;
        hornG = std::min(1.0, 2 * bal);
        drumG = std::min(1.0, 2 * (1 - bal));
        vol = std::pow(10.0, s[20] / 20);
        aInc = 1 / (0.0015 * sr);
        rInc = 1 / (0.006 * sr);
        sInc = 1 / (0.0015 * sr);
        exC = 1 - std::exp(-1 / (0.01 * sr));
        xc = std::exp(-2 * kPi * 800 / sr);
        const double cg = std::tan(kPi * 2500 / sr), ck = 1 / 0.8;
        cA1 = 1 / (1 + cg * (cg + ck));
        cA2 = cg * cA1;
        cA3 = cg * cA2;
        for (int k = 0; k < NV; ++k)
            if (VN[k] >= 0)
                vkeys(k);
    }

    void beginBlock(const MidiEv* ev, uint32_t count)
    {
        mqN = std::min<uint32_t>(count, kMaxEvents);
        std::copy(ev, ev + mqN, mq);
        mqI = 0;
        si = 0;
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

    // for the display
    double hornPh = 0, drumPh = 0.25, hornF = 0.8, drumF = 0.7, ex = 1, rotEff = 1;
    int voices() const
    {
        int n = 0;
        for (int v = 0; v < NV; ++v)
            n += VN[v] >= 0;
        return n;
    }
    double VN[NV] = {}, VG[NV] = {};

private:
    double keff(double f0, double k) const
    {
        while (f0 * k < 32 && k > 0 && k <= 8)
            k *= 2;
        while (f0 * k > 5900 && k - std::floor(k / 2) * 2 == 0)
            k /= 2;
        return f0 * k > 0.45 * fSr ? 0 : k;
    }

    void vkeys(int v)
    {
        const double f0 = 440 * std::pow(2.0, (VN[v] - 81) / 12);
        double km = 1;
        for (int d = 0; d < 9; ++d)
        {
            const double k = keff(f0, kHK[d]);
            KE[v * 9 + d] = k;
            if (DG[d] > 0 && k > km)
                km = k;
        }
        const double k = keff(f0, percK);
        VKP[v] = k;
        if (percOn && VPK[v] > 0 && k > km)
            km = k;
        VKM[v] = km;
    }

    void vStart(int v, double n, double perc)
    {
        VN[v] = n; VG[v] = 1; VPEND[v] = -1;
        VE[v] = 0; VPH[v] = 0;
        VDT[v] = 440 * std::pow(2.0, (n - 81) / 12) / fSr;
        VPK[v] = perc; VPE[v] = 1;
        VCE[v] = 1;
        age += 1;
        VAGE[v] = age;
        vkeys(v);
    }

    void noteOn(double n)
    {
        const int ni = int(n);
        const double perc = kdN == 0 ? 1 : 0;
        if (KD[ni] == 0)
            kdN += 1;
        KD[ni] = 1;
        int f = -1;
        for (int v = 0; v < NV; ++v)
            if (f < 0 && ((VPEND[v] < 0 && VN[v] == n) || VPEND[v] == n))
                f = v;
        if (f >= 0)
        {
            if (VPEND[f] < 0)
            {
                VG[f] = 1;
                VCE[f] = 1;
            }
            return;
        }
        for (int v = 0; v < NV; ++v)
            if (f < 0 && VN[v] < 0)
                f = v;
        double ba = 0;
        if (f < 0)
            for (int v = 0; v < NV; ++v)
                if (VG[v] == 0 && (f < 0 || VAGE[v] < ba))
                {
                    f = v;
                    ba = VAGE[v];
                }
        if (f < 0)
            for (int v = 0; v < NV; ++v)
                if (f < 0 || VAGE[v] < ba)
                {
                    f = v;
                    ba = VAGE[v];
                }
        if (VN[f] < 0 || VE[f] <= 0)
            vStart(f, n, perc);
        else
        {
            VG[f] = 0;
            VPEND[f] = n;
            VPP[f] = perc;
        }
    }

    void noteOff(double n)
    {
        const int ni = int(n);
        if (KD[ni] > 0)
        {
            KD[ni] = 0;
            kdN -= 1;
        }
        for (int v = 0; v < NV; ++v)
        {
            if (VPEND[v] < 0 && VN[v] == n && VG[v] > 0)
            {
                VG[v] = 0;
                VCE[v] = 0.5;
            }
            if (VPEND[v] == n)
                VPEND[v] = -1;
        }
    }

    void allOff()
    {
        for (int v = 0; v < NV; ++v)
        {
            VG[v] = 0;
            VPEND[v] = -1;
        }
        std::fill(KD, KD + 128, 0.0);
        kdN = 0;
    }

    void rotSet()
    {
        double m = rotMode;
        if (wheel)
            m = m == 1 ? 2 : m == 2 ? 1 : m == 3 ? 2 : m;
        rotEff = m;
        hornTarget = m == 2 ? 6.7 : m == 1 ? 0.8 : 0;
        drumTarget = m == 2 ? 5.8 : m == 1 ? 0.7 : 0;
    }

    void midiEv(double st, double d1, double d2)
    {
        const int t = int(st) & 0xF0;
        if (t == 0x90 && d2 > 0)
            noteOn(d1);
        else if (t == 0x80 || t == 0x90)
            noteOff(d1);
        else if (t == 0xB0)
        {
            if (d1 == 11)
                exT = d2 / 127;
            else if (d1 == 1)
            {
                wheel = d2 >= 64;
                rotSet();
            }
            else if (d1 == 123 || d1 == 120)
                allOff();
        }
    }

    double tap(const std::vector<double>& buf, double d) const
    {
        double rp = rbW - d;
        if (rp < 0)
            rp += rbSize;
        const int i = int(std::floor(rp));
        const double f = rp - i;
        int i2 = i + 1;
        if (i2 >= rbSize)
            i2 = 0;
        return buf[size_t(i)] + (buf[size_t(i2)] - buf[size_t(i)]) * f;
    }

    void sample(double& outL, double& outR)
    {
        while (mqI < mqN && double(mq[mqI].offset) <= si)
        {
            midiEv(mq[mqI].st, mq[mqI].d1, mq[mqI].d2);
            ++mqI;
        }
        si += 1;

        double sum = 0, csum = 0;
        for (int v = 0; v < NV; ++v)
        {
            if (VN[v] < 0)
                continue;
            double ph = VPH[v] + VDT[v];
            ph -= std::floor(ph);
            VPH[v] = ph;
            const double a = 2 * kPi * ph;
            H[1] = std::sin(a);
            const double c2 = 2 * std::cos(a);
            H[2] = c2 * H[1];
            const int km = int(VKM[v]);
            for (int k = 3; k <= km; ++k)
                H[k] = c2 * H[k - 1] - H[k - 2];
            const double* ke = KE + v * 9;
            double y = DG[0] * H[int(ke[0])] + DG[1] * H[int(ke[1])] + DG[2] * H[int(ke[2])] + DG[3] * H[int(ke[3])]
                     + DG[4] * H[int(ke[4])] + DG[5] * H[int(ke[5])] + DG[6] * H[int(ke[6])] + DG[7] * H[int(ke[7])]
                     + DG[8] * H[int(ke[8])];
            if (VPK[v] > 0)
            {
                if (percOn)
                    y += percG * H[int(VKP[v])] * VPE[v];
                VPE[v] *= pC;
            }
            sum += y * VE[v];
            csum += VCE[v];
            VCE[v] *= cC;
            if (VG[v] > 0)
                VE[v] = std::min(1.0, VE[v] + aInc);
            else
                VE[v] = std::max(0.0, VE[v] - (VPEND[v] >= 0 ? sInc : rInc));
            if (VG[v] <= 0 && VE[v] <= 0)
            {
                if (VPEND[v] >= 0)
                    vStart(v, VPEND[v], VPP[v]);
                else
                    VN[v] = -1;
            }
        }

        seed *= 16807;
        seed -= std::floor(seed / 2147483647) * 2147483647;
        const double nz = seed / 1073741824 - 1;
        const double cV3 = nz - cS2;
        const double cV1 = cA1 * cS1 + cA2 * cV3;
        const double cV2 = cS2 + cA2 * cS1 + cA3 * cV3;
        cS1 = 2 * cV1 - cS1;
        cS2 = 2 * cV2 - cS2;

        ex += (exT - ex) * exC;
        double x = (sum * 0.075 + cV1 * csum * click * 0.3) * (0.1 + 0.9 * ex * ex);

        VB[size_t(vbW)] = x;
        if (vib > 0)
        {
            lfoPh += lfoDt;
            lfoPh -= std::floor(lfoPh);
            double rp = vbW - fSr * (0.0002 + vibD * (0.5 + 0.5 * std::sin(2 * kPi * lfoPh)));
            if (rp < 0)
                rp += vbSize;
            const int i = int(std::floor(rp));
            const double f = rp - i;
            int i2 = i + 1;
            if (i2 >= vbSize)
                i2 = 0;
            const double w = VB[size_t(i)] + (VB[size_t(i2)] - VB[size_t(i)]) * f;
            x = vib >= 4 ? 0.5 * (x + w) : w;
        }
        vbW += 1;
        if (vbW >= vbSize)
            vbW = 0;

        x = std::max(-3.0, std::min(3.0, x * drive));
        x = x * (27 + x * x) / (27 + 9 * x * x) * drvN;

        const double sr = fSr;
        lp1 = (1 - xc) * x + xc * lp1;
        lp2 = (1 - xc) * lp1 + xc * lp2;
        hornF = hornC * hornF + (1 - hornC) * hornTarget;
        drumF = drumC * drumF + (1 - drumC) * drumTarget;
        hornPh += hornF / sr;
        if (hornPh >= 1)
            hornPh -= 1;
        drumPh += drumF / sr;
        if (drumPh >= 1)
            drumPh -= 1;
        hl[size_t(rbW)] = x - lp2;
        hr[size_t(rbW)] = x - lp2;
        dl[size_t(rbW)] = lp2;
        dr[size_t(rbW)] = lp2;
        const double hA = 2 * kPi * hornPh, dA = 2 * kPi * drumPh;
        const double hL = tap(hl, 1 + sr * 0.00035 * (1 + std::sin(hA))) * (1 - 0.45 * (1 - std::cos(hA)));
        const double hR = tap(hr, 1 + sr * 0.00035 * (1 + std::sin(hA + 2 * kPi * 0.2))) * (1 - 0.45 * (1 - std::cos(hA + 2 * kPi * 0.2)));
        const double dL = tap(dl, 1 + sr * 0.0005 * (1 + std::sin(dA))) * (1 - 0.2 * (1 - std::cos(dA)));
        const double dR = tap(dr, 1 + sr * 0.0005 * (1 + std::sin(dA + 2 * kPi * 0.2))) * (1 - 0.2 * (1 - std::cos(dA + 2 * kPi * 0.2)));
        rbW += 1;
        if (rbW >= rbSize)
            rbW = 0;

        if (rotMode > 0)
        {
            outL = (hL * hornG + dL * drumG) * vol;
            outR = (hR * hornG + dR * drumG) * vol;
        }
        else
        {
            outL = x * vol;
            outR = outL;
        }
    }

    static constexpr double kHK[9] = { 1, 3, 2, 4, 6, 8, 10, 12, 16 };

    double fSr = 44100;
    double VE[NV] = {}, VPH[NV] = {}, VDT[NV] = {}, VPE[NV] = {}, VPK[NV] = {}, VCE[NV] = {};
    double VAGE[NV] = {}, VKM[NV] = {}, VPEND[NV] = {}, VKP[NV] = {}, VPP[NV] = {};
    double KE[NV * 9] = {}, H[17] = {}, DG[9] = {}, KD[128] = {};
    double kdN = 0, age = 0, exT = 1;
    bool wheel = false, percOn = false;
    double percK = 6, pC = 0, percG = 1, click = 0, cC = 0, vib = 0, vibD = 0, lfoDt = 0;
    double drive = 1, drvN = 1, rotMode = 1, acc = 1, hornC = 0, drumC = 0, bal = 0.5, hornG = 1, drumG = 1, vol = 1;
    double aInc = 0, rInc = 0, sInc = 0, exC = 0, xc = 0, cA1 = 0, cA2 = 0, cA3 = 0;
    double hornTarget = 0.8, drumTarget = 0.7;
    std::vector<double> VB, hl, hr, dl, dr;
    int vbSize = 0, vbW = 0, rbSize = 0, rbW = 0;
    double lfoPh = 0, lp1 = 0, lp2 = 0, seed = 22222, cS1 = 0, cS2 = 0;

    MidiEv mq[kMaxEvents];
    uint32_t mqN = 0, mqI = 0;
    double si = 0;
};

} // namespace nullorgan
