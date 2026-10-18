/*
 * NULL JSFX - Synth (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * As synth_gfx.jsfx: the display shows the oscillator wave (or the table's frame at Position),
 * the filter's response and the two envelopes, with a dot per voice; the controls sit on four
 * tabs (OSC, FILTER, AMP, LFO + FX).
 */

#include "NullUI.hpp"
#include "SynthSliders.hpp"
#include "SynthTables.hpp"

#include <cstdio>

START_NAMESPACE_DISTRHO

using K = NullKnobsUI;

// slider index (from 0), kind, label, row, tab, bipolar, log
static const NullKnobsUI::Control kControls[] = {
    { 0, K::kSelect, "ENGINE", 0, 0, false },
    { 1, K::kSelect, "OSC 1", 0, 0, false },
    { 2, K::kSelect, "OSC 2", 0, 0, false },
    { 3, K::kKnob, "SEMI", 0, 0, true },
    { 4, K::kKnob, "DETUNE", 0, 0, true },
    { 5, K::kKnob, "OSC MIX", 0, 0, false },
    { 8, K::kSelect, "TABLE", 1, 0, false },
    { 9, K::kKnob, "POSITION", 1, 0, false },
    { 6, K::kKnob, "SUB", 1, 0, false },
    { 7, K::kKnob, "NOISE", 1, 0, false },
    { 32, K::kKnob, "UNISON", 1, 0, false },
    { 33, K::kKnob, "SPREAD", 1, 0, false },

    { 10, K::kSelect, "MODE", 0, 1, false },
    { 11, K::kSelect, "SLOPE", 0, 1, false },
    { 12, K::kKnob, "CUTOFF", 0, 1, false, true },
    { 13, K::kKnob, "RESONANCE", 0, 1, false },
    { 14, K::kKnob, "ENV AMOUNT", 0, 1, true },
    { 15, K::kKnob, "KEY TRACK", 0, 1, false },
    { 16, K::kKnob, "ATTACK", 1, 1, false, true },
    { 17, K::kKnob, "DECAY", 1, 1, false, true },
    { 18, K::kKnob, "SUSTAIN", 1, 1, false },
    { 19, K::kKnob, "RELEASE", 1, 1, false, true },

    { 20, K::kKnob, "ATTACK", 0, 2, false, true },
    { 21, K::kKnob, "DECAY", 0, 2, false, true },
    { 22, K::kKnob, "SUSTAIN", 0, 2, false },
    { 23, K::kKnob, "RELEASE", 0, 2, false, true },
    { 41, K::kKnob, "VELOCITY", 0, 2, false },
    { 44, K::kKnob, "VOLUME", 0, 2, false },
    { 31, K::kSelect, "VOICES", 1, 2, false },
    { 30, K::kKnob, "GLIDE", 1, 2, false },
    { 43, K::kKnob, "OCTAVE", 1, 2, true },
    { 42, K::kKnob, "BEND", 1, 2, false },
    { 34, K::kKnob, "CHARACTER", 1, 2, false },

    { 24, K::kSelect, "LFO SHAPE", 0, 3, false },
    { 25, K::kKnob, "LFO RATE", 0, 3, false, true },
    { 26, K::kSelect, "LFO SYNC", 0, 3, false },
    { 27, K::kKnob, "TO PITCH", 0, 3, false },
    { 28, K::kKnob, "TO FILTER", 0, 3, false },
    { 29, K::kKnob, "TO POSITION", 0, 3, false },
    { 35, K::kKnob, "CHORUS", 1, 3, false },
    { 36, K::kKnob, "DELAY", 1, 3, false },
    { 37, K::kSelect, "DELAY TIME", 1, 3, false },
    { 38, K::kKnob, "FEEDBACK", 1, 3, false },
    { 39, K::kKnob, "REVERB", 1, 3, false },
    { 40, K::kKnob, "SIZE", 1, 3, false },
};
static constexpr uint32_t kNumControls = sizeof(kControls) / sizeof(kControls[0]);

// the sliders with the selectors' upper-case names
static const SliderInfo* uiSliders()
{
    using namespace nullsynth;
    static SliderInfo s[kNumParams];
    for (uint32_t i = 0; i < kNumParams; ++i)
        s[i] = kSliders[i];
    s[0].options = kEngineShort;
    s[1].options = kOsc1WaveShort;
    s[2].options = kOsc2WaveShort;
    s[8].options = kTableShort;
    s[10].options = kFilterModeShort;
    s[11].options = kSlopeShort;
    s[24].options = kLfoShapeShort;
    s[26].options = kLfoSyncShort;
    s[31].options = kVoiceModeShort;
    s[37].options = kDelayTimeShort;
    return s;
}

