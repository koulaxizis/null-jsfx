/*
 * NULL JSFX - Closer (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "CloserDSP.hpp"

START_NAMESPACE_DISTRHO

class UICloser : public NullOneSliderUI
{
public:
    UICloser()
        : NullOneSliderUI({ "CLOSER", "AMOUNT", "OFF", "CLOSE", 0.0f, 100.0f, 40.0f, false, false }) {}

protected:
    // as in closer_gfx.jsfx: the presence and proximity lift (make-up left out); the dim curve
    // is the presence while the compressor works hard
    void drawDisplay() override
    {
        setPlot(46.0f, 64.0f, 284.0f, 78.0f);
        gridFreq(12.0f);
        double sr = getSampleRate();
        if (!(sr > 0.0))
            sr = 48000.0;
        const nullcloser::Coeffs c = nullcloser::coeffsFor(sr);
        const nullcloser::Params p = nullcloser::paramsFor(value());
        constexpr int n = 285;
        float xs[n], ys[n];
        for (int pass = 0; pass < 2; ++pass)
        {
            const double k = pass == 0 ? p.kp * 0.5 : p.kp;
            for (int i = 0; i < n; ++i)
            {
                const double f = 20.0 * std::pow(1000.0, double(i) / double(n - 1));
                xs[i] = fPX + fPW * float(i) / float(n - 1);
                ys[i] = linY(float(nullcloser::responseDb(c, p, k, f, sr)), -8.0f, 8.0f);
            }
            beginPath();
            moveTo(xs[0], ys[0]);
            for (int i = 1; i < n; ++i)
                lineTo(xs[i], ys[i]);
            strokeColor(col(theme::kAccent, pass == 0 ? 0.45f : 1.0f));
            strokeWidth(2.0f);
            lineJoin(ROUND);
            stroke();
        }
    }

    void formatCaption(char* buf, size_t n, float v) override
    {
        if (std::lround(v) == 0)
        {
            std::snprintf(buf, n, "UNTOUCHED");
            return;
        }
        const nullcloser::Params p = nullcloser::paramsFor(v);
        std::snprintf(buf, n, "RATIO %.1f:1  PRESENCE +%.1f dB", p.ratio, p.a * 5.0);
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UICloser)
};

UI* createUI()
{
    return new UICloser();
}

END_NAMESPACE_DISTRHO
