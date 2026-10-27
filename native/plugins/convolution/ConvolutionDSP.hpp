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
 * Only the first IR is built at once. Later ones are built in small steps, one every 32 samples
 * as the audio plays, into a second partition set while the current IR keeps playing; a ready
 * IR takes over at the next block, crossfaded over that block, and the tail carries on (the
 * input spectra of the last 3 s stay). The audio thread never stops for a whole rebuild.
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
    // @init: buffers for 3 s of IR at this rate (two partition sets), everything cleared
    void setSampleRate(double sr)
    {
        fSr = sr;
        fPmax = int(std::ceil(3.0 * sr / kB));
        fIrLenMax = fPmax * kB;
        for (std::vector<double>* v : { &fSetA[0], &fSetA[1], &fSetB[0], &fSetB[1], &fXl, &fXr })
            v->assign(size_t(fPmax) * kFS, 0.0);
        fH[0] = fSetA[0].data();
        fH[1] = fSetA[1].data();
        fG[0] = fSetB[0].data();
        fG[1] = fSetB[1].data();
        fIr.assign(size_t(2 * fIrLenMax), 0.0);
        fInL.assign(kM, 0.0);
        fInR.assign(kM, 0.0);
        fOlaL.assign(kB, 0.0);
        fOlaR.assign(kB, 0.0);
        fTmp.assign(kFS, 0.0);
        fAcc.assign(kFS, 0.0);
        fAccO.assign(kFS, 0.0);
        fP = fPo = 1;
        fPos = fSlot = 0;
        fIrLen = 0;
        fGen = 0;
        fStFile = -1;
        fBst = 0;
        fBcnt = 0;
        fXfOn = false;
        fCurSrc = 0;
        fCurFile = 0;
        fCurLen = fCurPd = 0.0;
        fLc1 = fLc2 = fHc1 = fLcr1 = fLcr2 = fHcr1 = 0.0;
        fSeed = 12345.0;
        std::fill(fEnv, fEnv + kEnvBins, 0.0f);
        std::fill(fEnvNext, fEnvNext + kEnvBins, 0.0f);
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

    // @slider: a new file is staged (a build using it starts over); the first IR is built at
    // once, later ones step by step in process(); then the tone
    void update()
    {
        if (fFileId != fStFile)
        {
            stageFile();
            if (fBst > 0 && fBsrc == kFileSource)
                buildStart();
        }
        if (fGen == 0)
        {
            buildStart();
            while (fBst < kReady)
                buildStep();
            buildApply();
        }
        else if (fBst == 0 && wanted())
            buildStart();
        fLc = std::exp(-2.0 * M_PI * fLowCut / fSr);
        fHc = std::exp(-2.0 * M_PI * std::min(fHighCut, fSr * 0.45) / fSr);
        fMix = fMixPct / 100.0;
        fOut = std::pow(10.0, fOutDb / 20.0);
    }

    static constexpr int latency() noexcept { return kB; }

    // display: the IR's length in samples (0 = none), a counter of IRs that took over, and the
    // IR's peak per 15 ms over 3 s
    int irLength() const noexcept { return fIrLen; }
    int builds() const noexcept { return fGen; }
    bool building() const noexcept { return fBst != 0; }
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
                if (++fSlot >= fPmax)
                    fSlot = 0;
                // a finished IR takes over here, crossfaded from the old one over this block
                if (fBst == kReady)
                {
                    buildApply();
                    fXfOn = true;
                }
                convBlock(fInL, fXl, fH[0], fG[0], fOlaL);
                convBlock(fInR, fXr, fH[1], fG[1], fOlaR);
                fXfOn = false;
                if (fBst == 0 && wanted())
                    buildStart();
            }
            // one build step every 32 samples
            if (fBst > 0 && fBst < kReady && ++fBcnt >= 32)
            {
                fBcnt = 0;
                buildStep();
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
    // the build steps (fBst), as the JSFX's build_step
    enum { kNoise = 1, kResample, kTrim, kShift, kFade, kEnergy, kPartition, kReady };

    // pm_rand: Park-Miller in exact doubles, 0 < value < 1
    double rnd() noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483647.0;
    }

    // stage_file: the chosen WAV's length after resampling (0 = none)
    void stageFile()
    {
        fStFile = fFileId;
        fStLen = 0;
        const int nch = fFile.channels;
        const double fsr = fFile.sampleRate;
        if (nch <= 0 || fsr <= 0.0)
            return;
        const double avail = std::min(double(fFile.data.size()), double(fIrLenMax) * nch * 2.0);
        const double n = std::floor(avail / nch);
        fStN = std::floor(std::min(n * nch, 2.0 * fIrLenMax) / nch);
        fStNch = nch;
        fStRatio = fsr / fSr;
        fStLen = int(std::min(double(fIrLenMax), std::floor(fStN / fStRatio)));
    }

    bool wanted() const noexcept
    {
        return fSrc != fCurSrc || fFileId != fCurFile || fLength != fCurLen || fPreDelay != fCurPd;
    }

    // build_start: start building the IR for the current settings
    void buildStart()
    {
        fBsrc = fSrc;
        fBfile = fFileId;
        fBlen = fLength;
        fBpd = fPreDelay;
        fBd = int(std::floor(fBpd * fSr / 1000.0));
        fSeed = 12345.0;
        fBc = fBi = 0;
        fBlp = 0.0;
        fEnvJ = 0;
        if (fBsrc == kFileSource)
        {
            fBn = fStLen;
            fBst = kResample;
        }
        else
        {
            fBt60 = fBsrc == 0 ? 0.5 : fBsrc == 1 ? 1.2 : fBsrc == 2 ? 2.4 : fBsrc == 3 ? 1.8 : 3.0;
            fBn = int(std::min(double(fIrLenMax - fBd), std::floor(std::min(3.0, fBt60 * 1.2) * fSr)));
            fBbr = fBsrc == 3 ? 9000.0 : fBsrc == 4 ? 3500.0 : 6000.0;
            fBer = fBsrc == 0 ? 8.0 : fBsrc == 3 ? 2.0 : 25.0;
            fBst = kNoise;
        }
    }

    // build_step: one step of the build
    void buildStep()
    {
        double* irL = fIr.data();
        double* irR = fIr.data() + fIrLenMax;
        switch (fBst)
        {
        case kNoise: {
            // generated room: decaying noise that darkens over time, 1024 samples a step, then
            // the early reflections; left channel, then right
            double* dst = fBc ? irR : irL;
            const int end = std::min(fBn, fBi + 1024);
            for (; fBi < end; ++fBi)
            {
                const double t = fBi / fSr;
                const double env = std::exp(-6.9 * t / fBt60);
                const double c = std::exp(-2.0 * M_PI * (300.0 + fBbr * std::exp(-t / (fBt60 * 0.35))) / fSr);
                const double x = rnd() * 2.0 - 1.0;
                fBlp = (1.0 - c) * x + c * fBlp;
                dst[fBi] = fBlp * env * std::min(1.0, t / (fBer / 1000.0));
            }
            if (fBi >= fBn)
            {
                for (int k = 0; k < 8; ++k)
                {
                    const double q = std::floor((2.0 + rnd() * fBer) * fSr / 1000.0);
                    if (q < fBn)
                        dst[size_t(q)] += (rnd() * 2.0 - 1.0) * 0.6 * std::exp(-k * 0.25);
                }
                if (fBc == 0)
                {
                    fBc = 1;
                    fBi = 0;
                    fBlp = 0.0;
                }
                else
                    fBst = kTrim;
            }
            break;
        }
        case kResample: {
            // the staged file, resampled linearly into both IR channels, 2048 samples a step
            const int ch = std::min(fBc, fStNch - 1);
            const double* d = fFile.data.data();
            double* dst = fBc ? irR : irL;
            const int end = std::min(fBn, fBi + 2048);
            for (; fBi < end; ++fBi)
            {
                double p = fBi * fStRatio;
                const double f = p - std::floor(p);
                p = std::min(std::floor(p), fStN - 1.0);
                dst[fBi] = d[size_t(p) * size_t(fStNch) + size_t(ch)] * (1.0 - f) +
                           d[size_t(std::min(p + 1.0, fStN - 1.0)) * size_t(fStNch) + size_t(ch)] * f;
            }
            if (fBi >= fBn)
            {
                if (fBc == 0)
                {
                    fBc = 1;
                    fBi = 0;
                }
                else
                    fBst = kTrim;
            }
            break;
        }
        case kTrim:
            // Length trims the IR; Pre-Delay shifts it later in time
            fBn = int(std::floor(fBn * fBlen / 100.0));
            if (fBd > 0 && fBn > 0)
            {
                fBn = std::min(fBn, fIrLenMax - fBd);
                fBi = fBn - 1;
                fBst = kShift;
            }
            else
                fBst = kFade;
            break;
        case kShift: {
            const int end = std::max(-1, fBi - 8192);
            for (; fBi > end; --fBi)
            {
                irL[fBi + fBd] = irL[fBi];
                irR[fBi + fBd] = irR[fBi];
            }
            if (fBi < 0)
            {
                std::fill(irL, irL + fBd, 0.0);
                std::fill(irR, irR + fBd, 0.0);
                fBn += fBd;
                fBst = kFade;
            }
            break;
        }
        case kFade: {
            // fade out the last 50 ms so a trimmed IR does not end with a click
            const int fade = std::min(fBn, int(std::floor(0.05 * fSr)));
            for (int i = 0; i < fade; ++i)
            {
                irL[fBn - 1 - i] *= i / (0.05 * fSr);
                irR[fBn - 1 - i] *= i / (0.05 * fSr);
            }
            fBP = std::max(1, std::min(fPmax, int(std::ceil(double(fBn) / kB))));
            fBc = fBi = 0;
            fBe = 0.0;
            if (fBn > 0)
                fBst = kEnergy;
            else
            {
                std::fill(fG[0], fG[0] + size_t(fBP) * kFS, 0.0);
                std::fill(fG[1], fG[1] + size_t(fBP) * kFS, 0.0);
                envelopeSteps(true);
                fBst = kReady;
            }
            break;
        }
        case kEnergy: {
            // each channel is normalised to unit energy, summed 8192 samples a step
            const double* src = fBc ? irR : irL;
            const int end = std::min(fBn, fBi + 8192);
            for (; fBi < end; ++fBi)
                fBe += src[fBi] * src[fBi];
            if (fBi >= fBn)
            {
                const double g = fBe > 0.0 ? 1.0 / std::sqrt(fBe) : 0.0;
                if (fBc == 0)
                {
                    fBgL = g;
                    fBc = 1;
                    fBi = 0;
                    fBe = 0.0;
                }
                else
                {
                    fBgR = g;
                    fBc = fBi = 0;
                    fBst = kPartition;
                }
            }
            envelopeSteps(false);
            break;
        }
        case kPartition:
            // split each channel into P partitions and FFT each one, two a step
            for (int r = 0; r < 2 && fBst == kPartition; ++r)
            {
                const double* src = fBc ? irR : irL;
                const double g = fBc ? fBgR : fBgL;
                double* h = fG[fBc] + size_t(fBi) * kFS;
                std::fill(h, h + kFS, 0.0);
                for (int i = 0; i < kB; ++i)
                    if (fBi * kB + i < fBn)
                        h[2 * i] = src[fBi * kB + i] * g;
                eelfft::fft(h, kM);
                if (++fBi >= fBP)
                {
                    if (fBc == 0)
                    {
                        fBc = 1;
                        fBi = 0;
                    }
                    else
                        fBst = kReady;
                }
            }
            envelopeSteps(fBst == kReady);
            break;
        default:
            break;
        }
    }

    // build_apply: the built IR takes over; the two partition sets swap, the old one stays for
    // the crossfade
    void buildApply()
    {
        std::swap(fH[0], fG[0]);
        std::swap(fH[1], fG[1]);
        fPo = fP;
        fP = fBP;
        fIrLen = fBn;
        fCurSrc = fBsrc;
        fCurFile = fBfile;
        fCurLen = fBlen;
        fCurPd = fBpd;
        std::copy(fEnvNext, fEnvNext + kEnvBins, fEnv);
        ++fGen;
        fBst = 0;
    }

    // display (as in convolution_gfx.jsfx): the peak of both channels per 1/200 of 3 s, about
    // 8192 samples a step while the IR is normalised and partitioned (all when last is set)
    void envelopeSteps(bool last)
    {
        const double* irL = fIr.data();
        const double* irR = fIr.data() + fIrLenMax;
        int done = 0;
        while (fEnvJ < kEnvBins && (last || done < 8192))
        {
            const int j = fEnvJ++;
            const int a = int(std::floor(j * 3.0 * fSr / kEnvBins));
            const int b = std::min(fBn, int(std::floor((j + 1) * 3.0 * fSr / kEnvBins)));
            double m = 0.0;
            for (int i = a; i < b; ++i)
                m = std::max(m, std::max(std::fabs(irL[i]), std::fabs(irR[i])));
            fEnvNext[j] = float(m);
            done += std::max(0, b - a) + 1;
        }
    }

    // the sum over the partitions of one IR, back in the time domain in dst
    void convSum(const std::vector<double>& X, const double* H, int np, double* dst)
    {
        double* tmp = fTmp.data();
        std::fill(dst, dst + kFS, 0.0);
        for (int p = 0; p < np; ++p)
        {
            int s = fSlot - p;
            if (s < 0)
                s += fPmax;
            std::copy(X.data() + size_t(s) * kFS, X.data() + size_t(s + 1) * kFS, tmp);
            eelfft::convolveC(tmp, H + size_t(p) * kFS, kM);
            for (int i = 0; i < kFS; ++i)
                dst[i] += tmp[i];
        }
        eelfft::ifft(dst, kM);
    }

    // conv_block: one block of overlap-save convolution for one channel (the input spectra of
    // the last P_max blocks stay in X, so a new IR carries the tail on); Ho = the old IR while
    // crossfading
    void convBlock(const std::vector<double>& inb, std::vector<double>& X, const double* H, const double* Ho,
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
        convSum(X, H, fP, acc);
        if (fXfOn)
        {
            double* acco = fAccO.data();
            convSum(X, Ho, fPo, acco);
            for (int i = 0; i < kB; ++i)
            {
                const double w = (i + 1.0) / kB;
                ola[size_t(i)] = acco[2 * (kB + i)] / kM * (1.0 - w) + acc[2 * (kB + i)] / kM * w;
            }
        }
        else
            for (int i = 0; i < kB; ++i)
                ola[size_t(i)] = acc[2 * (kB + i)] / kM;
    }

    double fSr = 48000.0;
    int fGen = 0, fPmax = 1, fIrLenMax = kB, fP = 1, fPo = 1, fPos = 0, fSlot = 0, fIrLen = 0;
    std::vector<double> fSetA[2], fSetB[2], fXl, fXr, fIr, fInL, fInR, fOlaL, fOlaR, fTmp, fAcc, fAccO;
    double* fH[2] = { nullptr, nullptr }; // the IR playing (one of the two sets)
    double* fG[2] = { nullptr, nullptr }; // the set a new IR is built into
    bool fXfOn = false;
    IrFile fFile;
    int fFileId = 0, fStFile = -1, fStLen = 0, fStNch = 1;
    double fStN = 0.0, fStRatio = 1.0;
    int fSrc = 2, fCurSrc = 0, fCurFile = 0;
    double fLength = 100.0, fPreDelay = 0.0, fLowCut = 80.0, fHighCut = 12000.0, fMixPct = 30.0, fOutDb = 0.0;
    double fCurLen = 0.0, fCurPd = 0.0;
    // the build in progress
    int fBst = 0, fBcnt = 0, fBsrc = 0, fBfile = 0, fBd = 0, fBc = 0, fBi = 0, fBn = 0, fBP = 1, fEnvJ = 0;
    double fBlen = 0.0, fBpd = 0.0, fBt60 = 1.0, fBbr = 0.0, fBer = 1.0, fBlp = 0.0, fBe = 0.0, fBgL = 0.0, fBgR = 0.0;
    double fLc = 0.0, fHc = 0.0, fMix = 0.3, fOut = 1.0;
    double fLc1 = 0.0, fLc2 = 0.0, fHc1 = 0.0, fLcr1 = 0.0, fLcr2 = 0.0, fHcr1 = 0.0;
    double fSeed = 12345.0;
    float fEnv[kEnvBins] = {}, fEnvNext[kEnvBins] = {};
};

} // namespace nullconv
