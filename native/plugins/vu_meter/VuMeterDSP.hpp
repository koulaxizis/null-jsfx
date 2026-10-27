/*
 * NULL JSFX - VU Meter, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/vu_meter.jsfx, computed in double in the EEL2 order: per
 * channel a VU (second-order low pass of the rectified signal, damping 0.8, 99 % in 300 ms),
 * a PPM (5 ms charge, 24 dB in 2.8 s fall) or a K-14 / K-20 RMS (300 ms) reading, its hold
 * (3 s or until reset) and an OVER timer. The audio passes through (Monitor folds it to Mono or
 * Side after the meters). The View carries the readings to the interface.
 */

#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

namespace nullvu {

// what the interface draws, written once per block
struct View {
    std::atomic<float> y[2] { { -200.0f }, { -200.0f } };   // readings, dB on the mode's scale
    std::atomic<float> h[2] { { -200.0f }, { -200.0f } };   // held readings
    std::atomic<bool> over[2] { { false }, { false } };
    std::atomic<bool> resetRequest { false };               // set by the interface (a click)
};

class VuMeter {
public:
    // @init
    void setSampleRate(double sr)
    {
        const double w = 2.0 * M_PI * 2.085341377482407 / sr;
        const double al = std::sin(w) / (2.0 * 0.625);
        const double a0 = 1.0 + al;
        fB0 = (1.0 - std::cos(w)) / 2.0 / a0;
        fB1 = (1.0 - std::cos(w)) / a0;
        fB2 = fB0;
        fA1 = -2.0 * std::cos(w) / a0;
        fA2 = (1.0 - al) / a0;
        fPa = 1.0 - std::exp(-1.0 / (0.005 * sr));
        fPf = std::pow(10.0, -24.0 / 2.8 / 20.0 / sr);
        fKa = 1.0 - std::exp(-std::log(100.0) / (0.3 * sr));
        fHn = std::floor(3.0 * sr);
        fOn = std::floor(sr);
        for (Chan& c : fC)
            c = Chan();
        fLastMode = fLastHold = -1;
    }

    // @slider
    void setParams(double mode, double ref, double hold, double monitor)
    {
        fMode = int(std::floor(mode + 0.5));
        fRef = ref;
        fHold = int(std::floor(hold + 0.5));
        fMon = int(std::floor(monitor + 0.5));
        if (fMode != fLastMode || fHold != fLastHold)
        {
            for (Chan& c : fC)
                c.z1 = c.z2 = c.p = c.m = 0.0;
            reset();
            fLastMode = fMode;
            fLastHold = fHold;
        }
    }

    void reset()
    {
        for (Chan& c : fC)
        {
            c.h = -200.0;
            c.ht = c.ov = 0.0;
        }
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double l = inL[i], r = inR[i];
            chan(fC[0], l);
            chan(fC[1], r);
            if (fMon)
            {
                const double x = fMon == 1 ? (l + r) * 0.5 : (l - r) * 0.5;
                outL[i] = outR[i] = float(x);
            }
            else
            {
                outL[i] = float(l);
                outR[i] = float(r);
            }
        }
    }

    void publish(View& v) const
    {
        for (int k = 0; k < 2; ++k)
        {
            v.y[k].store(float(fC[k].y), std::memory_order_relaxed);
            v.h[k].store(float(fC[k].h), std::memory_order_relaxed);
            v.over[k].store(fC[k].ov > 0.0, std::memory_order_relaxed);
        }
    }

private:
    struct Chan {
        double z1 = 0.0, z2 = 0.0, p = 0.0, m = 0.0;
        double y = -200.0, h = -200.0, ht = 0.0, ov = 0.0;
    };

    static double db(double a) { return a > 0.0000000001 ? 20.0 * std::log10(a) : -200.0; }

    // vu_chan
    void chan(Chan& c, double x) noexcept
    {
        const double a = std::fabs(x);
        if (fMode == 0)
        {
            const double v = fB0 * a + c.z1;
            c.z1 = fB1 * a - fA1 * v + c.z2;
            c.z2 = fB2 * a - fA2 * v;
            c.y = db(v * M_PI / 2.0) - fRef;
        }
        else if (fMode == 1)
        {
            c.p = a > c.p ? c.p + (a - c.p) * fPa : c.p * fPf;
            c.y = db(c.p) - fRef;
        }
        else
        {
            c.m += (a * a - c.m) * fKa;
            c.y = (c.m > 0.00000000000000000001 ? 10.0 * std::log10(2.0 * c.m) : -200.0) + (fMode == 2 ? 14.0 : 20.0);
        }
        if (c.y > c.h)
        {
            c.h = c.y;
            c.ht = fHn;
        }
        else if (fHold == 1)
        {
            if (c.ht > 0.0)
                c.ht -= 1.0;
            else
                c.h = c.y;
        }
        else if (fHold == 0)
            c.h = c.y;
        if (a >= 1.0)
            c.ov = fOn;
        else if (c.ov > 0.0)
            c.ov -= 1.0;
    }

    Chan fC[2];
    double fB0 = 0.0, fB1 = 0.0, fB2 = 0.0, fA1 = 0.0, fA2 = 0.0, fPa = 0.0, fPf = 1.0, fKa = 0.0;
    double fHn = 0.0, fOn = 0.0, fRef = -18.0;
    int fMode = 0, fHold = 1, fMon = 0, fLastMode = -1, fLastHold = -1;
};

} // namespace nullvu
