/*
 * NULL JSFX - Riser (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "RiserDSP.hpp"

START_NAMESPACE_DISTRHO

class UIRiser : public NullOneSliderUI
{
public:
    UIRiser()
        : NullOneSliderUI({ "RISER", "INTENSITY", "OFF", "DROP", 0.0f, 100.0f, 0.0f, false, false }) {}

protected:
    // high-pass magnitude (dB) at frequency f for the slider setting (svf_hp of the JSFX)
    static double hpDb(double f, double t, double sr)
    {
        const nullriser::Settings s = nullriser::settingsFor(t);
        const double g = std::tan(nullriser::kPi * std::fmin(s.fc, 0.45 * sr) / sr);
        const double k = 1.0 / s.q;
        const double w = std::tan(nullriser::kPi * std::fmin(f, sr * 0.499) / sr) / g;
        const double w2 = w * w;
        return 10.0 * std::log10(std::fmax(w2 * w2 / ((1.0 - w2) * (1.0 - w2) + k * k * w2), 1e-10));
    }

    // as in riser_gfx.jsfx: the high-pass response at the slider setting, the noise band centre dashed
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(24.0f);

        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const double t = value() / 100.0;
        if (value() > 0.0f)
            dashedV(freqX(float(nullriser::settingsFor(t).fc * 2.0 + 200.0)));

        constexpr int n = 285;
        float xs[n], ys[n];
        for (int i = 0; i < n; ++i)
        {
            const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
            xs[i] = fPX + fPW * float(i) / float(n - 1);
            ys[i] = linY(float(hpDb(f, t, sr)), -16.0f, 16.0f);
        }
        curve(xs, ys, n, linY(0.0f, -16.0f, 16.0f));
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
        {
            std::snprintf(buf, n, "UNTOUCHED");
            return;
        }
        const double t = v / 100.0, fc = nullriser::settingsFor(t).fc;
        char f[24];
        if (fc < 1000.0)
            std::snprintf(f, sizeof(f), "%d Hz", int(std::floor(fc + 0.5)));
        else
            std::snprintf(f, sizeof(f), "%.1f kHz", fc / 1000.0);
        std::snprintf(buf, n, "HP %s  WIDTH +%.1f dB", f, t * 6.0);
    }

    void formatValue(char* buf, size_t n, float v) override
    {
        std::snprintf(buf, n, "%d%%", int(std::lround(v)));
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIRiser)
};

UI* createUI()
{
    return new UIRiser();
}

END_NAMESPACE_DISTRHO
