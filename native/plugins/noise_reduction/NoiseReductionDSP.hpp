/*
 * NULL JSFX - Noise Reduction, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/noise_reduction.jsfx, computed in double like EEL2 (FFTs with
 * EelFft.hpp, EEL2's fft/ifft/fft_permute semantics): every 512 samples a 2048-point Hann frame
 * of both channels is analysed; the noise estimate is the learnt profile (average magnitude per
 * bin while Learn is On) or, in Auto, a minimum-statistics tracker. The Wiener-like gain
 * 1 - (noise x Threshold)^2 / power (power smoothed over time and across frequency), floored by
 * Reduction, follows attack/release per bin, applied to both channels and overlap-added (Hann synthesis window).
 * The learnt profile survives activate(); a new sample rate maps it by frequency, like the
 * JSFX's @init. Latency: N - 1 samples.
 *
 * 4.2: the profile can come from a file (Noise File, Use Noise File) or from the saved state.
 * The file is read and analysed off the audio thread (NoiseFile.hpp, the JSFX's nf_analyse());
 * the results reach the DSP as a Profiles message, which process() applies at a block start
 * (takeProfiles(), then block(), the JSFX's @serialize read and @block), never allocating.
 */

#pragma once

#include "EelFft.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullnr {

static constexpr int kN = 2048, kH = kN / 4, kNB = kN / 2 + 1, kU = 4;
static constexpr double kBig = 1000000000000000000000000000000.0; // 1e30, as written in the JSFX

// what the interface draws: the spectrum, the noise estimate and the gain per bin
struct View {
    float spec[kNB] = {}, noise[kNB] = {}, gain[kNB] = {};
    std::atomic<float> sampleRate { 48000.0f };
    std::atomic<float> learnt { 0.0f }; // seconds of learnt profile
    std::atomic<int> mode { 0 };        // 0 = profile, 1 = learning, 2 = auto
    // 4.2: where the profile came from (0 none, 1 file, 2 Learn), the file cut at 60 s, the
    // file unreadable
    std::atomic<int> source { 0 };
    std::atomic<bool> cut { false }, fileFailed { false };
};

// the profile of a noise file (NoiseFile.hpp analyseFile()): summed magnitudes over `frames`
// frames, at `sampleRate`
struct FileProfile {
    bool ok = false, cut = false;
    double frames = 0.0, sampleRate = 0.0;
    double sum[kNB] = {};
};

// a profile as saved with the project (the JSFX's @serialize): average magnitudes in single
// precision, the source (1 file, 2 Learn), Use Noise File and the 60 s cut when it was saved
struct SavedProfile {
    bool ok = false, gate = false, cut = false;
    int src = 0;
    float sampleRate = 0.0f, length = 0.0f;
    float avg[kNB] = {};
};

// what the plugin hands the audio thread (NullMailbox.hpp; the newest wins, so it carries
// everything): the profile of the current noise file, and the saved profile; a sequence
// number that moved means a new file was chosen / a state was loaded
struct Profiles {
    uint32_t fileSeq = 0, savedSeq = 0;
    FileProfile file;
    SavedProfile saved;
};

// the profile in use, for the saved state: read by the plugin off the audio thread
// (a sequence lock: odd while the audio thread writes)
struct ProfileSnapshot {
    std::atomic<uint32_t> seq { 0 };
    std::atomic<int> src { 0 };
    std::atomic<float> sampleRate { 0.0f }, length { 0.0f };
    std::atomic<bool> gate { false }, cut { false };
    std::atomic<float> avg[kNB] = {};

    // the snapshot as a SavedProfile (ok = false when there is no profile)
    SavedProfile read() const
    {
        SavedProfile s;
        for (int tries = 0; tries < 1000; ++tries)
        {
            const uint32_t a = seq.load(std::memory_order_acquire);
            if (a & 1u)
                continue;
            s.src = src.load(std::memory_order_relaxed);
            s.sampleRate = sampleRate.load(std::memory_order_relaxed);
            s.length = length.load(std::memory_order_relaxed);
            s.gate = gate.load(std::memory_order_relaxed);
            s.cut = cut.load(std::memory_order_relaxed);
            for (int k = 0; k < kNB; ++k)
                s.avg[k] = avg[k].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq.load(std::memory_order_relaxed) == a)
                break;
        }
        s.ok = s.src >= 1 && s.src <= 2;
        return s;
    }
};

