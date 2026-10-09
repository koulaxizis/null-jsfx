/*
 * NULL JSFX - Louder, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/louder.jsfx (computed in double, like EEL2):
 * up to +12 dB of gain into a 2 ms look-ahead peak limiter (ceiling -0.3 dBFS, 80 ms
 * release). Latency = la = max(1, floor(0.002 * srate)) samples (the JSFX's pdc_delay).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nulllouder {

static constexpr double kMaxGainDb = 12.0;
static constexpr double kCeilingDb = -0.3;

inline double gainForAmount(double amount) noexcept { return std::pow(10.0, amount / 100.0 * 12.0 / 20.0); }
inline double ceilingLin() noexcept { return std::pow(10.0, -0.3 / 20.0); }
inline int lookahead(double sr) noexcept { return std::max(1, int(std::floor(0.002 * sr))); }

class LouderProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fLa = lookahead(sr);
        fBuf.assign(size_t(2 * fLa + 2), 0.0);
        fPos = 0;
        fEnv = 1.0;
        fHval = 1.0;
        fHcount = 0.0;
        fCeiling = ceilingLin();
        fAtt = std::exp(-3.0 / fLa);
        fRel = std::exp(-1.0 / (0.08 * sr));
    }

    // @slider
    void setAmount(double amount) noexcept { fGain = gainForAmount(amount); }

    int latency() const noexcept { return fLa; }
    double env() const noexcept { return fEnv; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames) noexcept
    {
        const double la = fLa, ceiling = fCeiling, att = fAtt, rel = fRel, gain = fGain;
        double* buf = fBuf.data();
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double s0 = inL[i], s1 = inR[i];
            const double xl = s0 * gain, xr = s1 * gain;
            const double pk = std::max(std::fabs(xl), std::fabs(xr));
            const double target = pk > ceiling ? ceiling / pk : 1.0;
            if (target <= fHval) { fHval = target; fHcount = la; }
            else if (fHcount > 0.0) fHcount -= 1.0;
            else fHval = target;
            fEnv = fHval < fEnv ? att * fEnv + (1.0 - att) * fHval : rel * fEnv + (1.0 - rel) * fHval;
            const double dl = buf[fPos], dr = buf[fPos + fLa];
            buf[fPos] = xl;
            buf[fPos + fLa] = xr;
            fPos = (fPos + 1) % fLa;
            const double o0 = std::max(-ceiling, std::min(ceiling, dl * fEnv));
            const double o1 = std::max(-ceiling, std::min(ceiling, dr * fEnv));
            outL[i] = static_cast<T>(o0);
            outR[i] = static_cast<T>(o1);
        }
    }

private:
    std::vector<double> fBuf;
    int fLa = 1, fPos = 0;
    double fEnv = 1.0, fHval = 1.0, fHcount = 0.0;
    double fCeiling = 1.0, fAtt = 0.0, fRel = 0.0, fGain = 1.0;
};

} // namespace nulllouder
