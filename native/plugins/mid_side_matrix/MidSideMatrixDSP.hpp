/*
 * NULL JSFX - Mid/Side Matrix, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/mid_side_matrix.jsfx, computed in double like EEL2. Encode:
 * L/R in, mid (0.707 (L + R)) on the left and side (0.707 (L - R)) on the right out. Decode:
 * mid on the left and side on the right in, L/R out. Each path has its own gain (-6 to +6 dB);
 * the two paths are blended by the mode (0 or 1), as the JSFX does, rather than switched.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nullmidside {

inline double gainFor(double db) noexcept { return std::exp(db * 0.11512925464970228420); }

class MidSideProcessor {
public:
    // @slider
    void setParams(double midDb, double sideDb, double mode)
    {
        fMid = gainFor(midDb);
        fSide = gainFor(sideDb);
        fMode = mode;
    }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            const double midEnc = (l + r) * 0.707 * fMid;
            const double sideEnc = (l - r) * 0.707 * fSide;
            const double midDec = l * fMid, sideDec = r * fSide;
            const double sum = (midDec + sideDec) * 0.707;
            const double diff = (midDec - sideDec) * 0.707;
            const double be = 1.0 - fMode, bd = fMode;
            outL[i] = static_cast<T>(midEnc * be + sum * bd);
            outR[i] = static_cast<T>(sideEnc * be + diff * bd);
        }
    }

private:
    double fMid = 1.0, fSide = 1.0, fMode = 1.0;
};

} // namespace nullmidside