class NoiseReducer {
public:
    NoiseReducer()
        : fFftL(2 * kN), fFftR(2 * kN), fWin(kN), fInL(kN), fInR(kN), fOutL(kN), fOutR(kN), fPw(kNB), fGs(kNB),
          fGt(kNB), fAp(kNB), fAm(kNB), fAs(kU * kNB), fNz(kNB), fDsp(kNB), fPsum(kNB), fPtmp(kNB), fFileSum(kNB)
    {
    }

    void setView(View* v) noexcept { fView = v; }
    void setSnapshot(ProfileSnapshot* s) noexcept { fSnap = s; }

    static constexpr int latency() noexcept { return kN - 1; }

    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        for (std::vector<double>* v : { &fWin, &fInL, &fInR, &fOutL, &fOutR, &fPw, &fGs, &fGt, &fAp, &fAm, &fAs,
                                        &fNz, &fDsp })
            std::fill(v->begin(), v->end(), 0.0);
        for (int i = 0; i < kN; ++i)
            fWin[size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
        // the profile survives @init (playback start); a new sample rate maps it by frequency,
        // and a profile from a file is read from the file again
        if (!(fProfSr == sr))
        {
            if (fLcount > 0.0 && fProfSr > 0.0)
            {
                for (int k = 0; k < kNB; ++k)
                    fPtmp[size_t(k)] = fPsum[size_t(k)] / fLcount;
                map(fProfSr);
                if (fProfSrc == 1 && fGate)
                    fNfReq = true;
            }
            else
            {
                std::fill(fPsum.begin(), fPsum.end(), 0.0);
                fLcount = 0.0;
                fProfSrc = 0;
                fProfLen = 0.0;
            }
            fLearning = false;
            fProfSr = sr;
        }
        for (int i = 0; i < kNB; ++i)
        {
            fGs[size_t(i)] = 1.0;
            fAm[size_t(i)] = kBig;
        }
        std::fill(fAs.begin(), fAs.end(), kBig);
        fOscale = 2.0 / (3.0 * kN);
        fAa = std::exp(-kH / (0.05 * sr));
        fV = std::max(1.0, std::floor(0.6 * sr / kH + 0.5));
        fAbias = 2.0 * std::pow(sr / 48000.0, -0.28);
        fIpos = fOpos = fCnt = fFilled = 0;
        fAfc = 0.0;
        fAslot = 0;
        fAstarted = false;
        if (fView)
            fView->sampleRate.store(float(sr), std::memory_order_relaxed);
        publish();
    }

    // Use Noise File (slider11), read at the next block start like the JSFX's @block
    void setGate(bool on) noexcept { fGate = on; }

    // a Profiles message (audio thread, at a block start, before process()): a new file's
    // profile, and a loaded state (the JSFX's @serialize read), which wins over the file
    void takeProfiles(const Profiles& m) noexcept
    {
        std::copy(m.file.sum, m.file.sum + kNB, fFileSum.begin());
        fFileOk = m.file.ok;
        fFileCut = m.file.cut;
        fFileFrames = m.file.frames;
        fFileSr = m.file.sampleRate;
        if (m.fileSeq != fFileSeq)
        {
            fFileSeq = m.fileSeq;
            fFileChanged = true; // slider10 != nf_cur
        }
        if (m.savedSeq != fSavedSeq)
        {
            fSavedSeq = m.savedSeq;
            loadSaved(m.saved);
        }
    }