class UISynth : public NullKnobsUI
{
public:
    UISynth()
        : NullKnobsUI({ "SYNTH", "INSTRUMENTS", 600, 150, "OSC|FILTER|AMP|LFO + FX", 2,
                        uiSliders(), nullsynth::kNumParams, kControls, kNumControls, true })
    {
    }

protected:
    int iv(uint32_t p) const { return int(std::floor(value(p) + 0.5f)); }

    void panel(float x, float w, const char* title)
    {
        beginPath();
        rect(x, 76.0f, w, 116.0f);
        fillColor(col(theme::kBackground));
        fill();
        beginPath();
        rect(x + 0.5f, 76.5f, w - 1.0f, 115.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x, 68.0f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, title, true);
    }

    void hline(float x0, float x1, float y)
    {
        beginPath();
        moveTo(x0, std::round(y) + 0.5f);
        lineTo(x1, std::round(y) + 0.5f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
    }

    void vline(float x, float y0, float y1)
    {
        beginPath();
        moveTo(std::round(x) + 0.5f, y0);
        lineTo(std::round(x) + 0.5f, y1);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
    }

    void poly(const float* xs, const float* ys, int n, Color c)
    {
        beginPath();
        moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i)
            lineTo(xs[i], ys[i]);
        strokeColor(c);
        strokeWidth(1.5f);
        lineJoin(ROUND);
        stroke();
    }

    static float wave(int w, float p)
    {
        return w == 0 ? 2 * p - 1 : w == 1 ? (p < 0.5f ? 1.0f : -1.0f) : w == 2 ? (p < 0.5f ? 4 * p - 1 : 3 - 4 * p)
                                                                              : std::sin(2.0f * float(nullsynth::kPi) * p);
    }

    static float mag(float r, float q, int mode)
    {
        const float d = std::sqrt((1 - r * r) * (1 - r * r) + (r / q) * (r / q));
        return mode == 0 ? 1 / d : mode == 1 ? (r / q) / d : r * r / d;
    }

    static float seg(float ms) { return 0.25f * (0.1f + 0.9f * std::log10(std::max(1.0f, ms)) / 4.0f); }

    void env(float x0, float w, float a, float d, float s, float r, Color c)
    {
        float xs[36], ys[36];
        int n = 0;
        const float xa = x0 + w * seg(a), xd = xa + w * seg(d), xh = xd + w * 0.2f, xr = xh + w * seg(r);
        xs[n] = x0; ys[n++] = 186.0f;
        xs[n] = xa; ys[n++] = 86.0f;
        for (int i = 1; i <= 16; ++i)
        {
            const float u = i / 16.0f, e = s + (1 - s) * std::exp(-4.6f * u);
            xs[n] = xa + (xd - xa) * u; ys[n++] = 186.0f - 100.0f * e;
        }
        xs[n] = xh; ys[n++] = 186.0f - 100.0f * s;
        for (int i = 1; i <= 16; ++i)
        {
            const float u = i / 16.0f, e = s * std::exp(-4.6f * u);
            xs[n] = xh + (xr - xh) * u; ys[n++] = 186.0f - 100.0f * e;
        }
        poly(xs, ys, n, c);
    }

