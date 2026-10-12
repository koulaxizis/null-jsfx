/*
 * NULL JSFX - Convolution, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/convolution.jsfx, computed in double like EEL2 (FFTs with
 * EelFft.hpp, EEL2's fft/ifft/convolve_c semantics): uniformly partitioned overlap-save FFT
 * convolution in 1024-sample blocks (the reported latency) with an impulse response of up to
 * 3 s: one of five generated rooms (the JSFX's Park-Miller noise, restarted for every build)
 * or a WAV file (resampled linearly, as the JSFX's load_file), trimmed by Length, shifted by
 * Pre-Delay, faded out over 50 ms and energy-normalised. The wet signal gets one-pole low and
 * high cuts; the dry signal is delayed by one block to stay aligned.
 */

#pragma once

#include "ConvolutionWav.hpp"
#include "EelFft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullconv {

static constexpr int kB = 1024, kM = 2 * kB, kFS = 2 * kM; // block, FFT size, slots per spectrum
static constexpr int kFileSource = 5;                     // Source = IR File
static constexpr int kEnvBins = 200;                       // display: IR envelope over 3 s

class ConvolutionProcessor {
public:
    // @init: buffers for 3 s of IR at this rate, everything cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPmax = int(std::ceil(3.0 * sr / kB));
        fIrLenMax = fPmax * kB;
        for (std::vector<double>* v : { &fHl, &fHr, &fXl, &fXr })
            v->assign(size_t(fPmax) * kFS, 0.0);
        fIr.assign(size_t(2 * fIrLenMax), 0.0);
        fInL.assign(kM, 0.0);
        fInR.assign(kM, 0.0);
        fOlaL.assign(kB, 0.0);
        fOlaR.assign(kB, 0.0);
        fTmp.assign(kFS, 0.0);
        fAcc.assign(kFS, 0.0);
        fP = 1;
        fPos = fSlot = 0;
        fIrLen = 0;
        fLastSrc = -1;
        fLc1 = fLc2 = fHc1 = fLcr1 = fLcr2 = fHcr1 = 0.0;
        fSeed = 12345.0;
        std::fill(fEnv, fEnv + kEnvBins, 0.0f);
    }

    // the IR file (Source = IR File); fileId changes whenever another file is chosen, like the
    // JSFX's slider2. The data is swapped in, so the old one can be freed off the audio thread.
    void swapFile(IrFile& file, int fileId)
    {
        std::swap(fFile.data, file.data);
        std::swap(fFile.channels, file.channels);
        std::swap(fFile.sampleRate, file.sampleRate);
        fFileId = fileId;
    }

    void setParams(int src, double lengthPct, double preDelayMs, double lowCutHz, double highCutHz, double mixPct,
                   double outDb)
    {
        fSrc = src;
        fLength = lengthPct;
        fPreDelay = preDelayMs;
        fLowCut = lowCutHz;
        fHighCut = highCutHz;
        fMixPct = mixPct;
        fOutDb = outDb;
    }

    // @slider: rebuild the IR when the source, file, length or pre-delay changed, then the tone
    void update()
    {
        if (fSrc != fLastSrc || fFileId != fLastFile || fLength != fLastLen || fPreDelay != fLastPd)
        {
            build();
            fSlot = 0;
            fLastSrc = fSrc;
            fLastFile = fFileId;
            fLastLen = fLength;
            fLastPd = fPreDelay;
        }
        fLc = std::exp(-2.0 * M_PI * fLowCut / fSr);
        fHc = std::exp(-2.0 * M_PI * std::min(fHighCut, fSr * 0.45) / fSr);
        fMix = fMixPct / 100.0;
        fOut = std::pow(10.0, fOutDb / 20.0);
    }

    static constexpr int latency() noexcept { return kB; }

    // display: the IR's length in samples (0 = none) and its peak per 15 ms over 3 s
    int irLength() const noexcept { return fIrLen; }
    int builds() const noexcept { return fBuilds; }
    const float* envelope() const noexcept { return fEnv; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t n = 0; n < frames; ++n)
        {
            fInL[size_t(fPos)] = inL[n];
            fInR[size_t(fPos)] = inR[n];
            if (++fPos >= kM)
                fPos = 0;
            const int bpos = fPos % kB;
            double wl = fOlaL[size_t(bpos)], wr = fOlaR[size_t(bpos)];
            int dpos = fPos - kB - 1;
            if (dpos < 0)
                dpos += kM;
            const double dl = fInL[size_t(dpos)], dr = fInR[size_t(dpos)];
            if (bpos == kB - 1)
            {
                if (++fSlot >= fP)
                    fSlot = 0;
                convBlock(fInL, fXl, fHl, fOlaL);
                convBlock(fInR, fXr, fHr, fOlaR);
            }

            fLc1 = fLc * (fLc1 + wl - fLc2);
            fLc2 = wl;
            wl = fLc1;
            fHc1 = (1.0 - fHc) * wl + fHc * fHc1;
            wl = fHc1;
            fLcr1 = fLc * (fLcr1 + wr - fLcr2);
            fLcr2 = wr;
            wr = fLcr1;
            fHcr1 = (1.0 - fHc) * wr + fHc * fHcr1;
            wr = fHcr1;

            outL[n] = static_cast<T>((dl * (1.0 - fMix) + wl * fMix) * fOut);
            outR[n] = static_cast<T>((dr * (1.0 - fMix) + wr * fMix) * fOut);
        }
    }