    // @slider (Threshold as the JSFX slider holds it, a multiple of 0.5)
    void setParams(bool learn, bool autoOn, double reductionDb, double thresholdDb, double smoothingPct,
                   double attackMs, double releaseMs, bool listenRemoved, double mixPct)
    {
        fLearn = learn;
        fAuto = autoOn;
        fGfloor = std::pow(10.0, -reductionDb / 20.0);
        fThr = std::pow(10.0, thresholdDb / 20.0);
        fTs = smoothingPct / 100.0 * 0.85;
        fFsm = smoothingPct / 100.0 * 0.8;
        fGa = attackMs > 0.0 ? std::exp(-kH / (attackMs / 1000.0 * fSr)) : 0.0;
        fGr = std::exp(-kH / (releaseMs / 1000.0 * fSr));
        fListen = listenRemoved;
        fMix = mixPct / 100.0;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        block();
        for (uint32_t n = 0; n < frames; ++n)
        {
            fInL[size_t(fIpos)] = inL[n];
            fInR[size_t(fIpos)] = inR[n];
            if (++fIpos >= kN)
                fIpos = 0;
            if (fFilled < kN)
                ++fFilled;

            if (++fCnt >= kH)
            {
                fCnt = 0;
                frame();
            }

            const double wl = fOutL[size_t(fOpos)], wr = fOutR[size_t(fOpos)];
            fOutL[size_t(fOpos)] = 0.0;
            fOutR[size_t(fOpos)] = 0.0;
            if (++fOpos >= kN)
                fOpos = 0;

            // the dry signal, delayed by N - 1 like the processed one
            const double dl = fInL[size_t(fIpos)], dr = fInR[size_t(fIpos)];
            outL[n] = static_cast<T>(dl * (1.0 - fMix) + wl * fMix);
            outR[n] = static_cast<T>(dr * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    // nr_map(): the profile at from_sr (average magnitudes in fPtmp) mapped to fSr by frequency,
    // linear between bins, the top bin held above its Nyquist
    void map(double fromSr) noexcept
    {
        for (int k = 0; k < kNB; ++k)
        {
            const double s = k * fSr / fromSr;
            const double j = std::floor(s);
            const double fr = s - j;
            fPsum[size_t(k)] = j >= kNB - 1 ? fPtmp[size_t(kNB - 1)]
                                            : fPtmp[size_t(j)] * (1.0 - fr) + fPtmp[size_t(j) + 1] * fr;
        }
        fLcount = 1.0;
    }

    // @serialize, reading: a saved profile replaces the one in use
    void loadSaved(const SavedProfile& s) noexcept
    {
        if (!s.ok || s.src < 1 || s.src > 2 || !(s.sampleRate > 0.0f))
            return;
        const double ssr = s.sampleRate;
        for (int k = 0; k < kNB; ++k)
            fPtmp[size_t(k)] = s.avg[k];
        if (std::fabs(ssr - fSr) < 0.5)
        {
            std::copy(fPtmp.begin(), fPtmp.end(), fPsum.begin());
            fLcount = 1.0;
        }
        else
            map(ssr);
        fProfSrc = s.src;
        fProfLen = s.length;
        fProfSr = fSr;
        fLearning = false;
        fFileChanged = false;
        fGateWas = s.gate;
        fNfCut = s.cut;
        fNfFail = false;
        // at another sample rate a profile from a file is read from the file again, if it can be
        fNfReq = std::fabs(ssr - fSr) >= 0.5 && s.src == 1 && s.gate;
        publish();
    }

    // @block: the noise file. A new file, or Use Noise File switched On without a profile from a
    // file, takes the file's profile; Use Noise File Off empties the profile.
    void block() noexcept
    {
        const bool on = fGate;
        bool changed = false;
        if (fFileChanged)
        {
            fFileChanged = false;
            if (on)
                fNfReq = true;
        }
        if (on && !fGateWas && fProfSrc != 1)
            fNfReq = true;
        if (!on && fGateWas)
        {
            std::fill(fPsum.begin(), fPsum.end(), 0.0);
            fLcount = 0.0;
            fProfSrc = 0;
            fProfLen = 0.0;
            fNfReq = false;
            fNfFail = false;
            fNfCut = false;
            changed = true;
        }
        fGateWas = on;
        if (fNfReq && on)
        {
            // the file's profile must be at this sample rate: else wait for it (the plugin reads
            // the file again after a sample rate change)
            if (fFileOk && !(fFileSr == fSr))
                return;
            fNfCut = false;
            fNfFail = true;
            if (fFileOk)
            {
                std::copy(fFileSum.begin(), fFileSum.end(), fPsum.begin());
                fLcount = fFileFrames;
                fProfSrc = 1;
                fProfLen = fFileFrames * kH / fSr;
                fNfFail = false;
                fNfCut = fFileCut;
            }
            changed = true;
        }
        fNfReq = false;
        if (changed)
            publish();
    }

    // the profile in use, for the saved state and the interface
    void publish() noexcept
    {
        if (fView)
        {
            fView->source.store(fLcount > 0.0 ? fProfSrc : 0, std::memory_order_relaxed);
            fView->cut.store(fNfCut, std::memory_order_relaxed);
            fView->fileFailed.store(fNfFail, std::memory_order_relaxed);
            fView->learnt.store(float(fProfLen), std::memory_order_relaxed);
        }
        if (!fSnap)
            return;
        const uint32_t s0 = fSnap->seq.load(std::memory_order_relaxed);
        fSnap->seq.store(s0 + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        const int src = fLcount > 0.0 && fProfSrc >= 1 && fProfSrc <= 2 ? fProfSrc : 0;
        fSnap->src.store(src, std::memory_order_relaxed);
        fSnap->sampleRate.store(float(fSr), std::memory_order_relaxed);
        fSnap->length.store(float(fProfLen), std::memory_order_relaxed);
        fSnap->gate.store(fGate, std::memory_order_relaxed);
        fSnap->cut.store(fNfCut, std::memory_order_relaxed);
        if (src > 0)
            for (int k = 0; k < kNB; ++k)
                fSnap->avg[k].store(float(fPsum[size_t(k)] / fLcount), std::memory_order_relaxed);
        fSnap->seq.store(s0 + 2, std::memory_order_release);
    }

    void frame()
    {
        double* bl = fFftL.data();
        double* br = fFftR.data();
        for (int i = 0; i < kN; ++i)
        {
            const int p = (fIpos + i) % kN;
            const double w = fWin[size_t(i)];
            bl[2 * i] = fInL[size_t(p)] * w;
            bl[2 * i + 1] = 0.0;
            br[2 * i] = fInR[size_t(p)] * w;
            br[2 * i + 1] = 0.0;
        }
        eelfft::fft(bl, kN);
        eelfft::fftPermute(bl, kN);
        eelfft::fft(br, kN);
        eelfft::fftPermute(br, kN);
        const bool ready = fFilled >= kN;
        // Learn switched on: start a new profile
        if (fLearn && !fLearning)
        {
            std::fill(fPsum.begin(), fPsum.end(), 0.0);
            fLcount = 0.0;
            fProfSrc = 2;
        }
        fLearning = fLearn;

        for (int k = 0; k < kNB; ++k)
        {
            const size_t K = size_t(k);
            const double rl = bl[2 * k], il = bl[2 * k + 1], rr = br[2 * k], ir = br[2 * k + 1];
            const double x = (std::sqrt(rl * rl + il * il) + std::sqrt(rr * rr + ir * ir)) * 0.5;
            const double x2 = x * x;
            fPw[K] = fPw[K] * fTs + x2 * (1.0 - fTs);
            if (ready)
            {
                if (fLearning)
                    fPsum[K] += x;
                fAp[K] = fAstarted ? fAp[K] * fAa + x2 * (1.0 - fAa) : x2;
                fAm[K] = std::min(fAm[K], fAp[K]);
            }
            fDsp[K] = x > fDsp[K] ? x : fDsp[K] * 0.7 + x * 0.3;
        }
        if (ready)
        {
            if (fLearning)
            {
                fLcount += 1.0;
                fProfLen = fLcount * kH / fSr;
            }
            fAstarted = true;
            fAfc += 1.0;
            if (fAfc >= fV)
            {
                fAfc = 0.0;
                std::copy(fAm.begin(), fAm.end(), fAs.begin() + fAslot * kNB);
                fAslot = (fAslot + 1) % kU;
                std::copy(fAp.begin(), fAp.end(), fAm.begin());
            }
        }

        // the power smoothed across frequency too: one-pole forward, then backward (zero phase);
        // random noise peaks spread below the threshold, strong partials stay above it
        double t = fPw[0];
        for (int k = 0; k < kNB; ++k)
        {
            t = t * fFsm + fPw[size_t(k)] * (1.0 - fFsm);
            fGt[size_t(k)] = t;
        }
        t = fGt[size_t(kNB - 1)];
        for (int k = kNB - 1; k >= 0; --k)
        {
            t = t * fFsm + fGt[size_t(k)] * (1.0 - fFsm);
            fGt[size_t(k)] = t;
        }

        // noise estimate and the raw Wiener-like gain, floored
        for (int k = 0; k < kNB; ++k)
        {
            const size_t K = size_t(k);
            double nk;
            if (fAuto)
            {
                double m = fAm[K];
                for (int j = 0; j < kU; ++j)
                    m = std::min(m, fAs[size_t(j * kNB + k)]);
                nk = m < kBig ? std::sqrt(m * fAbias) : 0.0;
            }
            else
                nk = fLcount > 0.0 ? fPsum[K] / fLcount : 0.0;
            fNz[K] = nk;
            double tn = nk * fThr;
            tn *= tn;
            const double g = tn > 0.0 ? (fGt[K] > tn ? 1.0 - tn / fGt[K] : 0.0) : 1.0;
            fGt[K] = std::max(fGfloor, g);
        }
        // attack (gain opening) and release (gain closing) per bin, then apply
        for (int k = 0; k < kNB; ++k)
        {
            const size_t K = size_t(k);
            const double g0 = fGt[K];
            double s = fGs[K];
            s = g0 > s ? s * fGa + g0 * (1.0 - fGa) : s * fGr + g0 * (1.0 - fGr);
            fGs[K] = s;
            const double g = fListen ? 1.0 - s : s;
            bl[2 * k] *= g;
            bl[2 * k + 1] *= g;
            br[2 * k] *= g;
            br[2 * k + 1] *= g;
            if (k > 0 && k < kN / 2)
            {
                bl[2 * (kN - k)] *= g;
                bl[2 * (kN - k) + 1] *= g;
                br[2 * (kN - k)] *= g;
                br[2 * (kN - k) + 1] *= g;
            }
        }
        eelfft::fftIpermute(bl, kN);
        eelfft::ifft(bl, kN);
        eelfft::fftIpermute(br, kN);
        eelfft::ifft(br, kN);
        for (int i = 0; i < kN; ++i)
        {
            const size_t p = size_t((fOpos + i) % kN);
            const double w = fWin[size_t(i)] * fOscale;
            fOutL[p] += bl[2 * i] * w;
            fOutR[p] += br[2 * i] * w;
        }

        if (fView)
        {
            for (int k = 0; k < kNB; ++k)
            {
                fView->spec[k] = float(fDsp[size_t(k)]);
                fView->noise[k] = float(fNz[size_t(k)]);
                fView->gain[k] = float(fGs[size_t(k)]);
            }
            fView->mode.store(fAuto ? 2 : fLearning ? 1 : 0, std::memory_order_relaxed);
        }
        if (fLearning)
            publish();
    }

    View* fView = nullptr;
    ProfileSnapshot* fSnap = nullptr;
    double fSr = 48000.0, fProfSr = 0.0;
    std::vector<double> fFftL, fFftR, fWin, fInL, fInR, fOutL, fOutR, fPw, fGs, fGt, fAp, fAm, fAs, fNz, fDsp, fPsum;
    // 4.2: the profile's source (0 none, 1 file, 2 Learn) and length (s), the noise file's
    // profile and its state (nf_req, nf_was, nf_fail, nf_cut of the JSFX)
    std::vector<double> fPtmp, fFileSum;
    int fProfSrc = 0;
    double fProfLen = 0.0, fFileFrames = 0.0, fFileSr = 0.0;
    uint32_t fFileSeq = 0, fSavedSeq = 0;
    bool fGate = false, fGateWas = false, fNfReq = false, fNfFail = false, fNfCut = false;
    bool fFileOk = false, fFileCut = false, fFileChanged = false;
    int fIpos = 0, fOpos = 0, fCnt = 0, fFilled = 0, fAslot = 0;
    double fAfc = 0.0, fV = 1.0, fAbias = 2.0, fLcount = 0.0, fOscale = 0.0, fAa = 0.0;
    bool fAstarted = false, fLearning = false;
    bool fLearn = false, fAuto = false, fListen = false;
    double fGfloor = 1.0, fThr = 1.0, fTs = 0.0, fFsm = 0.0, fGa = 0.0, fGr = 0.0, fMix = 1.0;
};

} // namespace nullnr
