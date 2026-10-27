/*
 * NULL JSFX - Multichannel Loudness, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/multichannel_loudness.jsfx, computed in double like EEL2 and
 * in the same order: the Loudness Meter's measurement (LoudnessMeterDSP.hpp) on a surround bed,
 * every channel of the Bed but the LFE K-weighted and weighted by its place (ITU-R BS.1770-4:
 * 1.41 at 60 to 120 degrees, 5.1 Ls/Rs and 7.1 Lss/Rss; 1.0 for the rest), and the true peak of
 * every channel of the Bed, LFE too. The audio passes through. The results go to a View the UI
 * reads.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nullmloud {

static constexpr int kHist = 3000;  // history entries (100 ms each, 5 min)
static constexpr int kBins = 9000;  // 0.01 LU bins from -70 LUFS
static constexpr int kCh = 12;      // the bed's pins: L R C LFE Ls Rs Lrs Rrs Ltf Rtf Ltr Rtr

// what the interface draws; written by the audio thread, read by the UI (a torn read only
// draws one odd frame)
struct View {
    std::atomic<float> m { -200.0f }, s { -200.0f }, integrated { -200.0f }, lra { 0.0f };
    std::atomic<float> tpMax { 0.0f }, mMax { -200.0f }, sMax { -200.0f };
    float histS[kHist] = {}, histM[kHist] = {};
    std::atomic<int> hw { 0 }, hn { 0 };
    std::atomic<float> chPeak[kCh] {};  // each channel's true peak since the interface last looked
};

// loudness of a mean-square energy, -200 for silence (the JSFX's lm_db)
inline double lmDb(double p) noexcept { return p > 0.0 ? -0.691 + 10.0 * std::log10(p) : -200.0; }

class LoudnessMeter {
public:
    // @init
    void setSampleRate(double sr) noexcept
    {
        const double pi = 3.141592653589793;
        double k = std::tan(pi * 1681.974450955533 / sr);
        const double vh = std::pow(10.0, 3.999843853973347 / 20.0);
        const double vb = std::pow(vh, 0.4996667741545416);
        double q = 0.7071752369554196;
        const double a0 = 1.0 + k / q + k * k;
        b0 = (vh + vb * k / q + k * k) / a0;
        b1 = 2.0 * (k * k - vh) / a0;
        b2 = (vh - vb * k / q + k * k) / a0;
        a1 = 2.0 * (k * k - 1.0) / a0;
        a2 = (1.0 - k / q + k * k) / a0;
        k = std::tan(pi * 38.13547087602444 / sr);
        q = 0.5003270373238773;
        const double c0 = 1.0 + k / q + k * k;
        c1 = 2.0 * (k * k - 1.0) / c0;
        c2 = (1.0 - k / q + k * k) / c0;
        std::memset(z, 0, sizeof(z));

        static const double ph01[24] = {
            0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625,
            0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500,
            -0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250,
            0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375,
        };
        for (int i = 0; i < 24; ++i)
            tc[i] = ph01[i];
        for (int i = 24; i < 48; ++i)
            tc[i] = tc[47 - i];
        std::memset(x, 0, sizeof(x));
        tw = 0;

        n = std::floor(sr * 0.1 + 0.5);
        acc = 0.0;
        cnt = 0;
        std::memset(sb, 0, sizeof(sb));
        sbw = 0;
        reset();
        m = s = -200.0;
        publish();
    }

    // clears integrated, LRA, maxima and the history
    void reset() noexcept
    {
        std::memset(ic, 0, sizeof(ic));
        std::memset(ie, 0, sizeof(ie));
        std::memset(rc, 0, sizeof(rc));
        blocks = 0;
        absSum = 0.0; absN = 0; ihi = 0;
        stSum = 0.0; stN = 0; rhi = 0;
        integrated = -200.0;
        lra = 0.0;
        mMax = sMax = -200.0;
        tpMax = spMax = 0.0;
        hw = hn = 0;
        if (view)
        {
            view->hw.store(0, std::memory_order_relaxed);
            view->hn.store(0, std::memory_order_relaxed);
        }
        publish();
    }

    void setView(View* v) noexcept { view = v; }

    // @slider: the Bed (0 = 5.1, 1 = 7.1, 2 = 7.1.4) sets the weights and the channels measured
    void setBed(int bed) noexcept
    {
        for (int i = 0; i < kCh; ++i)
            w[i] = i == 3 ? 0 : (i >= 6 && bed == 0) || (i >= 8 && bed < 2) ? 0 : i == 4 || i == 5 ? 1.41 : 1;
        nch = bed == 0 ? 6 : bed == 1 ? 8 : 12;
    }

    // @sample for a block: the twelve pins pass through
    void process(const float* const* in, float* const* out, uint32_t frames) noexcept
    {
        double cp[kCh] = {};
        for (uint32_t i = 0; i < frames; ++i)
        {
            tw -= 1;
            if (tw < 0)
                tw = 11;
            for (int c = 0; c < nch; ++c)
            {
                const double v = in[c][i];
                const double kv = kweight(v, z[c]);
                acc += w[c] * kv * kv;
                double* xc = x[c];
                xc[tw] = xc[tw + 12] = v;
                double pk = std::fabs(v);
                if (pk > spMax)
                    spMax = pk;
                for (int ph = 0; ph < 48; ph += 12)
                {
                    double a = 0.0;
                    for (int j = 0; j < 12; ++j)
                        a += tc[ph + j] * xc[tw + j];
                    pk = std::max(pk, std::fabs(a));
                }
                if (pk > tpMax)
                    tpMax = pk;
                if (pk > cp[c])
                    cp[c] = pk;
            }

            cnt += 1;
            if (cnt >= n)
            {
                cnt = 0;
                subblock();
            }
        }
        for (int c = 0; c < kCh; ++c)
        {
            if (out[c] != in[c])
                std::memcpy(out[c], in[c], sizeof(float) * frames);
            if (view && float(cp[c]) > view->chPeak[c].load(std::memory_order_relaxed))
                view->chPeak[c].store(float(cp[c]), std::memory_order_relaxed);
        }
        if (view)
            view->tpMax.store(float(tpMax), std::memory_order_relaxed);
    }

private:
    double kweight(double x, double* z) const noexcept
    {
        const double y = b0 * x + z[0];
        z[0] = b1 * x - a1 * y + z[1];
        z[1] = b2 * x - a2 * y;
        const double k = y + z[2];
        z[2] = -2.0 * y - c1 * k + z[3];
        z[3] = y - c2 * k;
        return k;
    }

    void subblock() noexcept
    {
        sb[sbw] = acc;
        acc = 0.0;
        sbw += 1;
        if (sbw >= 30)
            sbw = 0;
        blocks += 1;

        double sum = 0.0;
        int k = sbw;
        for (int j = 0; j < 4; ++j) { k -= 1; if (k < 0) k += 30; sum += sb[k]; }
        const double mp = sum / (4.0 * n);
        for (int j = 0; j < 26; ++j) { k -= 1; if (k < 0) k += 30; sum += sb[k]; }
        const double stp = sum / (30.0 * n);
        m = lmDb(mp);
        s = lmDb(stp);

        if (blocks >= 4 && m > -70.0)
        {
            int b = int(std::min(double(kBins - 1), std::floor((m + 70.0) * 100.0)));
            ic[b] += 1.0;
            ie[b] += mp;
            if (b >= ihi)
                ihi = b + 1;
            absSum += mp;
            absN += 1.0;
            const double rel = lmDb(absSum / absN) - 10.0;
            b = int(std::max(0.0, std::floor((rel + 70.0) * 100.0)));
            double es = 0.0, ec = 0.0;
            for (; b < ihi; ++b) { es += ie[b]; ec += ic[b]; }
            integrated = ec > 0.0 ? lmDb(es / ec) : -200.0;
        }

        if (blocks >= 30 && s > -70.0)
        {
            int b = int(std::min(double(kBins - 1), std::floor((s + 70.0) * 100.0)));
            rc[b] += 1.0;
            if (b >= rhi)
                rhi = b + 1;
            stSum += stp;
            stN += 1.0;
            const double rel = lmDb(stSum / stN) - 20.0;
            b = int(std::max(0.0, std::floor((rel + 70.0) * 100.0)));
            double cntAll = 0.0;
            for (int j = b; j < rhi; ++j)
                cntAll += rc[j];
            if (cntAll > 0.0)
            {
                const double lo = std::floor((cntAll - 1.0) * 0.1 + 0.5);
                const double hi = std::floor((cntAll - 1.0) * 0.95 + 0.5);
                double cum = 0.0;
                int ilo = -1, ihiIdx = -1;
                for (int j = b; j < rhi && ihiIdx < 0; ++j)
                {
                    cum += rc[j];
                    if (ilo < 0 && cum > lo) ilo = j;
                    if (ihiIdx < 0 && cum > hi) ihiIdx = j;
                }
                lra = (ihiIdx - ilo) / 100.0;
            }
        }

        if (m > mMax) mMax = m;
        if (s > sMax) sMax = s;
        if (view)
        {
            view->histS[hw] = float(s);
            view->histM[hw] = float(m);
        }
        hw += 1;
        if (hw >= kHist)
            hw = 0;
        hn = std::min(kHist, hn + 1);
        publish();
    }

    void publish() noexcept
    {
        if (!view)
            return;
        view->m.store(float(m), std::memory_order_relaxed);
        view->s.store(float(s), std::memory_order_relaxed);
        view->integrated.store(float(integrated), std::memory_order_relaxed);
        view->lra.store(float(lra), std::memory_order_relaxed);
        view->tpMax.store(float(tpMax), std::memory_order_relaxed);
        view->mMax.store(float(mMax), std::memory_order_relaxed);
        view->sMax.store(float(sMax), std::memory_order_relaxed);
        view->hn.store(hn, std::memory_order_relaxed);
        view->hw.store(hw, std::memory_order_release);
    }

    View* view = nullptr;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, c1 = 0, c2 = 0;
    double z[kCh][4] = {};
    double tc[48] = {}, x[kCh][24] = {};
    double w[kCh] = {};
    int nch = 12;
    int tw = 0;
    double n = 4410.0, acc = 0.0;
    double cnt = 0;
    double sb[30] = {};
    int sbw = 0;
    double blocks = 0;
    double ic[kBins] = {}, ie[kBins] = {}, rc[kBins] = {};
    double absSum = 0, absN = 0, stSum = 0, stN = 0;
    int ihi = 0, rhi = 0;
    double m = -200, s = -200, integrated = -200, lra = 0, mMax = -200, sMax = -200, tpMax = 0, spMax = 0;
    int hw = 0, hn = 0;
};

} // namespace nullmloud