private:
    // pm_rand: Park-Miller in exact doubles, 0 < value < 1
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    // gen_ir: early reflections + exponentially decaying noise that darkens over time
    void genIr(double* dst, int len, double t60, double bright, double erMs)
    {
        std::fill(dst, dst + len, 0.0);
        double lp = 0.0;
        for (int n = 0; n < len; ++n)
        {
            const double t = n / fSr;
            const double env = std::exp(-6.9 * t / t60);
            const double c = std::exp(-2.0 * M_PI * (300.0 + bright * std::exp(-t / (t60 * 0.35))) / fSr);
            const double x = rnd() * 2.0 - 1.0;
            lp = (1.0 - c) * x + c * lp;
            dst[n] = lp * env * std::min(1.0, t / (erMs / 1000.0));
        }
        for (int k = 0; k < 8; ++k)
        {
            const double n = std::floor((2.0 + rnd() * erMs) * fSr / 1000.0);
            if (n < len)
                dst[size_t(n)] += (rnd() * 2.0 - 1.0) * 0.6 * std::exp(-k * 0.25);
        }
    }

    // load_file: the chosen WAV resampled into both IR channels; returns the length (0 = none)
    int loadFile()
    {
        const int nch = fFile.channels;
        const double fsr = fFile.sampleRate;
        if (nch <= 0 || fsr <= 0.0)
            return 0;
        const double avail = std::min(double(fFile.data.size()), double(fIrLenMax) * nch * 2.0);
        double n = std::floor(avail / nch);
        const double count = std::min(n * nch, 2.0 * fIrLenMax);
        n = std::floor(count / nch);
        const double ratio = fsr / fSr;
        const int len = int(std::min(double(fIrLenMax), std::floor(n / ratio)));
        const double* d = fFile.data.data();
        for (int c = 0; c < 2; ++c)
        {
            const int ch = std::min(c, nch - 1);
            double* dst = fIr.data() + size_t(c) * size_t(fIrLenMax);
            for (int i = 0; i < len; ++i)
            {
                double p = i * ratio;
                const double f = p - std::floor(p);
                p = std::min(std::floor(p), n - 1.0);
                dst[i] = d[size_t(p) * size_t(nch) + size_t(ch)] * (1.0 - f) +
                         d[size_t(std::min(p + 1.0, n - 1.0)) * size_t(nch) + size_t(ch)] * f;
            }
        }
        return len;
    }

    // partition: split one channel's IR into P blocks, normalise its energy, FFT each block
    void partition(const double* src, std::vector<double>& H, int len)
    {
        double e = 0.0;
        for (int i = 0; i < len; ++i)
            e += src[i] * src[i];
        const double g = e > 0.0 ? 1.0 / std::sqrt(e) : 0.0;
        for (int p = 0; p < fP; ++p)
        {
            double* h = H.data() + size_t(p) * kFS;
            std::fill(h, h + kFS, 0.0);
            for (int i = 0; i < kB; ++i)
                if (p * kB + i < len)
                    h[2 * i] = src[p * kB + i] * g;
            eelfft::fft(h, kM);
        }
    }

    void build()
    {
        const int pd = int(std::floor(fPreDelay * fSr / 1000.0));
        fSeed = 12345.0;
        double* irL = fIr.data();
        double* irR = fIr.data() + fIrLenMax;
        int srcLen;
        if (fSrc == kFileSource)
            srcLen = loadFile();
        else
        {
            const double t60 = fSrc == 0 ? 0.5 : fSrc == 1 ? 1.2 : fSrc == 2 ? 2.4 : fSrc == 3 ? 1.8 : 3.0;
            srcLen = int(std::min(double(fIrLenMax - pd), std::floor(std::min(3.0, t60 * 1.2) * fSr)));
            const double bright = fSrc == 3 ? 9000.0 : fSrc == 4 ? 3500.0 : 6000.0;
            const double er = fSrc == 0 ? 8.0 : fSrc == 3 ? 2.0 : 25.0;
            genIr(irL, srcLen, t60, bright, er);
            genIr(irR, srcLen, t60, bright, er);
        }
        srcLen = int(std::floor(srcLen * fLength / 100.0));
        if (pd > 0 && srcLen > 0)
        {
            srcLen = std::min(srcLen, fIrLenMax - pd);
            for (int i = srcLen - 1; i >= 0; --i)
            {
                irL[i + pd] = irL[i];
                irR[i + pd] = irR[i];
            }
            std::fill(irL, irL + pd, 0.0);
            std::fill(irR, irR + pd, 0.0);
            srcLen += pd;
        }
        const int fade = std::min(srcLen, int(std::floor(0.05 * fSr)));
        for (int i = 0; i < fade; ++i)
        {
            irL[srcLen - 1 - i] *= i / (0.05 * fSr);
            irR[srcLen - 1 - i] *= i / (0.05 * fSr);
        }
        fIrLen = srcLen;
        ++fBuilds;
        fP = std::max(1, std::min(fPmax, int(std::ceil(double(srcLen) / kB))));
        if (srcLen > 0)
        {
            partition(irL, fHl, srcLen);
            partition(irR, fHr, srcLen);
        }
        else
        {
            std::fill(fHl.begin(), fHl.begin() + size_t(fP) * kFS, 0.0);
            std::fill(fHr.begin(), fHr.begin() + size_t(fP) * kFS, 0.0);
        }
        std::fill(fXl.begin(), fXl.end(), 0.0);
        std::fill(fXr.begin(), fXr.end(), 0.0);
        envelope(irL, irR);
    }

    // display (as in convolution_gfx.jsfx): the peak of both channels per 1/200 of 3 s
    void envelope(const double* irL, const double* irR)
    {
        for (int j = 0; j < kEnvBins; ++j)
        {
            const int a = int(std::floor(j * 3.0 * fSr / kEnvBins));
            const int b = std::min(fIrLen, int(std::floor((j + 1) * 3.0 * fSr / kEnvBins)));
            double m = 0.0;
            for (int i = a; i < b; ++i)
                m = std::max(m, std::max(std::fabs(irL[i]), std::fabs(irR[i])));
            fEnv[j] = float(m);
        }
    }

    // conv_block: one block of overlap-save convolution for one channel
    void convBlock(const std::vector<double>& inb, std::vector<double>& X, const std::vector<double>& H,
                   std::vector<double>& ola)
    {
        double* xs = X.data() + size_t(fSlot) * kFS;
        for (int i = 0; i < kM; ++i)
        {
            xs[2 * i] = inb[size_t((fPos + i) % kM)];
            xs[2 * i + 1] = 0.0;
        }
        eelfft::fft(xs, kM);
        double* acc = fAcc.data();
        double* tmp = fTmp.data();
        std::fill(acc, acc + kFS, 0.0);
        for (int p = 0; p < fP; ++p)
        {
            int s = fSlot - p;
            if (s < 0)
                s += fP;
            std::copy(X.data() + size_t(s) * kFS, X.data() + size_t(s + 1) * kFS, tmp);
            eelfft::convolveC(tmp, H.data() + size_t(p) * kFS, kM);
            for (int i = 0; i < kFS; ++i)
                acc[i] += tmp[i];
        }
        eelfft::ifft(acc, kM);
        for (int i = 0; i < kB; ++i)
            ola[size_t(i)] = acc[2 * (kB + i)] / kM;
    }

    double fSr = 48000.0;
    int fBuilds = 0, fPmax = 1, fIrLenMax = kB, fP = 1, fPos = 0, fSlot = 0, fIrLen = 0;
    std::vector<double> fHl, fHr, fXl, fXr, fIr, fInL, fInR, fOlaL, fOlaR, fTmp, fAcc;
    IrFile fFile;
    int fFileId = 0, fLastFile = 0;
    int fSrc = 2, fLastSrc = -1;
    double fLength = 100.0, fPreDelay = 0.0, fLowCut = 80.0, fHighCut = 12000.0, fMixPct = 30.0, fOutDb = 0.0;
    double fLastLen = 0.0, fLastPd = 0.0;
    double fLc = 0.0, fHc = 0.0, fMix = 0.3, fOut = 1.0;
    double fLc1 = 0.0, fLc2 = 0.0, fHc1 = 0.0, fLcr1 = 0.0, fLcr2 = 0.0, fHcr1 = 0.0;
    double fSeed = 12345.0;
    float fEnv[kEnvBins] = {};
};

} // namespace nullconv
