/*
 * NULL JSFX - Stereo Scope (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "StereoScopeSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kKnob, "GAIN", 0, -1, false },
    { 1, NullKnobsUI::kSelect, "AUTO", 0, -1, false },
    { 2, NullKnobsUI::kKnob, "DECAY", 0, -1, false },
    { 3, NullKnobsUI::kKnob, "INTEGRATION", 0, -1, false, true },
    { 4, NullKnobsUI::kSelect, "MONITOR", 0, -1, false },
    { 5, NullKnobsUI::kSelect, "BANDS", 0, -1, false },
};

static constexpr uint32_t kBg2 = 0x131211; // --bg-secondary (NG_BG2 in null_gfx.jsfx-inc)

class UIStereoScope : public NullKnobsUI
{
public:
    UIStereoScope()
        : NullKnobsUI({ "STEREO SCOPE", "ADVANCED", 600, 300, nullptr, 1, nullscope::kSliders, nullscope::kNumSliders,
                        kControls, sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    const nullscope::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<StereoScopeBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    void line(float x1, float y1, float x2, float y2, uint32_t rgb, float alpha)
    {
        beginPath();
        moveTo(x1, y1);
        lineTo(x2, y2);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    void box(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        rect(x, y, w, h);
        fillColor(col(rgb, alpha));
        fill();
    }

    // readout tile, as sc_tile in stereo_scope_gfx.jsfx
    void tile(float x, float y, float w, float h, const char* name, const char* value)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 4.0f);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(x + 8.0f, y + 11.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, name, true);
        label(x + 8.0f, y + 31.0f, ALIGN_LEFT, 15.0f, 0.0f, theme::kText, value, true);
    }

    // bar meter from -1 to +1 filled from the centre to v, as sc_bar
    void bar(float x, float y, float w, float h, const char* name, float v, const char* lo, const char* mid, const char* hi)
    {
        label(x, y - 8.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, name, true);
        box(x, y, w, h, theme::kBackground);
        for (int i = 0; i <= 4; ++i)
            box(x + w * float(i) / 4.0f - (i == 4 ? 1.0f : 0.0f), y, 1.0f, h, theme::kBorder);
        const float cx = x + w * 0.5f, vx = x + w * (v + 1.0f) * 0.5f;
        box(std::fmin(cx, vx), y + 2.0f, std::fabs(vx - cx), h - 4.0f, theme::kAccent, 0.75f);
        box(vx - 1.0f, y - 2.0f, 2.0f, h + 4.0f, theme::kText);
        label(x, y + h + 9.0f, ALIGN_LEFT, 7.5f, 0.0f, theme::kMuted, lo);
        label(x + w * 0.5f, y + h + 9.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, mid);
        label(x + w, y + h + 9.0f, ALIGN_RIGHT, 7.5f, 0.0f, theme::kMuted, hi);
    }

    // a band's correlation: a thin bar from the centre, its label on the left (sc_bbar)
    void bandBar(float y, const char* name, float v)
    {
        const float x = 350.0f, w = 218.0f, h = 7.0f;
        label(318.0f, y + h * 0.5f, ALIGN_LEFT, 7.0f, 1.0f, theme::kMuted, name, true);
        box(x, y, w, h, theme::kBackground);
        for (int i = 0; i <= 4; ++i)
            box(x + w * float(i) / 4.0f - (i == 4 ? 1.0f : 0.0f), y, 1.0f, h, theme::kBorder);
        const float cx = x + w * 0.5f, vx = x + w * (v + 1.0f) * 0.5f;
        box(std::fmin(cx, vx), y + 1.0f, std::fabs(vx - cx), h - 2.0f, theme::kAccent, 0.75f);
        box(vx - 1.0f, y - 1.0f, 2.0f, h + 2.0f, theme::kText);
    }

    // as the @gfx of stereo_scope_gfx.jsfx: the goniometer on the left; readouts, correlation and
    // balance bars, levels and the correlation history on the right
    void drawDisplay() override
    {
        const nullscope::View* v = view();
        const float cx = 167.0f, cy = 204.0f, rad = 135.0f, d = 0.7071f;

        // ---- goniometer ----
        beginPath();
        circle(cx, cy, rad);
        fillColor(col(kBg2));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        beginPath();
        circle(cx, cy, rad * 0.5f);
        strokeColor(col(theme::kBorder, 0.6f));
        stroke();
        line(cx - rad * d, cy - rad * d, cx + rad * d, cy + rad * d, theme::kBorder, 0.6f);
        line(cx + rad * d, cy - rad * d, cx - rad * d, cy + rad * d, theme::kBorder, 0.6f);
        line(cx, cy - rad, cx, cy + rad, theme::kTrackHover, 1.0f);
        line(cx - rad, cy, cx + rad, cy, theme::kTrackHover, 1.0f);
        label(cx - 108.0f, cy - 108.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, "L", true);
        label(cx + 108.0f, cy - 108.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, "R", true);
        label(cx + 9.0f, cy - 127.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, "M", true);
        label(cx + 127.0f, cy - 9.0f, ALIGN_CENTER, 7.5f, 0.0f, theme::kMuted, "S", true);

        // scope gain: Auto follows the peak of what is drawn (fast down, slow up), else the knob
        const int n = 1024 + int(std::floor(value(2) / 100.0f * 7168.0f));
        const int rw = v ? v->rw.load(std::memory_order_acquire) : 0;
        auto at = [&](int i) { return (rw - 1 - i + nullscope::kRing) % nullscope::kRing; };
        float pk = 0.0f;
        if (v)
            for (int i = 0; i < n; ++i)
            {
                const int k = at(i);
                const float l = v->ringL[k], r = v->ringR[k];
                pk = std::fmax(pk, std::fmax(std::fabs(l + r), std::fabs(r - l)) * d);
            }
        const float t = pk > 0.0001f ? clampf(0.85f / pk, 0.25f, 64.0f) : 1.0f;
        if (t < fAutoGain)
            fAutoGain = t;
        else
            fAutoGain += (t - fAutoGain) * 0.08f;
        const bool autoOn = value(1) >= 0.5f;
        const float g = autoOn ? fAutoGain : std::pow(10.0f, value(0) / 20.0f);

        if (v)
        {
            static const float kAlpha[] = { 0.1f, 0.2f, 0.36f, 0.6f };
            for (int b = 0; b < 4; ++b)
            {
                beginPath();
                const int i0 = n * (4 - b) / 4 - 1, i1 = n * (3 - b) / 4;
                for (int i = i0; i >= i1; --i)
                {
                    const int k = at(i);
                    const float l = v->ringL[k] * g, r = v->ringR[k] * g;
                    const float x = clampf((r - l) * d, -1.0f, 1.0f), y = clampf((l + r) * d, -1.0f, 1.0f);
                    rect(cx + x * rad - 0.5f, cy - y * rad - 0.5f, 1.0f, 1.0f);
                }
                fillColor(col(theme::kAccent, kAlpha[b]));
                fill();
            }
        }
        char buf[48];
        std::snprintf(buf, sizeof(buf), autoOn ? "AUTO %+.1f dB" : "GAIN %+.1f dB", 20.0 * std::log10(double(g)));
        label(32.0f, 336.0f, ALIGN_LEFT, 7.5f, 0.0f, theme::kMuted, buf);

        // ---- readouts ----
        const double ll = v ? v->ll.load(std::memory_order_relaxed) : 0.0;
        const double rr = v ? v->rr.load(std::memory_order_relaxed) : 0.0;
        const double lr = v ? v->lr.load(std::memory_order_relaxed) : 0.0;
        const double c = nullscope::correlation(ll, rr, lr);
        const bool live = ll + rr > 0.0000000001;
        if (live)
            std::snprintf(buf, sizeof(buf), "%+.2f", c);
        else
            std::snprintf(buf, sizeof(buf), "--");
        tile(318.0f, 66.0f, 80.0f, 46.0f, "CORRELATION", buf);
        const double mm = (ll + rr + 2.0 * lr) / 4.0, ss = std::max(0.0, (ll + rr - 2.0 * lr) / 4.0);
        if (live)
        {
            const double w = mm > 0.0000000001 ? std::min(999.0, 100.0 * std::sqrt(ss / mm)) : 999.0;
            std::snprintf(buf, sizeof(buf), "%d%%", int(std::floor(w + 0.5)));
        }
        else
            std::snprintf(buf, sizeof(buf), "--");
        tile(403.0f, 66.0f, 80.0f, 46.0f, "WIDTH", buf);
        if (live)
        {
            const double bal = ll <= 0.0 ? 60.0 : rr <= 0.0 ? -60.0 : std::max(-60.0, std::min(60.0, 10.0 * std::log10(rr / ll)));
            if (std::fabs(bal) < 0.05)
                std::snprintf(buf, sizeof(buf), "C");
            else
                std::snprintf(buf, sizeof(buf), bal > 0.0 ? "R %.1f" : "L %.1f", std::fabs(bal));
        }
        else
            std::snprintf(buf, sizeof(buf), "--");
        tile(488.0f, 66.0f, 80.0f, 46.0f, "BALANCE", buf);

        // ---- meters ----
        if (value(5) >= 0.5f)
        {
            // Band Correlation: Low, Mid and High instead of the one bar
            label(318.0f, 124.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, "CORRELATION BY BAND  (-1 TO +1)", true);
            static const char* const kBands[] = { "LOW", "MID", "HIGH" };
            for (int b = 0; b < 3; ++b)
                bandBar(132.0f + 12.0f * float(b), kBands[b], v ? v->band[b].load(std::memory_order_relaxed) : 0.0f);
        }
        else
            bar(318.0f, 138.0f, 250.0f, 10.0f, "CORRELATION", float(c), "-1", "0", "+1");
        const double bp = live ? (rr - ll) / (rr + ll) : 0.0;
        bar(318.0f, 182.0f, 250.0f, 10.0f, "BALANCE", float(bp), "L", "C", "R");
        meter(318.0f, 214.0f, 250.0f, 7.0f, "L", v ? nulljsfx::toDb(v->pl.load(std::memory_order_relaxed)) : -140.0f, -140.0f, -60.0f);
        meter(318.0f, 228.0f, 250.0f, 7.0f, "R", v ? nulljsfx::toDb(v->pr.load(std::memory_order_relaxed)) : -140.0f, -140.0f, -60.0f);

        // ---- correlation history (5 s, newest on the right) ----
        setPlot(336.0f, 254.0f, 232.0f, 82.0f);
        grid(5, 4, false);
        const float my = std::round(fPY + fPH * 0.5f) + 0.5f;
        line(fPX, my, fPX + fPW, my, theme::kTrackHover, 1.0f);
        label(fPX - 5.0f, fPY, ALIGN_RIGHT, 7.5f, 0.0f, theme::kMuted, "+1");
        label(fPX - 5.0f, fPY + fPH * 0.5f, ALIGN_RIGHT, 7.5f, 0.0f, theme::kMuted, "0");
        label(fPX - 5.0f, fPY + fPH, ALIGN_RIGHT, 7.5f, 0.0f, theme::kMuted, "-1");
        if (v)
        {
            const int hw = v->hw.load(std::memory_order_acquire), hn = v->hn.load(std::memory_order_relaxed);
            constexpr int H = nullscope::kHist;
            if (hn > 1)
            {
                beginPath();
                for (int i = H - hn; i < H; ++i)
                {
                    const int k = (hw - H + i + H) % H;
                    const float x = fPX + fPW * float(i) / float(H - 1), y = linY(v->hist[k], -1.0f, 1.0f);
                    if (i == H - hn)
                        moveTo(x, y);
                    else
                        lineTo(x, y);
                }
                strokeColor(col(theme::kAccent));
                strokeWidth(2.0f);
                lineJoin(ROUND);
                stroke();
            }
        }
    }

    void formatValue(uint32_t param, float val, char* buf, size_t n) override
    {
        if (param == 0)
        {
            if (value(1) >= 0.5f)
                std::snprintf(buf, n, "AUTO");
            else
                std::snprintf(buf, n, "%+.1f dB", double(val));
        }
        else
            NullKnobsUI::formatValue(param, val, buf, n);
    }

private:
    float fAutoGain = 1.0f;
};

UI* createUI()
{
    return new UIStereoScope();
}

END_NAMESPACE_DISTRHO
