/*
 * NULL JSFX - Glitch FX, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/glitch_fx.jsfx, computed in double like EEL2. Every 1/Rate
 * seconds an "event" grabs the latest slice of audio (Stutter Length, a 1/x note at the host
 * tempo) and loops it; Gate chops it, Pitch Shift replays it faster or slower, Reverse plays
 * some events backwards, Tape Stop slows some events to a halt and Granular loops a Hann
 * grain. Without Stutter the effects act on the live signal. The dice use the same exact
 * Park-Miller generator as the JSFX (prand), so both give the same output.
 * Trigger MIDI: held notes C3..F3 (60..65) play Stutter, Gate, Pitch Shift, Reverse, Tape Stop
 * and Granular, each note-on starts an event, the latest note's velocity scales the Mix.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullglitch {

struct Params {
    double rate = 4.0;      // events per second
    double lengthDiv = 8.0; // stutter slice = a 1/x note
    bool stutter = true;
    double gateDepth = 30.0;
    bool gate = false;
    double pitchSt = 0.0;
    bool pitch = false;
    double revChance = 0.0;
    bool reverse = false;
    double tapeMs = 300.0;
    bool tape = false;
    double grainMs = 200.0;
    bool granular = false;
    double mixPct = 50.0;
    bool midi = false; // Trigger MIDI
};

// one MIDI message at its frame in the block
struct MidiMsg {
    uint32_t frame;
    uint8_t status, data1, data2;
};

// EEL2's x % y: both sides made positive and truncated to integers
inline double eelMod(double a, double b) noexcept
{
    const uint64_t ia = uint64_t(std::fabs(a)), ib = uint64_t(std::fabs(b));
    return ib ? double(ia % ib) : 0.0;
}

// The display (glitch_fx_gfx.jsfx draws the same): the level of the wet signal at time t
// (seconds) for the settings, ignoring the random Reverse and Tape Stop. Also gives the
// playhead inside the loop (0..1), or -1 when Stutter is off.
inline double displayLevel(const Params& p, double bpm, double t, double* head) noexcept
{
    const double evl = 1.0 / p.rate;
    const double sl = std::min(2.0, 240.0 / bpm / p.lengthDiv);
    const double ll = p.granular ? std::min(p.grainMs / 1000.0, sl) : sl;
    const double te = t - std::floor(t / evl) * evl;
    double env = 1.0;
    *head = -1.0;
    if (p.stutter)
    {
        double rp = te * (p.pitch ? std::pow(2.0, p.pitchSt / 12.0) : 1.0);
        rp -= std::floor(rp / ll) * ll;
        const double ph = rp / ll;
        env = p.granular ? 0.5 - 0.5 * std::cos(2.0 * M_PI * ph) : std::min(1.0, std::min(ph, 1.0 - ph) * ll / 0.003);
        *head = ph;
    }
    if (p.gate)
    {
        const double per = sl / 2.0;
        if (te - std::floor(te / per) * per > sl / 4.0)
            env *= 1.0 - p.gateDepth / 100.0;
    }
    return env;
}

class GlitchProcessor {
public:
    // @init: a 4 s stereo buffer, cleared, and the generator reseeded
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = std::ceil(sr * 4.0);
        fL.assign(size_t(fSize), 0.0);
        fR.assign(size_t(fSize), 0.0);
        fW = 0;
        fEvCnt = 1000000000.0;
        fSliceStart = 0.0;
        fRead = 0.0;
        fSpeed = 1.0;
        fDir = 1.0;
        fTape = 0.0;
        fRRead = 0.0;
        fG1ph = 0.0; fG2ph = 0.5;
        fG1st = fG2st = 0.0;
        fSeed = 12345.0;
        for (int k = 0; k < 8; ++k)
            fHeld[k] = fHVel[k] = 0.0;
        fStamp = 0.0;
        fMNew = false;
        fMTgt = 0.0;
        fMAmt = 0.0;
        fMK = 1.0 - std::exp(-1.0 / (sr * 0.005));
        updateParams();
    }

    // the effects the held MIDI notes play (bit k = note 60 + k), for the display
    int heldMask() const noexcept
    {
        int m = 0;
        for (int k = 0; k < 6; ++k)
            if (fHeld[k] > 0.0)
                m |= 1 << k;
        return m;
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, double tempo,
                 const MidiMsg* midi = nullptr, uint32_t midiCount = 0)
    {
        uint32_t mi = 0;
        for (uint32_t i = 0; i < frames; ++i)
        {
            // MIDI notes due now: C3..F3 held / released, all notes off
            for (; mi < midiCount && midi[mi].frame <= i; ++mi)
                applyMidi(midi[mi]);
            // the switches: the sliders, or under Trigger MIDI the held notes
            bool swSt, swGate, swPitch = false, swRev = false, swTape = false, swGran;
            if (fP.midi)
            {
                swSt = fHeld[0] > 0.0; swGate = fHeld[1] > 0.0; swPitch = fHeld[2] > 0.0;
                swRev = fHeld[3] > 0.0; swTape = fHeld[4] > 0.0; swGran = fHeld[5] > 0.0;
                fMAmt += (fMTgt - fMAmt) * fMK;
            }
            else
            {
                swSt = fP.stutter; swGate = fP.gate; swGran = fP.granular;
            }

            const double s0 = inL[i], s1 = inR[i];
            fL[size_t(fW)] = s0;
            fR[size_t(fW)] = s1;

            const double bpm = tempo > 0.0 ? tempo : 120.0;
            const double sliceLen = std::max(16.0, std::min(fSize / 2.0, std::floor(fSr * 240.0 / bpm / fP.lengthDiv)));
            const double loopLen = swGran ? std::min(fGrainLen, sliceLen) : sliceLen;

            fEvCnt += 1.0;
            if (fP.midi)
            {
                // a note-on starts a new event; Reverse and Tape Stop follow their notes
                if (fMNew)
                {
                    fMNew = false;
                    fEvCnt = 0.0;
                    fSliceStart = fW - sliceLen;
                    fRead = 0.0;
                    fRRead = 0.0;
                }
                fDir = swRev ? -1.0 : 1.0;
                fTape = swTape ? 1.0 : 0.0;
            }
            else if (fEvCnt >= fEvLen)
            {
                fEvCnt = 0.0;
                fSliceStart = fW - sliceLen;
                fRead = 0.0;
                fRRead = 0.0;
                fDir = (fP.reverse && prand(100.0) < fRevChance) ? -1.0 : 1.0;
                fTape = (fP.tape && prand(100.0) < 30.0) ? 1.0 : 0.0;
            }

            fSpeed = fP.midi ? (swPitch ? fPitchM : 1.0) : fPitch;
            if (fTape != 0.0)
                fSpeed *= std::max(0.0, 1.0 - fEvCnt / fTapeLen);

            double wl, wr;
            if (swSt)
            {
                fRead += fSpeed;
                if (fRead >= loopLen)
                    fRead -= loopLen;
                const double pos = fDir > 0.0 ? fSliceStart + fRead : fSliceStart + loopLen - 1.0 - fRead;
                wl = rd(fL, pos);
                wr = rd(fR, pos);
                const double ph = fRead / loopLen;
                const double win = swGran ? 0.5 - 0.5 * std::cos(2.0 * M_PI * ph)
                                               : std::min(1.0, std::min(ph, 1.0 - ph) * loopLen / (0.003 * fSr));
                wl *= win;
                wr *= win;
            }
            else if (swGran)
            {
                const double gstep = 1.0 / fGrainLen;
                fG1ph += gstep;
                if (fG1ph >= 1.0)
                {
                    fG1ph -= 1.0;
                    fG1st = fW - fGrainLen * (std::max(fSpeed, 1.0) + 1.0) - prand(fGrainLen);
                }
                fG2ph += gstep;
                if (fG2ph >= 1.0)
                {
                    fG2ph -= 1.0;
                    fG2st = fW - fGrainLen * (std::max(fSpeed, 1.0) + 1.0) - prand(fGrainLen);
                }
                const double gp1 = fDir > 0.0 ? fG1ph : 1.0 - fG1ph;
                const double gp2 = fDir > 0.0 ? fG2ph : 1.0 - fG2ph;
                const double gw1 = 0.5 - 0.5 * std::cos(2.0 * M_PI * fG1ph);
                const double gw2 = 0.5 - 0.5 * std::cos(2.0 * M_PI * fG2ph);
                const double p1 = fG1st + gp1 * fGrainLen * fSpeed, p2 = fG2st + gp2 * fGrainLen * fSpeed;
                wl = rd(fL, p1) * gw1 + rd(fL, p2) * gw2;
                wr = rd(fR, p1) * gw1 + rd(fR, p2) * gw2;
            }
            else if (fDir < 0.0)
            {
                fRRead += fSpeed;
                if (fRRead >= sliceLen)
                {
                    fRRead -= sliceLen;
                    fSliceStart = fW - sliceLen;
                }
                const double pos = fSliceStart + sliceLen - 1.0 - fRRead;
                wl = rd(fL, pos);
                wr = rd(fR, pos);
                const double ph = fRRead / sliceLen;
                const double win = std::min(1.0, std::min(ph, 1.0 - ph) * sliceLen / (0.003 * fSr));
                wl *= win;
                wr *= win;
            }
            else
            {
                fRead += 1.0 - fSpeed;
                if (fRead < 0.0)
                    fRead += 0.05 * fSr;
                if (fRead >= 0.05 * fSr)
                    fRead -= 0.05 * fSr;
                wl = rd(fL, fW - 1.0 - fRead);
                wr = rd(fR, fW - 1.0 - fRead);
            }

            if (swGate)
            {
                if (eelMod(fEvCnt, std::max(16.0, sliceLen / 2.0)) > sliceLen / 4.0)
                {
                    wl *= 1.0 - fGateDepth;
                    wr *= 1.0 - fGateDepth;
                }
            }

            fW += 1.0;
            if (fW >= fSize)
                fW = 0.0;

            const double mx = fP.midi ? fMix * fMAmt : fMix;
            outL[i] = static_cast<T>(s0 * (1.0 - mx) + wl * mx);
            outR[i] = static_cast<T>(s1 * (1.0 - mx) + wr * mx);
        }
    }

private:
    void updateParams()
    {
        fEvLen = std::max(1.0, std::floor(fSr / fP.rate));
        fPitch = fP.pitch ? std::pow(2.0, fP.pitchSt / 12.0) : 1.0;
        fGateDepth = fP.gateDepth / 100.0;
        fRevChance = fP.revChance;
        fTapeLen = std::max(1.0, fSr * fP.tapeMs / 1000.0);
        fGrainLen = std::max(16.0, std::floor(fSr * fP.grainMs / 1000.0));
        fMix = fP.mixPct / 100.0;
        fPitchM = std::pow(2.0, fP.pitchSt / 12.0);
    }

    void applyMidi(const MidiMsg& m) noexcept
    {
        const int st = m.status & 0xF0, d = m.data1 & 0x7F, n = d - 60;
        if (st == 0x90 && m.data2 > 0 && n >= 0 && n < 6)
        {
            fStamp += 1.0;
            fHeld[n] = fStamp;
            fHVel[n] = double(m.data2);
            fMNew = true;
        }
        else if ((st == 0x80 || st == 0x90) && n >= 0 && n < 6)
            fHeld[n] = 0.0;
        else if (st == 0xB0 && (d == 120 || d == 123))
            for (int k = 0; k < 8; ++k)
                fHeld[k] = 0.0;
        // the latest held note's velocity sets the amount
        fMTgt = 0.0;
        double b = 0.0;
        for (int k = 0; k < 6; ++k)
            if (fHeld[k] > b)
            {
                b = fHeld[k];
                fMTgt = fHVel[k] / 127.0;
            }
    }

    // random value in [0, x): Park-Miller in exact doubles, as prand() in the JSFX
    double prand(double x) noexcept
    {
        fSeed *= 16807.0;
        fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
        return fSeed / 2147483648.0 * x;
    }

    // the JSFX's rd(): linear interpolation with one wrap each way
    double rd(const std::vector<double>& buf, double p) const noexcept
    {
        if (p < 0.0)
            p += fSize;
        if (p >= fSize)
            p -= fSize;
        const double i = std::floor(p);
        const double f = p - i;
        double i2 = i + 1.0;
        if (i2 >= fSize)
            i2 = 0.0;
        const double a = buf[size_t(i)];
        return a + (buf[size_t(i2)] - a) * f;
    }

    Params fP;
    double fSr = 48000.0, fSize = 1.0, fW = 0.0;
    std::vector<double> fL, fR;
    double fEvCnt = 0.0, fSliceStart = 0.0, fRead = 0.0, fSpeed = 1.0, fDir = 1.0, fTape = 0.0, fRRead = 0.0;
    double fG1ph = 0.0, fG2ph = 0.5, fG1st = 0.0, fG2st = 0.0, fSeed = 12345.0;
    double fHeld[8] = {}, fHVel[8] = {}, fStamp = 0.0, fMTgt = 0.0, fMAmt = 0.0, fMK = 0.0, fPitchM = 1.0;
    bool fMNew = false;
    double fEvLen = 1.0, fPitch = 1.0, fGateDepth = 0.0, fRevChance = 0.0, fTapeLen = 1.0, fGrainLen = 16.0, fMix = 0.5;
};

} // namespace nullglitch
