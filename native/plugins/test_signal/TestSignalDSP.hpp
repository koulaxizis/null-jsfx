/*
 * NULL JSFX - Test Signal, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/test_signal.jsfx, computed in double in the EEL2 order: a sine
 * or a logarithmic sweep from a phase accumulator, pink noise (Paul Kellet's filter) or white
 * noise from an exact Park-Miller generator, or a 2 ms raised-cosine pulse twice a second; the
 * level glides over 10 ms, Channels and Invert Right pick the sides, Pass Input adds the signal
 * to the input instead of replacing it.
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace nulltestsignal {

static constexpr double kPi = 3.141592653589793; // EEL2 $pi

struct Params {
    int signal = 0;
    double frequency = 1000.0, level = -20.0, sweepTime = 10.0;
    int channels = 0;
    bool invert = false, pass = false;
};

class TestSignalProcessor {
public:
    void setSampleRate(double sr) noexcept
    {
        fSr = sr;
        derive();
    }

    void setParams(const Params& p) noexcept
    {
        fP = p;
        derive();
    }

    // @init
    void reset() noexcept
    {
        fPh = 0.0;
        fSwT = 0.0;
        fPlN = 0.0;
        fSeed = 12345.0;
        for (double& k : fPk)
            k = 0.0;
        fGs = 0.0;
        fCurF = 1000.0;
    }

    // one sample of @sample; l and r hold the input and return the output
    void processSample(double& l, double& r) noexcept
    {
        fGs += (fG - fGs) * fGc;
        double x;
        if (fP.signal == 0)
        {
            fCurF = fP.frequency;
            x = std::sin(2.0 * kPi * fPh);
            fPh += fCurF / fSr;
            fPh -= std::floor(fPh);
        }
        else if (fP.signal == 1)
        {
            fCurF = 20.0 * std::pow(1000.0, fSwT / fP.sweepTime);
            x = std::sin(2.0 * kPi * fPh);
            fPh += fCurF / fSr;
            fPh -= std::floor(fPh);
            fSwT += 1.0 / fSr;
            if (fSwT >= fP.sweepTime)
                fSwT = 0.0;
        }
        else if (fP.signal == 4)
        {
            x = fPlN < fPlLen ? 0.5 - 0.5 * std::cos(2.0 * kPi * fPlN / fPlLen) : 0.0;
            fPlN += 1.0;
            if (fPlN >= fPlEvery)
                fPlN = 0.0;
        }
        else
        {
            fSeed *= 16807.0;
            fSeed -= std::floor(fSeed / 2147483647.0) * 2147483647.0;
            const double w = fSeed / 1073741824.0 - 1.0;
            if (fP.signal == 3)
                x = w * 1.7320508075688772;
            else
            {
                fPk[0] = 0.99886 * fPk[0] + w * 0.0555179;
                fPk[1] = 0.99332 * fPk[1] + w * 0.0750759;
                fPk[2] = 0.969 * fPk[2] + w * 0.153852;
                fPk[3] = 0.8665 * fPk[3] + w * 0.3104856;
                fPk[4] = 0.55 * fPk[4] + w * 0.5329522;
                fPk[5] = -0.7616 * fPk[5] - w * 0.016898;
                x = (fPk[0] + fPk[1] + fPk[2] + fPk[3] + fPk[4] + fPk[5] + fPk[6] + w * 0.5362) * 0.5628;
                fPk[6] = w * 0.115926;
            }
        }
        x *= fGs;
        const double xl = fP.channels == 2 ? 0.0 : x;
        const double xr = fP.channels == 1 ? 0.0 : fP.invert ? -x : x;
        if (fP.pass)
        {
            l += xl;
            r += xr;
        }
        else
        {
            l = xl;
            r = xr;
        }
    }

    // the frequency now (the display), in Hz
    double currentFrequency() const noexcept { return fCurF; }

private:
    // @slider
    void derive() noexcept
    {
        fG = std::pow(10.0, fP.level / 20.0);
        fGc = 1.0 - std::exp(-1000.0 / (10.0 * fSr));
        fPlLen = std::floor(0.002 * fSr);
        fPlEvery = std::floor(0.5 * fSr);
    }

    Params fP;
    double fSr = 48000.0;
    double fG = 0.1, fGc = 0.0, fPlLen = 96.0, fPlEvery = 24000.0;
    double fPh = 0.0, fSwT = 0.0, fPlN = 0.0, fSeed = 12345.0, fGs = 0.0, fCurF = 1000.0;
    double fPk[7] = {};
};

} // namespace nulltestsignal
