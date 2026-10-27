/*
 * NULL JSFX - VU Meter (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullUI.hpp"
#include "VuMeterSliders.hpp"

START_NAMESPACE_DISTRHO

static const NullKnobsUI::Control kControls[] = {
    { 0, NullKnobsUI::kSelect, "MODE", 0, -1, false },
    { 1, NullKnobsUI::kKnob, "REFERENCE", 0, -1, false },
    { 2, NullKnobsUI::kSelect, "PEAK HOLD", 0, -1, false },
    { 3, NullKnobsUI::kSelect, "MONITOR", 0, -1, false },
};

class UIVuMeter : public NullKnobsUI
{
public:
    UIVuMeter()
        : NullKnobsUI({ "VU METER", "ADVANCED", 480, 160, nullptr, 1, nullvu::kSliders, nullvu::kNumSliders, kControls,
                        sizeof(kControls) / sizeof(kControls[0]), true }) {}

protected:
    nullvu::View* view() const
    {
#if DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
        auto* p = static_cast<Plugin*>(getPluginInstancePointer());
        return p ? &static_cast<VuMeterBase*>(p)->view : nullptr;
#else
        return nullptr;
#endif
    }

    int mode() const { return int(std::lround(value(0))); }

    // vg_lo / vg_hi / vg_pos of vu_meter_gfx.jsfx: VU linear in voltage, the others in dB
    float lo() const { return mode() == 0 ? -20.0f : mode() == 1 ? -24.0f : -30.0f; }
    float hi() const { const int m = mode(); return m == 0 ? 3.0f : m == 1 ? 12.0f : m == 2 ? 14.0f : 20.0f; }
    float pos(float db) const
    {
        db = std::fmin(hi(), std::fmax(lo(), db));
        if (mode() == 0)
            return (std::pow(10.0f, db / 20.0f) - 0.1f) / (std::pow(10.0f, 3.0f / 20.0f) - 0.1f);
        return (db - lo()) / (hi() - lo());
    }

    // a point of the dial: pos 0..1 across -50..+50 degrees, r units from the pivot
    float px(float p, float r) const { return fPivX + r * std::sin((p * 100.0f - 50.0f) / 180.0f * float(M_PI)); }
    float py(float p, float r) const { return fPivY - r * std::cos((p * 100.0f - 50.0f) / 180.0f * float(M_PI)); }

    void line(float p0, float r0, float p1, float r1, const Color& c, float w)
    {
        beginPath();
        moveTo(px(p0, r0), py(p0, r0));
        lineTo(px(p1, r1), py(p1, r1));
        strokeColor(c);
        strokeWidth(w);
        stroke();
    }

    void tick(float db, bool major, bool lbl)
    {
        const float p = pos(db);
        const uint32_t c = db > 0.0f ? theme::kText : theme::kMuted;
        line(p, major ? 102.0f : 105.0f, p, 111.0f, col(c), 1.0f);
        if (lbl)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), db > 0.0f ? "+%d" : "%d", int(db));
            label(px(p, 122.0f), py(p, 122.0f), ALIGN_CENTER, 7.5f, 0.0f, c, buf);
        }
    }

    static void fmt(char* buf, size_t n, const char* pre, float v, float floor)
    {
        if (v <= floor)
            std::snprintf(buf, n, "%s-INF", pre);
        else if (v > 0.05f)
            std::snprintf(buf, n, "%s+%.1f", pre, double(v));
        else
            std::snprintf(buf, n, "%s%.1f", pre, double(v));
    }

    // vg_face: one meter, 208 x 144 at (x, y)
    void face(float x, float y, const char* name, float rd, float hd, bool over)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, 207.0f, 143.0f, 4.0f);
        fillColor(col(theme::kBackground));
        fill();
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        fPivX = x + 104.0f;
        fPivY = y + 150.0f;
        const float zero = pos(0.0f);
        for (int i = 0; i < 60; ++i)
        {
            const float p = float(i) / 60.0f, q = float(i + 1) / 60.0f;
            const bool up = p >= zero - 0.0001f;
            line(p, 111.0f, q, 111.0f, up ? col(theme::kText) : col(theme::kMuted, 0.7f), 1.0f);
        }
        if (mode() == 0)
        {
            static const float kTicks[][3] = { { -20, 1, 1 }, { -10, 1, 1 }, { -7, 1, 1 }, { -5, 1, 1 }, { -3, 1, 1 }, { -2, 0, 0 },
                                               { -1, 0, 0 },  { 0, 1, 1 },   { 1, 0, 0 },  { 2, 0, 0 },  { 3, 1, 1 } };
            for (const auto& t : kTicks)
                tick(t[0], t[1] != 0.0f, t[2] != 0.0f);
        }
        else
        {
            for (float db = lo(); db <= hi(); db += mode() == 1 ? 6.0f : 10.0f)
                tick(db, true, true);
            if (mode() == 2)
                tick(14.0f, true, true);
        }
        if (hd > lo())
            line(pos(hd), 96.0f, pos(hd), 114.0f, col(theme::kAccent), 1.0f);
        line(pos(rd), 46.0f, pos(rd), 116.0f, col(theme::kText), 2.0f);
        label(x + 10.0f, y + 12.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, name, true);
        label(x + 198.0f, y + 12.0f, ALIGN_RIGHT, 7.0f, 1.2f, over ? theme::kText : theme::kBorder, "OVER", true);
        char buf[32];
        fmt(buf, sizeof(buf), "", rd, lo());
        label(x + 10.0f, y + 130.0f, ALIGN_LEFT, 13.0f, 0.0f, theme::kText, buf, true);
        fmt(buf, sizeof(buf), "MAX ", hd, lo());
        label(x + 198.0f, y + 130.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kMuted, buf, true);
    }

    void drawDisplay() override
    {
        const nullvu::View* v = view();
        for (int k = 0; k < 2; ++k)
            face(fVX + 224.0f * float(k), fVY, k ? "R" : "L", v ? v->y[k].load(std::memory_order_relaxed) : -200.0f,
                 v ? v->h[k].load(std::memory_order_relaxed) : -200.0f, v && v->over[k].load(std::memory_order_relaxed));
        char buf[48];
        const int ref = int(std::lround(value(1)));
        switch (mode())
        {
        case 0: std::snprintf(buf, sizeof(buf), "VU   0 VU = %d dBFS", ref); break;
        case 1: std::snprintf(buf, sizeof(buf), "PPM   0 = %d dBFS", ref); break;
        case 2: std::snprintf(buf, sizeof(buf), "K-14   0 = -14 dBFS RMS"); break;
        default: std::snprintf(buf, sizeof(buf), "K-20   0 = -20 dBFS RMS"); break;
        }
        for (int k = 0; k < 2; ++k)
            label(fVX + 104.0f + 224.0f * float(k), fVY + 12.0f, ALIGN_CENTER, 7.0f, 1.2f, theme::kMuted, buf, true);
    }

    // a click on the display resets the held readings
    bool onMouse(const MouseEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (ev.button == 1 && ev.press && !presetOpen() && x >= fVX && x < fVX + fVW && y >= fVY && y < fVY + fVH)
        {
            if (nullvu::View* v = view())
                v->resetRequest.store(true, std::memory_order_relaxed);
            return true;
        }
        return NullKnobsUI::onMouse(ev);
    }

    void formatValue(uint32_t param, float v, char* buf, size_t n) override
    {
        if (param == 1)
            std::snprintf(buf, n, "%d dBFS", int(std::lround(v)));
        else
            NullKnobsUI::formatValue(param, v, buf, n);
    }

private:
    float fPivX = 0.0f, fPivY = 0.0f;
};

UI* createUI()
{
    return new UIVuMeter();
}

END_NAMESPACE_DISTRHO
