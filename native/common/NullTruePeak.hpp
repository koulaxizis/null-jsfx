/*
 * NULL JSFX - true peak of a stereo pair, shared by Maximizer and Limiter
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nulljsfx {

// True peak of a stereo pair (BS.1770-4 Annex 2: 4x, 12 taps a phase, as loudness_meter.jsfx
// and tpeak() in maximizer.jsfx and limiter.jsfx), 6 samples late: the larger of the sample 6
// back and the four values interpolated around it. peak() returns the left (or, linked, the
// pair's) peak, right() the right's.
class TruePeak {
public:
    void reset() noexcept
    {
        for (double& x : fXL)
            x = 0.0;
        for (double& x : fXR)
            x = 0.0;
        fW = 0;
    }

    double peak(double xl, double xr, bool link) noexcept
    {
        static const double kC[24] = {
            0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625,
            0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500,
            -0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250,
            0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375,
        };
        if (--fW < 0)
            fW = 11;
        fXL[fW] = fXL[fW + 12] = xl;
        fXR[fW] = fXR[fW + 12] = xr;
        double pk = link ? std::max(std::fabs(fXL[fW + 6]), std::fabs(fXR[fW + 6])) : std::fabs(fXL[fW + 6]);
        fPr = std::fabs(fXR[fW + 6]);
        for (int ph = 0; ph < 48; ph += 12)
        {
            double al = 0.0, ar = 0.0;
            for (int j = 0; j < 12; ++j)
            {
                const int k = ph + j;
                const double c = k < 24 ? kC[k] : kC[47 - k]; // phases 2 and 3 are 1 and 0 reversed
                al += c * fXL[fW + j];
                ar += c * fXR[fW + j];
            }
            pk = link ? std::max(pk, std::max(std::fabs(al), std::fabs(ar))) : std::max(pk, std::fabs(al));
            fPr = std::max(fPr, std::fabs(ar));
        }
        return pk;
    }

    double right() const noexcept { return fPr; }

private:
    double fXL[24] = {}, fXR[24] = {}, fPr = 0.0;
    int fW = 0;
};

} // namespace nulljsfx
