/*
 * NULL JSFX - Octaver, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/octaver.jsfx, computed in double like EEL2 and in the same order:
 * an analog-style octave divider. The mono sum (DC removed) is low-passed at Tracking; a Schmitt
 * trigger clocks a flip-flop (-1 octave) that clocks a second one (-2 octaves); the squares
 * follow the input envelope. Up is the tracked signal full-wave rectified, normalised and DC
 * blocked. The voices go through a two-pole Tone low pass and are added to the dry signal.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nulloctaver {

static constexpr double kPi = 3.141592653589793;

// the signals the display reads, per sample (as the display-only code of octaver_gfx.jsfx)
struct Probe {
    double in, oct1, oct2, up, env, ff1;
};

class OctaverProcessor {
public:
    // @init
    void setSampleRate(double sr)
    {
        fSr = sr;
        dcR = 1.0 - 2.0 * kPi * 20.0 / sr;
        upR = 1.0 - 2.0 * kPi * 30.0 / sr;
        envA = 1.0 - std::exp(-1.0 / (0.001 * sr));
        envR = 1.0 - std::exp(-1.0 / (0.06 * sr));
        envS = 1.0 - std::exp(-1.0 / (0.008 * sr));
        trkRel = std::exp(-1.0 / (0.1 * sr));
        smK = 1.0 - std::exp(-1.0 / (0.01 * sr));
        dcX1 = dcY = 0.0;
        env1 = env = 0.0;
        t1 = t2 = tenv = 0.0;
        sch = -1.0;
        ff1 = ff2 = 1.0;
        upX1 = upY = 0.0;
        lp1 = lp2 = 0.0;
        first = true;
    }

    // @slider
    void setParams(double dry, double oct1, double oct2, double up, double toneHz, double trackHz)
    {
        tGd = dry / 100.0;
        tG1 = oct1 / 100.0 * 0.7;
        tG2 = oct2 / 100.0 * 0.7;
        tGu = up / 100.0;
        toneK = 1.0 - std::exp(-2.0 * kPi * toneHz / fSr);
        trkK = 1.0 - std::exp(-2.0 * kPi * trackHz / fSr);
        if (first)
        {
            gd = tGd; g1 = tG1; g2 = tG2; gu = tGu;
            first = false;
        }
    }

    double sampleRate() const noexcept { return fSr; }

    template <typename T, typename Display>
    void process(const T* inL, const T* inR, T* outL, T* outR, uint32_t frames, Display&& display)
    {
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double x0 = inL[i], x1 = inR[i];
            const double inX = (x0 + x1) * 0.5;
            dcY = inX - dcX1 + dcR * dcY;
            dcX1 = inX;

            const double envX = std::fabs(dcY);
            env1 += (envX > env1 ? envA : envR) * (envX - env1);
            env += envS * (env1 - env);

            t1 += trkK * (dcY - t1);
            t2 += trkK * (t1 - t2);
            const double trkX = std::fabs(t2);
            tenv = trkX > tenv ? trkX : tenv * trkRel;
            const double hyst = tenv * 0.2 + 0.00001;
            if (sch > 0.0)
            {
                if (t2 < -hyst)
                    sch = -1.0;
            }
            else if (t2 > hyst)
            {
                sch = 1.0;
                ff1 = -ff1;
                if (ff1 > 0.0)
                    ff2 = -ff2;
            }

            const double upX = trkX / std::max(tenv, 0.00001);
            upY = upX - upX1 + upR * upY;
            upX1 = upX;

            gd = glide(gd, tGd); g1 = glide(g1, tG1);
            g2 = glide(g2, tG2); gu = glide(gu, tGu);
            const double sq1 = ff1 * env;
            const double sq2 = ff2 * env;
            const double upv = 2.0 * upY * env;
            const double vox = g1 * sq1 + g2 * sq2 + gu * upv;
            lp1 += toneK * (vox - lp1);
            lp2 += toneK * (lp1 - lp2);

            flush(env1); flush(env); flush(t1); flush(t2); flush(dcY);
            flush(lp1); flush(lp2); flush(tenv); flush(upY);

            const double yl = x0 * gd + lp2;
            const double yr = x1 * gd + lp2;
            outL[i] = static_cast<T>(yl);
            outR[i] = static_cast<T>(yr);
            display(Probe { inX, g1 * sq1, g2 * sq2, gu * upv, env, ff1 }, yl, yr);
        }
    }

private:
    static void flush(double& v) noexcept
    {
        if (std::fabs(v) < 0.000000000000001)
            v = 0.0;
    }

    double glide(double v, double t) const noexcept
    {
        v += smK * (t - v);
        return std::fabs(t - v) < 1e-12 ? t : v;
    }

    double fSr = 48000.0;
    double dcR = 0, upR = 0, envA = 0, envR = 0, envS = 0, trkRel = 0, smK = 0;
    double dcX1 = 0, dcY = 0, env1 = 0, env = 0, t1 = 0, t2 = 0, tenv = 0;
    double sch = -1, ff1 = 1, ff2 = 1, upX1 = 0, upY = 0, lp1 = 0, lp2 = 0;
    double tGd = 1, tG1 = 0, tG2 = 0, tGu = 0, gd = 1, g1 = 0, g2 = 0, gu = 0;
    double toneK = 0.1, trkK = 0.04;
    bool first = true;
};

} // namespace nulloctaver
