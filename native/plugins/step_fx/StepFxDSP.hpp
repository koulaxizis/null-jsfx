/*
 * NULL JSFX - Step FX, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/step_fx.jsfx, computed in double like EEL2, in the same order.
 * A 16-step sequencer that plays one effect per step (Off, Repeat, Reverse, Filter, Gate, Tape
 * Stop, Pitch Down, Pitch Up, Crush). One step = Step Length at the host tempo, locked to the
 * host's beat position while the transport plays (re-read every block, as the JSFX's @block
 * does), free-running at the tempo (120 bpm without one) when stopped. Swing lengthens every
 * even step by sw and shortens the odd one. MIDI Trigger: held notes C3..G3 pick the effect.
 * Two voices crossfade every change of effect over 4 ms; Filter and Crush run after them.
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nullstepfx {

enum Effect { kOff, kRepeat, kReverse, kFilter, kGate, kTapeStop, kPitchDown, kPitchUp, kCrush, kNumEffects };

struct Params {
    int steps[16] = {};
    int stepLength = 1;    // 0..4: 1/8, 1/16, 1/32, 1/8T, 1/16T
    int numSteps = 16;
    double swing = 0.0;    // %
    int repeatSize = 1;    // 0..2: 1/16, 1/32, 1/64
    double filterHz = 800.0;
    double crush = 60.0;   // %
    double pitch = 12.0;   // semitones
    double mix = 100.0;    // %
    bool midi = false;
};

// steps per beat for a Step Length option (as the JSFX's spb)
inline double stepsPerBeat(int sel) noexcept
{
    return sel == 0 ? 2.0 : sel == 1 ? 4.0 : sel == 2 ? 8.0 : sel == 3 ? 3.0 : 6.0;
}

// a MIDI message at its frame in the block
struct MidiMsg {
    uint32_t frame;
    uint8_t status, data1, data2;
};

class StepFxProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fBufSize = int(std::ceil(sr * 4.0));
        fBufL.assign(size_t(fBufSize), 0.0);
        fBufR.assign(size_t(fBufSize), 0.0);
        std::memset(fHeld, 0, sizeof(fHeld));
        fWpos = 0;
        fStepPos = 0.0;
        fLastK = -1.0;
        fCurE = 0;
        fCurStep = 0;
        fVc = 0;
        fV[0] = Voice();
        fV[0].vg = 1.0;
        fV[1] = Voice();
        fStamp = 0.0;
        fMidiE = 0;
        fFw = std::fmax(1.0, std::floor(0.002 * sr));
        fXinc = 1.0 / std::fmax(1.0, std::floor(0.004 * sr));
        fWin = std::floor(sr * 0.06);
        fFTop = std::fmin(16000.0, sr * 0.4);
        fFLf = std::log(fFTop);
        fFA = 1.0 - std::exp(-1.0 / (0.015 * sr));
        fFG = 0.0;
        fFl = Svf();
        fFr = Svf();
        fCG = 0.0;
        fCPh = 1.0;
        fCHl = fCHr = 0.0;
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    int currentStep() const noexcept { return fCurStep; }  // -1 under MIDI Trigger
    int midiEffect() const noexcept { return fMidiE; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo, bool playing,
                 double beatPos, const MidiMsg* midi = nullptr, uint32_t midiCount = 0)
    {
        // @block
        const double bpm = tempo > 0.0 ? tempo : 120.0;
        const double stepInc = bpm / 60.0 * fSpb / fSr;
        const double stepN = fSr * 60.0 / bpm / fSpb;
        const double sliceN = std::fmax(16.0, std::floor(fSr * 60.0 / bpm * fRepBeats));
        if (playing)
            fStepPos = beatPos * fSpb;
        uint32_t mi = 0;
        const uint32_t mn = midiCount < 256 ? midiCount : 256;

        for (uint32_t i = 0; i < frames; ++i)
        {
            const double il = inL[i], ir = inR[i];
            fBufL[size_t(fWpos)] = il;
            fBufR[size_t(fWpos)] = ir;

            // MIDI notes due now
            while (mi < mn && midi[mi].frame <= i)
            {
                const int st = midi[mi].status & 0xF0, n = midi[mi].data1, v = midi[mi].data2;
                if (st == 0x90 && v > 0 && n >= 60 && n <= 67)
                {
                    fStamp += 1.0;
                    fHeld[n - 60] = fStamp;
                }
                else if ((st == 0x80 || st == 0x90) && n >= 60 && n <= 67)
                    fHeld[n - 60] = 0.0;
                else if (st == 0xB0 && (n == 120 || n == 123))
                    std::memset(fHeld, 0, sizeof(fHeld));
                fMidiE = 0;
                double b = 0.0;
                for (int k = 0; k < 8; ++k)
                    if (fHeld[k] > b)
                    {
                        b = fHeld[k];
                        fMidiE = k + 1;
                    }
                ++mi;
            }

            // the step, in swing pairs
            fStepPos += stepInc;
            const double wLen = 2.0 * fNSteps;
            if (fStepPos >= wLen)
                fStepPos -= wLen;
            const double pw = fStepPos - wLen * std::floor(fStepPos / wLen);
            const double pr = std::floor(pw / 2.0);
            const double pq = pw - 2.0 * pr;
            double k, seg;
            if (pq < 1.0 + fSw)
            {
                k = 2.0 * pr;
                seg = stepN * (1.0 + fSw);
            }
            else
            {
                k = 2.0 * pr + 1.0;
                seg = stepN * (1.0 - fSw);
            }
            int ce;
            bool trig;
            if (fP.midi)
            {
                ce = fMidiE;
                fCurStep = -1;
                seg = stepN;
                trig = ce != fCurE;
            }
            else
            {
                const double cs = k - fNSteps * std::floor(k / fNSteps);
                fCurStep = int(cs + 0.0001);
                ce = fP.steps[fCurStep & 15];
                trig = ce != fCurE ||
                       (k != fLastK && (ce == kRepeat || ce == kReverse || ce == kGate || ce == kTapeStop));
            }
            fLastK = k;

            if (trig)
            {
                seg = std::fmax(16.0, std::fmin(std::floor(fBufSize / 2.0), std::floor(seg)));
                fVc = 1 - fVc;
                start(fV[fVc], ce, seg, sliceN);
                fV[fVc].vg = 0.0;
                fCurE = ce;
            }

            // the two voices, crossfaded
            fV[0].vg = fVc == 0 ? std::fmin(1.0, fV[0].vg + fXinc) : std::fmax(0.0, fV[0].vg - fXinc);
            fV[1].vg = fVc == 1 ? std::fmin(1.0, fV[1].vg + fXinc) : std::fmax(0.0, fV[1].vg - fXinc);
            double wl = 0.0, wr = 0.0, vl, vr;
            if (fV[0].vg > 0.0)
            {
                run(fV[0], il, ir, vl, vr);
                wl += vl * fV[0].vg;
                wr += vr * fV[0].vg;
            }
            if (fV[1].vg > 0.0)
            {
                run(fV[1], il, ir, vl, vr);
                wl += vl * fV[1].vg;
                wr += vr * fV[1].vg;
            }

            // Filter
            fFG = fCurE == kFilter ? std::fmin(1.0, fFG + fXinc) : std::fmax(0.0, fFG - fXinc / 8.0);
            if (fFG > 0.0)
            {
                fFLf += ((fCurE == kFilter ? fFLc : std::log(fFTop)) - fFLf) * fFA;
                const double g = std::tan(kPi * std::exp(fFLf) / fSr);
                fA1 = 1.0 / (1.0 + g * (g + 0.3));
                fA2 = g * fA1;
                fA3 = g * fA2;
                const double yl = svf(fFl, wl), yr = svf(fFr, wr);
                wl += (yl - wl) * fFG;
                wr += (yr - wr) * fFG;
            }
            else
            {
                fFLf = std::log(fFTop);
                fFl = Svf();
                fFr = Svf();
            }

            // Crush
            fCG = fCurE == kCrush ? std::fmin(1.0, fCG + fXinc) : std::fmax(0.0, fCG - fXinc);
            if (fCG > 0.0)
            {
                fCPh += fCInc;
                if (fCPh >= 1.0)
                {
                    fCPh -= 1.0;
                    fCHl = std::floor(wl / fCQ + 0.5) * fCQ;
                    fCHr = std::floor(wr / fCQ + 0.5) * fCQ;
                }
                wl += (fCHl - wl) * fCG;
                wr += (fCHr - wr) * fCG;
            }
            else
                fCPh = 1.0;

            fWpos += 1;
            if (fWpos >= fBufSize)
                fWpos = 0;

            outL[i] = static_cast<T>(il * (1.0 - fMix) + wl * fMix);
            outR[i] = static_cast<T>(ir * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    static constexpr double kPi = 3.141592653589793;

    struct Voice {
        int e = 0;
        double el = 0, sn = 0, sl = 0, acc = 0, ph = 0, gg = 1, vg = 0;
        int s0 = 0;
    };
    struct Svf {
        double ic1 = 0, ic2 = 0;
    };

    void start(Voice& v, int code, double n, double slice) const
    {
        v.e = code;
        v.el = 0.0;
        v.sn = n;
        v.sl = slice;
        v.s0 = fWpos;
        v.acc = 0.0;
        v.ph = 0.0;
        v.gg = 1.0;
    }

    double rd(const std::vector<double>& buf, double p) const
    {
        if (p < 0.0)
            p += fBufSize;
        if (p >= fBufSize)
            p -= fBufSize;
        const double fi = std::floor(p);
        const double f = p - fi;
        int i = int(fi), i2 = i + 1;
        if (i2 >= fBufSize)
            i2 = 0;
        return buf[size_t(i)] + (buf[size_t(i2)] - buf[size_t(i)]) * f;
    }

    double svf(Svf& s, double x) const
    {
        const double v3 = x - s.ic2;
        const double v1 = fA1 * s.ic1 + fA2 * v3;
        const double v2 = s.ic2 + fA2 * s.ic1 + fA3 * v3;
        s.ic1 = 2.0 * v1 - s.ic1;
        s.ic2 = 2.0 * v2 - s.ic2;
        return v2;
    }

    // one sample of a voice (the JSFX's v_run)
    void run(Voice& v, double il, double ir, double& ol, double& orr)
    {
        switch (v.e)
        {
        case kRepeat: {
            const double m = v.el - v.sl * std::floor(v.el / v.sl);
            double w = std::fmin(1.0, (v.sl - m) / fFw);
            if (v.el >= v.sl)
                w = std::fmin(w, (m + 1.0) / fFw);
            int pos = v.s0 + int(m);
            if (pos >= fBufSize)
                pos -= fBufSize;
            ol = fBufL[size_t(pos)] * w;
            orr = fBufR[size_t(pos)] * w;
            break;
        }
        case kReverse: {
            if (v.el >= v.sn)
            {
                v.s0 = fWpos;
                v.el = 0.0;
            }
            int pos = v.s0 - 1 - int(v.el);
            if (pos < 0)
                pos += fBufSize;
            const double w = std::fmin(1.0, std::fmin(v.el + 1.0, v.sn - v.el) / fFw);
            ol = fBufL[size_t(pos)] * w;
            orr = fBufR[size_t(pos)] * w;
            break;
        }
        case kGate: {
            const double m = v.el - v.sl * std::floor(v.el / v.sl);
            v.gg = m < v.sl / 2.0 ? std::fmin(1.0, v.gg + 1.0 / fFw) : std::fmax(0.0, v.gg - 1.0 / fFw);
            ol = il * v.gg;
            orr = ir * v.gg;
            break;
        }
        case kTapeStop: {
            const double sp = std::fmax(0.0, 1.0 - v.el / v.sn);
            const double pos = v.s0 + v.acc;
            v.acc += sp;
            const double w = std::fmin(1.0, sp * 4.0);
            ol = rd(fBufL, pos) * w;
            orr = rd(fBufR, pos) * w;
            break;
        }
        case kPitchDown:
        case kPitchUp: {
            v.ph += v.e == kPitchDown ? fIncDn : fIncUp;
            if (v.ph >= 1.0)
                v.ph -= 1.0;
            if (v.ph < 0.0)
                v.ph += 1.0;
            double ph2 = v.ph + 0.5;
            if (ph2 >= 1.0)
                ph2 -= 1.0;
            const double d1 = 1.0 + v.ph * fWin, d2 = 1.0 + ph2 * fWin;
            const double g1 = 0.5 - 0.5 * std::cos(2.0 * kPi * v.ph);
            ol = rd(fBufL, fWpos - d1) * g1 + rd(fBufL, fWpos - d2) * (1.0 - g1);
            orr = rd(fBufR, fWpos - d1) * g1 + rd(fBufR, fWpos - d2) * (1.0 - g1);
            break;
        }
        default:
            ol = il;
            orr = ir;
            break;
        }
        v.el += 1.0;
    }

    void updateParams()
    {
        fSpb = stepsPerBeat(fP.stepLength);
        fNSteps = std::fmax(1.0, std::fmin(16.0, double(fP.numSteps)));
        fSw = fP.swing / 150.0; // as the JSFX: 2/3 of a step at 100%
        fRepBeats = fP.repeatSize == 0 ? 0.25 : fP.repeatSize == 1 ? 0.125 : 0.0625;
        fFLc = std::log(std::fmin(fP.filterHz, fFTop)); // below Nyquist at any sample rate
        const double crush = fP.crush / 100.0;
        fCQ = std::pow(2.0, 1.0 - (16.0 - 12.0 * crush));
        fCInc = 1.0 / (1.0 + 15.0 * crush);
        fIncDn = (1.0 - std::pow(2.0, -fP.pitch / 12.0)) / fWin;
        fIncUp = (1.0 - std::pow(2.0, fP.pitch / 12.0)) / fWin;
        fMix = fP.mix / 100.0;
    }

    Params fP;
    double fSr = 48000.0;
    int fBufSize = 1;
    std::vector<double> fBufL, fBufR;
    int fWpos = 0;
    double fStepPos = 0.0, fLastK = -1.0;
    int fCurE = 0, fCurStep = 0, fVc = 0;
    Voice fV[2];
    double fHeld[8] = {};
    double fStamp = 0.0;
    int fMidiE = 0;
    double fFw = 1.0, fXinc = 1.0, fWin = 1.0;
    double fFTop = 16000.0, fFLf = 0.0, fFA = 0.0, fFG = 0.0, fFLc = 0.0;
    double fA1 = 0.0, fA2 = 0.0, fA3 = 0.0;
    Svf fFl, fFr;
    double fCG = 0.0, fCPh = 1.0, fCHl = 0.0, fCHr = 0.0, fCQ = 1.0, fCInc = 1.0;
    double fSpb = 4.0, fNSteps = 16.0, fSw = 0.0, fRepBeats = 0.125, fIncDn = 0.0, fIncUp = 0.0, fMix = 1.0;
};

} // namespace nullstepfx
