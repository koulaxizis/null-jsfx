/*
 * NULL JSFX - Rotary Speaker, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/rotary_speaker.jsfx, computed in double like EEL2: a
 * Leslie-style speaker. The mono sum goes through a tube-style soft clip (Drive), splits at
 * 800 Hz into a spinning horn (highs) and drum (lows); each rotor changes level and pitch
 * (Doppler) as it turns and two virtual microphones pick it up. The rotors follow Speed with
 * inertia (horn ~0.7 s, drum ~3 s, scaled by Acceleration).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nullrotary {

struct Params {
    int speed = 0; // 0 slow (chorale), 1 fast (tremolo), 2 stop
    double accelPct = 100.0, balancePct = 50.0, drivePct = 20.0, widthPct = 80.0, mixPct = 100.0;
};

// the amp's soft clip as the JSFX computes it, for drive = 1 + Drive% / 100 * 6 (display too)
inline double softClip(double x, double drive) noexcept
{
    x = std::max(-3.0, std::min(3.0, x));
    return x * (27.0 + x * x) / (27.0 + 9.0 * x * x) / std::sqrt(drive);
}

class RotaryProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        fSize = std::ceil(sr * 0.02);
        fHL.assign(size_t(fSize), 0.0);
        fHR.assign(size_t(fSize), 0.0);
        fDL.assign(size_t(fSize), 0.0);
        fDR.assign(size_t(fSize), 0.0);
        fW = 0.0;
        fHornPh = 0.0; fDrumPh = 0.25;
        fHornF = 0.8; fDrumF = 0.7;
        fLp1 = fLp2 = 0.0;
        updateParams();
    }

    // @slider
    void setParams(const Params& p)
    {
        fP = p;
        updateParams();
    }

    // rotor state for the display: phases 0..1 and speeds in Hz
    double hornPhase() const noexcept { return fHornPh; }
    double drumPhase() const noexcept { return fDrumPh; }
    double hornHz() const noexcept { return fHornF; }
    double drumHz() const noexcept { return fDrumF; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double s0 = inL[i], s1 = inR[i];
            double x = (s0 + s1) * 0.5 * fDrive;
            x = std::max(-3.0, std::min(3.0, x));
            x = x * (27.0 + x * x) / (27.0 + 9.0 * x * x) / std::sqrt(fDrive);

            fLp1 = (1.0 - fXc) * x + fXc * fLp1;
            fLp2 = (1.0 - fXc) * fLp1 + fXc * fLp2;
            const double low = fLp2;
            const double high = x - fLp2;

            fHornF = fHornC * fHornF + (1.0 - fHornC) * fHornTarget;
            fDrumF = fDrumC * fDrumF + (1.0 - fDrumC) * fDrumTarget;
            fHornPh += fHornF / fSr;
            if (fHornPh >= 1.0)
                fHornPh -= 1.0;
            fDrumPh += fDrumF / fSr;
            if (fDrumPh >= 1.0)
                fDrumPh -= 1.0;

            const size_t w = size_t(fW);
            fHL[w] = high; fHR[w] = high;
            fDL[w] = low; fDR[w] = low;

            const double off = 0.25 * fWidth;
            const double ha = 2.0 * M_PI * fHornPh;
            const double da = 2.0 * M_PI * fDrumPh;
            const double hl = tap(fHL, 1.0 + fSr * 0.00035 * (1.0 + std::sin(ha))) * (1.0 - 0.45 * (1.0 - std::cos(ha)));
            const double hr = tap(fHR, 1.0 + fSr * 0.00035 * (1.0 + std::sin(ha + 2.0 * M_PI * off))) *
                              (1.0 - 0.45 * (1.0 - std::cos(ha + 2.0 * M_PI * off)));
            const double dl = tap(fDL, 1.0 + fSr * 0.0005 * (1.0 + std::sin(da))) * (1.0 - 0.2 * (1.0 - std::cos(da)));
            const double dr = tap(fDR, 1.0 + fSr * 0.0005 * (1.0 + std::sin(da + 2.0 * M_PI * off))) *
                              (1.0 - 0.2 * (1.0 - std::cos(da + 2.0 * M_PI * off)));

            fW += 1.0;
            if (fW >= fSize)
                fW = 0.0;

            const double hg = std::min(1.0, 2.0 * fBal), dg = std::min(1.0, 2.0 * (1.0 - fBal));
            const double wl = hl * hg + dl * dg;
            const double wr = hr * hg + dr * dg;
            outL[i] = static_cast<T>(s0 * (1.0 - fMix) + wl * fMix);
            outR[i] = static_cast<T>(s1 * (1.0 - fMix) + wr * fMix);
        }
    }

private:
    void updateParams()
    {
        fHornTarget = fP.speed == 0 ? 0.8 : fP.speed == 1 ? 6.7 : 0.0;
        fDrumTarget = fP.speed == 0 ? 0.7 : fP.speed == 1 ? 5.8 : 0.0;
        const double acc = fP.accelPct / 100.0;
        fHornC = std::exp(-1.0 / (0.7 * acc * fSr));
        fDrumC = std::exp(-1.0 / (3.0 * acc * fSr));
        fBal = fP.balancePct / 100.0;
        fDrive = 1.0 + fP.drivePct / 100.0 * 6.0;
        fWidth = fP.widthPct / 100.0;
        fMix = fP.mixPct / 100.0;
        fXc = std::exp(-2.0 * M_PI * 800.0 / fSr);
    }

    // the JSFX's tap(): read d samples back with linear interpolation
    double tap(const std::vector<double>& buf, double d) const noexcept
    {
        double rp = fW - d;
        if (rp < 0.0)
            rp += fSize;
        const double i = std::floor(rp), f = rp - i;
        double i2 = i + 1.0;
        if (i2 >= fSize)
            i2 = 0.0;
        const double a = buf[size_t(i)];
        return a + (buf[size_t(i2)] - a) * f;
    }

    Params fP;
    double fSr = 48000.0, fSize = 1.0, fW = 0.0;
    std::vector<double> fHL, fHR, fDL, fDR;
    double fHornPh = 0.0, fDrumPh = 0.25, fHornF = 0.8, fDrumF = 0.7, fLp1 = 0.0, fLp2 = 0.0;
    double fHornTarget = 0.8, fDrumTarget = 0.7, fHornC = 0.0, fDrumC = 0.0;
    double fBal = 0.5, fDrive = 2.2, fWidth = 0.8, fMix = 1.0, fXc = 0.0;
};

} // namespace nullrotary
