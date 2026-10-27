/*
 * NULL JSFX - Click Repair, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/click_repair.jsfx, computed in double like EEL2: a sample
 * that jumps further from the previous one than Threshold allows (4x to 30x the running
 * average step) marks Window samples from there as damaged, and those are redrawn as a
 * straight line between the good samples on either side. The output is delayed by
 * 2 x Window samples (the plugin's latency) so the repair can see past each click. Mode
 * Crackle (2.7) detects on the second difference against a faster average (3x to 20x), marks
 * the sample before each detection too, holds off for two samples after the window and
 * redraws with a cubic through two outputs before and two good samples after (2 x Window + 2
 * samples of latency).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nullclick {

class ClickRepairProcessor {
public:
    static constexpr int kRing = 256;

    // @init
    void setSampleRate(double)
    {
        for (int i = 0; i < kRing; ++i)
            fL.buf[i] = fL.bad[i] = fR.buf[i] = fR.bad[i] = 0.0;
        fPos = 0;
        fL.avg = fR.avg = 0.01;
        fL.left = fR.left = 0.0;
        fL.last = fR.last = fL.last2 = fR.last2 = 0.0;
        fClicks = 0;
    }

    // @slider
    void setParams(double thresholdPct, double window, double mode = 0.0)
    {
        fWin = std::max(1.0, std::min(50.0, std::floor(window)));
        fCrackle = mode >= 0.5;
        fDelay = int(2.0 * fWin + (fCrackle ? 2.0 : 0.0));
        fRatio = fCrackle ? 3.0 + thresholdPct / 100.0 * 17.0 : 4.0 + thresholdPct / 100.0 * 26.0;
    }

    int latency() const noexcept { return fDelay; }
    // clicks found so far (both channels), for the display
    uint32_t clicks() const noexcept { return fClicks; }

    // in/out of the sample being output now (the unrepaired one at the same time), for the display
    double lastInMid() const noexcept { return fInMid; }

    template <typename T>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            store(fL, inL[i]);
            store(fR, inR[i]);
            const double yl = repaired(fL), yr = repaired(fR);
            const int o = (fPos - fDelay + kRing) % kRing;
            fInMid = (fL.buf[o] + fR.buf[o]) * 0.5;
            outL[i] = static_cast<T>(yl);
            outR[i] = static_cast<T>(yr);
            fPos = (fPos + 1) % kRing;
        }
    }

private:
    struct Chan {
        double buf[kRing] = {}, bad[kRing] = {};
        double avg = 0.01, left = 0.0, last = 0.0, last2 = 0.0;
    };

    void store(Chan& c, double x) noexcept
    {
        if (fCrackle)
        {
            const double d = std::fabs(x - 2.0 * c.buf[(fPos - 1 + kRing) % kRing] + c.buf[(fPos - 2 + kRing) % kRing]);
            if (c.left <= -2.0 && d > std::max(c.avg * fRatio, 0.005))
            {
                c.left = fWin;
                c.bad[(fPos - 1 + kRing) % kRing] = 1.0;
                ++fClicks;
            }
            if (c.left > 0.0)
            {
                c.bad[fPos] = 1.0;
                c.left -= 1.0;
            }
            else
            {
                c.bad[fPos] = 0.0;
                if (c.left > -2.0)
                    c.left -= 1.0;
                else
                    c.avg = c.avg * 0.98 + d * 0.02;
            }
            c.buf[fPos] = x;
            return;
        }
        const double d = std::fabs(x - c.buf[(fPos - 1 + kRing) % kRing]);
        if (c.left <= 0.0 && d > std::max(c.avg * fRatio, 0.01))
        {
            c.left = fWin;
            ++fClicks;
        }
        if (c.left > 0.0)
        {
            c.bad[fPos] = 1.0;
            c.left -= 1.0;
        }
        else
        {
            c.bad[fPos] = 0.0;
            c.avg = c.avg * 0.995 + d * 0.005;
        }
        c.buf[fPos] = x;
    }

    double repaired(Chan& c) const noexcept
    {
        const int o = (fPos - fDelay + kRing) % kRing;
        double y;
        if (c.bad[o] != 0.0)
        {
            int j = o, n = 1;
            do
            {
                j = (j + 1) % kRing;
                n += 1;
            } while (c.bad[j] != 0.0 && n < kRing);
            if (fCrackle)
            {
                // the cubic through (-1, last2), (0, last), (n, buf[j]), (n + 1, y2) at 1
                const double y2 = c.bad[(j + 1) % kRing] != 0.0 ? c.buf[j] : c.buf[(j + 1) % kRing];
                const double dn = n;
                y = (2.0 * (dn - 1.0) * (dn + 2.0) * c.last - dn * (dn - 1.0) * c.last2 + 2.0 * (dn + 2.0) * c.buf[j]
                     - 2.0 * (dn - 1.0) * y2)
                    / ((dn + 1.0) * (dn + 2.0));
            }
            else
                y = c.last + (c.buf[j] - c.last) / n;
        }
        else
        {
            y = c.buf[o];
        }
        c.last2 = c.last;
        c.last = y;
        return y;
    }

    Chan fL, fR;
    int fPos = 0, fDelay = 20;
    double fWin = 10.0, fRatio = 17.0, fInMid = 0.0;
    bool fCrackle = false;
    uint32_t fClicks = 0;
};

} // namespace nullclick