    void drawDisplay() override
    {
        static const char* const kWaves[] = { "SAW", "SQR", "TRI", "SIN" };
        char title[64];
        float xs[169], ys[169];

        // the wave
        const bool wt = value(0) >= 0.5f;
        const int tbl = std::max(0, std::min(7, iv(8)));
        if (wt)
            std::snprintf(title, sizeof(title), "TABLE  %s  %d%%", nullsynth::kTableShort[tbl], iv(9));
        else
            std::snprintf(title, sizeof(title), "OSC  %s + %s", kWaves[std::max(0, std::min(3, iv(1)))], kWaves[std::max(0, std::min(3, iv(2)))]);
        panel(24.0f, 176.0f, title);
        hline(25.0f, 199.0f, 134.0f);
        const std::vector<double>& t = nullsynth::wavetables();
        const float px = value(9) / 100.0f * 7.0f;
        const int fi = std::min(6, int(std::floor(px)));
        const float ff = px - float(fi);
        const float r2 = std::pow(2.0f, value(3) / 12.0f), mix = value(5) / 100.0f;
        for (int n = 0; n <= 168; ++n)
        {
            const float p = n / 168.0f;
            float v;
            if (wt)
            {
                const int j = int(std::floor(p * 1023.0f + 0.5f));
                const double* b = t.data() + size_t(tbl * 8 + fi) * 8192;
                v = float(b[j] + (b[8192 + j] - b[j]) * ff);
            }
            else
            {
                float q = p * r2;
                q -= std::floor(q);
                v = (1 - mix) * wave(iv(1), p) + mix * wave(iv(2), q);
            }
            xs[n] = 28.0f + float(n);
            ys[n] = 134.0f - 50.0f * std::max(-1.0f, std::min(1.0f, v));
        }
        poly(xs, ys, 169, col(theme::kAccent));

        // the filter
        const float c = value(12) * std::pow(2.0f, value(34) / 100.0f);
        char cv[24];
        if (c >= 1000.0f)
            std::snprintf(cv, sizeof(cv), "%.1f kHz", double(c) / 1000.0);
        else
            std::snprintf(cv, sizeof(cv), "%d Hz", int(c));
        const int mode = std::max(0, std::min(2, iv(10)));
        const bool s24 = value(11) >= 0.5f;
        std::snprintf(title, sizeof(title), "%s %s  %s", mode == 0 ? "LP" : mode == 1 ? "BP" : "HP", s24 ? "24" : "12", cv);
        panel(212.0f, 176.0f, title);
        for (int k = 0; k < 3; ++k)
            vline(216.0f + 168.0f * (k + 1) / 3.0f, 77.0f, 191.0f);
        hline(213.0f, 387.0f, 82.0f + 18.0f / 48.0f * 104.0f);
        const float res = value(13) / 100.0f;
        const float q = s24 ? 1.31f + 10.7f * res * res : 0.707f + 11.3f * res * res;
        for (int n = 0; n <= 168; ++n)
        {
            const float r = 20.0f * std::pow(1000.0f, n / 168.0f) / c;
            float v = mag(r, q, mode);
            if (s24)
                v *= mag(r, 0.54f, mode);
            const float db = 20.0f * std::log10(std::max(0.000001f, v));
            xs[n] = 216.0f + float(n);
            ys[n] = 82.0f + (18.0f - std::max(-30.0f, std::min(18.0f, db))) / 48.0f * 104.0f;
        }
        poly(xs, ys, 169, col(theme::kAccent));

        // the envelopes and the voices
        panel(400.0f, 176.0f, "ENVELOPES");
        env(404.0f, 168.0f, value(16), value(17), value(18) / 100.0f, value(19), col(theme::kMuted, 0.8f));
        env(404.0f, 168.0f, value(20), value(21), value(22) / 100.0f, value(23), col(theme::kAccent));
        nulljsfx::Taps* tp = taps();
        for (int k = 0; k < 8; ++k)
        {
            const float e = tp ? std::max(0.0f, std::min(1.0f, tp->aux[k].load(std::memory_order_relaxed))) : 0.0f;
            beginPath();
            circle(510.0f + 9.0f * k, 68.0f, 2.5f);
            fillColor(mix3(theme::kBorder, theme::kAccent, e));
            fill();
        }
    }

    static Color mix3(uint32_t a, uint32_t b, float k)
    {
        auto ch = [k](uint32_t x, uint32_t y, int sh) { return int(float((x >> sh) & 255) * (1.0f - k) + float((y >> sh) & 255) * k); };
        return Color(ch(a, b, 16), ch(a, b, 8), ch(a, b, 0));
    }

    static void ms(float v, char* buf, size_t n)
    {
        if (v >= 1000.0f)
            std::snprintf(buf, n, "%.2f s", double(v) / 1000.0);
        else
            std::snprintf(buf, n, "%d ms", int(std::lround(v)));
    }

    void formatValue(uint32_t p, float v, char* buf, size_t n) override
    {
        switch (p)
        {
        case 3: std::snprintf(buf, n, "%+d st", int(std::lround(v))); break;
        case 4: std::snprintf(buf, n, "%+d ct", int(std::lround(v))); break;
        case 12:
            if (v >= 1000.0f)
                std::snprintf(buf, n, "%.2f kHz", double(v) / 1000.0);
            else
                std::snprintf(buf, n, "%d Hz", int(std::lround(v)));
            break;
        case 14: std::snprintf(buf, n, "%+d%%", int(std::lround(v))); break;
        case 16: case 17: case 19: case 20: case 21: case 23: ms(v, buf, n); break;
        case 25: std::snprintf(buf, n, "%.2f Hz", double(v)); break;
        case 27: std::snprintf(buf, n, "%.2f st", double(v)); break;
        case 28: std::snprintf(buf, n, "%.1f oct", double(v)); break;
        case 30: std::snprintf(buf, n, "%d ms", int(std::lround(v))); break;
        case 32: std::snprintf(buf, n, "%d", int(std::lround(v))); break;
        case 42: std::snprintf(buf, n, "%d st", int(std::lround(v))); break;
        case 43: std::snprintf(buf, n, "%+d", int(std::lround(v))); break;
        case 44: std::snprintf(buf, n, "%+.1f dB", double(v)); break;
        default: NullKnobsUI::formatValue(p, v, buf, n); break;
        }
    }
};

UI* createUI()
{
    return new UISynth();
}

END_NAMESPACE_DISTRHO
