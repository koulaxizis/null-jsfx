/*
 * NULL JSFX - Rack (VST3/CLAP), interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * 960 x 680 design units, resizable. Header; top bar (preset, meters, crossovers, input, output,
 * mix); the chain (8 slots) on the left, with the feedback loop's range drawn beside it; the
 * selected module's controls (built from its SliderInfo: knobs for numbers, selectors for enums,
 * rows of 6, pages past 3 rows; lane, band, channel and mix in its header) or the modulation
 * routes on the right; macros, LFO 1, LFO 2 and the envelope along the bottom, then the step
 * sequencer (16 bars to draw on) and the lanes and feedback loop below them.
 * The module chooser and the preset list open over the window.
 *
 * Slots, routes and the preset name are plugin state (setState); everything else is a
 * parameter. The interface reads the engine directly (direct access) for module names, the
 * modulated values, the sources and the meters.
 */

#include "NullUI.hpp"
#include "RackPlugin.hpp"

#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace rack;

namespace {

constexpr uint32_t kSelFill = 0x2d2b28; // selected tab / row (as NullKnobsUI's tabs)

// what a click area does
enum HitKind {
    kHitNone = 0,
    kHitKnob,        // a: parameter
    kHitSelect,      // a: parameter (left half steps down, right half up)
    kHitSlot,        // a: slot (select, drag to reorder)
    kHitSlotOn,      // a: slot
    kHitSlotUp,      // a: slot
    kHitSlotDown,    // a: slot
    kHitAddModule,   // a: slot (opens the chooser)
    kHitPick,        // a: module index, -1 = remove
    kHitPickClose,
    kHitPresetPrev,
    kHitPresetNext,
    kHitPresetOpen,
    kHitPresetPick,  // a: preset index
    kHitTab,         // a: page
    kHitModView,     // toggles the routes view
    kHitRouteAdd,
    kHitRouteSource, // a: route
    kHitRouteSlot,   // a: route
    kHitRouteValue,  // a: route
    kHitRouteDepth,  // a: route
    kHitRouteDelete, // a: route
    kHitModuleView,  // back to the module
    kHitPopupBack,   // the panel of a popup (swallows clicks)
    kHitSeqBars,     // the step sequencer's bars (click / drag to draw)
};

struct Hit {
    float x, y, w, h;
    int kind, a;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

} // namespace

class UIRack : public NullUI
{
public:
    UIRack()
        : NullUI(960, 680)
    {
        fHits.reserve(512);
        for (uint32_t i = 0; i < kNumParams; ++i)
        {
            char n[32], s[32];
            fParam[i] = paramInfo(i, n, sizeof(n), s, sizeof(s)).def;
        }
        syncFromPlugin(true);
    }

protected:
    // ---- plugin access -----------------------------------------------------------------------

    RackPluginBase* plugin() const { return static_cast<RackPluginBase*>(getPluginInstancePointer()); }
    RackEngine* engine() const
    {
        RackPluginBase* p = plugin();
        return p ? &p->engine() : nullptr;
    }

    const nulljsfx::ModuleDesc* slotDesc(uint32_t s) const
    {
        RackEngine* e = engine();
        return e && s < kSlots ? e->find(fIds[s].c_str()) : nullptr;
    }

    void syncFromPlugin(bool force)
    {
        RackPluginBase* p = plugin();
        if (!p)
            return;
        RackEngine& e = p->engine();
        if (force || p->stateVersion() != fSeenVersion)
        {
            fSeenVersion = p->stateVersion();
            for (uint32_t i = 0; i < kSlots; ++i)
                fIds[i] = e.slotId(i);
            fNumRoutes = e.routes(fRoutes);
            fPresetName = e.presetName();
        }
        const uint32_t now = fFrame;
        for (uint32_t i = 0; i < kNumParams; ++i)
            if (force || now - fEditFrame[i] > 20)
                fParam[i] = e.param(i);
    }

    void parameterChanged(uint32_t index, float v) override
    {
        if (index < kNumParams)
        {
            fParam[index] = v;
            repaint();
        }
    }

    void programLoaded(uint32_t) override
    {
        syncFromPlugin(true);
        repaint();
    }

    void stateChanged(const char*, const char*) override { repaint(); }

    void uiIdle() override
    {
        ++fFrame;
        syncFromPlugin(false);
        repaint();
    }

    // ---- parameter info, mapping and text ----------------------------------------------------

    bool isSlotValue(uint32_t p, uint32_t* slot = nullptr, uint32_t* v = nullptr) const
    {
        if (p < kNumGlobals || p >= kExtFirst)
            return false;
        const uint32_t k = p - kNumGlobals;
        if (k % kSlotParams < kSlotValue1)
            return false;
        if (slot)
            *slot = k / kSlotParams;
        if (v)
            *v = k % kSlotParams - kSlotValue1;
        return true;
    }

    // the slider behind a parameter: the module's for slot values, else the parameter's own
    SliderInfo info(uint32_t p, bool& log) const
    {
        uint32_t s, v;
        if (isSlotValue(p, &s, &v))
        {
            const nulljsfx::ModuleDesc* d = slotDesc(s);
            if (d && v < d->numSliders)
            {
                log = isLogSlider(d->id, v);
                return d->sliders[v];
            }
            log = false;
            return SliderInfo { "", "", 0.0f, 1.0f, 0.0f, "" };
        }
        char n[32], sy[32];
        return paramInfo(p, n, sizeof(n), sy, sizeof(sy), &log);
    }

    // knob position 0..1 for the current value, and the value for a knob position
    float knobPos(uint32_t p) const
    {
        if (isSlotValue(p))
            return fParam[p];
        bool log;
        const SliderInfo in = info(p, log);
        return plainToValue(in, log, fParam[p]);
    }
    float fromKnobPos(uint32_t p, float t) const
    {
        bool log;
        const SliderInfo in = info(p, log);
        if (isSlotValue(p))
            return plainToValue(in, log, valueToPlain(in, log, t));
        return valueToPlain(in, log, t);
    }
    float plainOf(uint32_t p) const
    {
        if (!isSlotValue(p))
            return fParam[p];
        bool log;
        const SliderInfo in = info(p, log);
        return valueToPlain(in, log, fParam[p]);
    }
    float defaultOf(uint32_t p) const
    {
        bool log;
        const SliderInfo in = info(p, log);
        return isSlotValue(p) ? plainToValue(in, log, in.def) : in.def;
    }
    bool bipolar(uint32_t p) const
    {
        bool log;
        const SliderInfo in = info(p, log);
        return in.min < 0.0f && in.max > 0.0f && std::fabs(in.min + in.max) < 1e-3f;
    }

    static void formatPlain(const SliderInfo& in, float v, char* buf, size_t n)
    {
        if (in.options)
        {
            const int k = std::max(0, std::min(int(in.max - in.min + 0.5f), int(std::lround(v - in.min))));
            std::snprintf(buf, n, "%s", in.options[k]);
            return;
        }
        const char* unit = in.unit ? in.unit : "";
        double x = v;
        if (std::strcmp(unit, "Hz") == 0 && std::fabs(x) >= 1000.0)
        {
            x /= 1000.0;
            unit = "kHz";
            std::snprintf(buf, n, std::fabs(x) >= 10.0 ? "%.1f kHz" : "%.2f kHz", x);
            return;
        }
        if (std::strcmp(unit, "ms") == 0 && std::fabs(x) >= 1000.0)
        {
            std::snprintf(buf, n, "%.2f s", x / 1000.0);
            return;
        }
        const char* sep = (unit[0] && std::strcmp(unit, "%") != 0) ? " " : "";
        const bool plus = in.min < 0.0f && x > 0.0;
        int dec = 0;
        const float range = in.max - in.min;
        if (in.step > 0.0f && in.step < 1.0f)
            dec = in.step < 0.0999f ? 2 : 1;
        else if (in.step <= 0.0f)
        {
            // continuous: whole numbers for 0..100-like amounts, else by size
            if (range >= 100.0f && (!in.unit[0] || std::strcmp(in.unit, "%") == 0))
                dec = 0;
            else if (range <= 2.0f || std::fabs(x) < 1.0)
                dec = 2;
            else if (std::fabs(x) < 10.0)
                dec = 1;
        }
        if (dec == 0 && in.step <= 0.0f)
            x = std::round(x);
        if (std::fabs(x) < 0.5 * std::pow(10.0, -dec))
            x = 0.0;
        std::snprintf(buf, n, plus ? "+%.*f%s%s" : "%.*f%s%s", dec, x, sep, unit);
    }

    void formatParam(uint32_t p, char* buf, size_t n) const
    {
        bool log;
        const SliderInfo in = info(p, log);
        formatPlain(in, plainOf(p), buf, n);
    }

    static void upper(const char* in, char* out, size_t n)
    {
        size_t i = 0;
        for (; in[i] && i + 1 < n; ++i)
            out[i] = char(std::toupper((unsigned char)in[i]));
        out[i] = '\0';
    }

    // ---- parameter edits ---------------------------------------------------------------------

    void setParam(uint32_t p, float v)
    {
        if (p >= kNumParams)
            return;
        fEditFrame[p] = fFrame;
        if (v == fParam[p])
            return;
        fParam[p] = v;
        setParameterValue(p, v);
        repaint();
    }

    void setParamGesture(uint32_t p, float v)
    {
        editParameter(p, true);
        setParam(p, v);
        editParameter(p, false);
    }

    void stepParam(uint32_t p, int dir, bool wrap)
    {
        bool log;
        const SliderInfo in = info(p, log);
        if (in.options || in.step >= 1.0f)
        {
            float plain = std::round(plainOf(p)) + float(dir) * (in.step >= 1.0f ? in.step : 1.0f);
            if (wrap && plain > in.max + 0.01f)
                plain = in.min;
            else if (wrap && plain < in.min - 0.01f)
                plain = in.max;
            plain = clampf(plain, in.min, in.max);
            if (!in.options && log)
            {
                // integer log sliders: 1% of the knob per step
                setParamGesture(p, fromKnobPos(p, clampf(knobPos(p) + float(dir) * 0.01f, 0.0f, 1.0f)));
                return;
            }
            setParamGesture(p, isSlotValue(p) ? plainToValue(in, log, plain) : plain);
            return;
        }
        setParamGesture(p, fromKnobPos(p, clampf(knobPos(p) + float(dir) * 0.01f, 0.0f, 1.0f)));
    }

    // ---- state edits -------------------------------------------------------------------------

    void sendSlots()
    {
        setState("slots", formatSlots(fIds).c_str());
    }

    void sendRoutes()
    {
        setState("routes", formatRoutes(fRoutes, fNumRoutes).c_str());
    }

    void markEdited()
    {
        if (fPresetName.empty() || fPresetName.back() != '*')
        {
            fPresetName += fPresetName.empty() ? "" : " *";
            setState("preset", fPresetName.c_str());
        }
    }

    void setModule(uint32_t s, const char* id)
    {
        if (s >= kSlots || fIds[s] == id)
            return;
        fIds[s] = id;
        sendSlots();
        // the plugin reset the slot's values to the module's defaults: tell the host
        const nulljsfx::ModuleDesc* d = slotDesc(s);
        for (uint32_t v = 0; v < kValues; ++v)
        {
            const float def = d && v < d->numSliders ? plainToValue(d->sliders[v], isLogSlider(d->id, v), d->sliders[v].def) : 0.0f;
            fParam[valueParam(s, v)] = -1.0f;
            setParamGesture(valueParam(s, v), def);
        }
        // routes to the old module's values no longer mean anything
        uint32_t n = 0;
        for (uint32_t i = 0; i < fNumRoutes; ++i)
            if (fRoutes[i].slot != int(s))
                fRoutes[n++] = fRoutes[i];
        if (n != fNumRoutes)
        {
            fNumRoutes = n;
            sendRoutes();
        }
        markEdited();
    }

    // moves slot a to position b (the others shift)
    void moveSlot(uint32_t a, uint32_t b)
    {
        if (a >= kSlots || b >= kSlots || a == b)
            return;
        int order[kSlots]; // new position i takes old slot order[i]
        int k = 0;
        for (int i = 0; i < int(kSlots); ++i)
            if (i != int(a))
                order[k++] = i;
        for (int i = int(kSlots) - 1; i > int(b); --i)
            order[i] = order[i - 1];
        order[b] = int(a);
        std::string ids[kSlots];
        float vals[kSlots][kSlotParams], lanes[kSlots];
        for (uint32_t i = 0; i < kSlots; ++i)
        {
            ids[i] = fIds[order[i]];
            for (uint32_t p = 0; p < kSlotParams; ++p)
                vals[i][p] = fParam[slotParam(uint32_t(order[i]), p)];
            lanes[i] = fParam[laneParam(uint32_t(order[i]))];
        }
        for (uint32_t i = 0; i < kSlots; ++i)
            fIds[i] = ids[i];
        sendSlots();
        // the engine moved the instances with their parameters; the host learns them here
        for (uint32_t i = 0; i < kSlots; ++i)
            if (order[i] != int(i))
            {
                for (uint32_t p = 0; p < kSlotParams; ++p)
                {
                    fParam[slotParam(i, p)] = -1.0f;
                    setParamGesture(slotParam(i, p), vals[i][p]);
                }
                fParam[laneParam(i)] = -1.0f;
                setParamGesture(laneParam(i), lanes[i]);
            }
        int where[kSlots];
        for (int i = 0; i < int(kSlots); ++i)
            where[order[i]] = i;
        for (uint32_t i = 0; i < fNumRoutes; ++i)
            fRoutes[i].slot = where[fRoutes[i].slot];
        if (fNumRoutes)
            sendRoutes();
        // the feedback loop's ends follow their slots
        for (uint32_t p : { uint32_t(kFbFrom), uint32_t(kFbTo) })
        {
            const int s = std::max(0, std::min(int(kSlots) - 1, int(std::lround(fParam[p])) - 1));
            if (where[s] != s)
                setParamGesture(p, float(where[s] + 1));
        }
        if (fSel == int(a))
            fSel = int(b);
        else
            fSel = where[fSel];
        markEdited();
    }

    int presetIndex() const
    {
        std::string n = fPresetName;
        if (n.size() > 2 && n.compare(n.size() - 2, 2, " *") == 0)
            n.resize(n.size() - 2);
        for (uint32_t i = 0; i < kNumPresets; ++i)
            if (n == kPresets[i].name)
                return int(i);
        return -1;
    }

    void applyPreset(uint32_t index)
    {
        RackEngine* e = engine();
        if (!e || index >= kNumPresets)
            return;
        std::unique_ptr<RackEngine::PresetState> st(new RackEngine::PresetState());
        e->resolvePreset(kPresets[index], *st);
        std::string none[kSlots];
        setState("slots", formatSlots(none).c_str()); // fresh modules everywhere
        for (uint32_t i = 0; i < kSlots; ++i)
            fIds[i] = st->ids[i];
        sendSlots();
        for (uint32_t i = 0; i < st->numRoutes; ++i)
            fRoutes[i] = st->routes[i];
        fNumRoutes = st->numRoutes;
        sendRoutes();
        fPresetName = kPresets[index].name;
        setState("preset", fPresetName.c_str());
        for (uint32_t i = 0; i < kNumParams; ++i)
        {
            fParam[i] = st->params[i] + 1.0f; // force the send
            setParamGesture(i, st->params[i]);
        }
        fSel = 0;
        for (uint32_t i = 0; i < kSlots; ++i)
            if (!fIds[i].empty())
            {
                fSel = int(i);
                break;
            }
        fPage = 0;
        fRouteScroll = 0;
    }

    // ---- lanes and the feedback loop ---------------------------------------------------------

    // a slot with a module on lane B: the rack runs two lanes
    bool anyLaneB() const
    {
        for (uint32_t i = 0; i < kSlots; ++i)
            if (!fIds[i].empty() && fParam[laneParam(i)] >= 0.5f)
                return true;
        return false;
    }

    // the loop's slots (0-based, from <= to), as the engine reads them
    void fbRange(int& from, int& to) const
    {
        from = std::max(0, std::min(int(kSlots) - 1, int(std::lround(fParam[kFbFrom])) - 1));
        to = std::max(0, std::min(int(kSlots) - 1, int(std::lround(fParam[kFbTo])) - 1));
        if (from > to)
            std::swap(from, to);
    }

    // ---- routes ------------------------------------------------------------------------------

    int nextModuleSlot(int from, int dir) const
    {
        for (int k = 1; k <= int(kSlots); ++k)
        {
            const int s = ((from + dir * k) % int(kSlots) + int(kSlots)) % int(kSlots);
            if (slotDesc(uint32_t(s)))
                return s;
        }
        return -1;
    }

    void addRoute()
    {
        if (fNumRoutes >= kMaxRoutes)
            return;
        int s = fSel >= 0 && slotDesc(uint32_t(fSel)) ? fSel : nextModuleSlot(-1, 1);
        if (s < 0)
            return;
        Route r;
        r.source = kSrcMacro1;
        r.slot = s;
        r.value = 0;
        r.depth = 0.5f;
        fRoutes[fNumRoutes++] = r;
        sendRoutes();
        markEdited();
    }

    void deleteRoute(int i)
    {
        if (i < 0 || i >= int(fNumRoutes))
            return;
        for (uint32_t k = uint32_t(i); k + 1 < fNumRoutes; ++k)
            fRoutes[k] = fRoutes[k + 1];
        --fNumRoutes;
        sendRoutes();
        markEdited();
    }

    // the modulation range of a slot value: lowest and highest the routes can push it to
    bool modRange(uint32_t slot, uint32_t v, float& lo, float& hi) const
    {
        lo = hi = 0.0f;
        bool any = false;
        for (uint32_t i = 0; i < fNumRoutes; ++i)
        {
            const Route& r = fRoutes[i];
            if (r.slot != int(slot) || r.value != int(v))
                continue;
            any = true;
            const float smin = r.source == kSrcLfo1 || r.source == kSrcLfo2 ? -1.0f : 0.0f;
            lo += std::fmin(r.depth * smin, r.depth);
            hi += std::fmax(r.depth * smin, r.depth);
        }
        return any;
    }

    // ---- drawing helpers ---------------------------------------------------------------------

    void addHit(float x, float y, float w, float h, int kind, int a = 0)
    {
        fHits.push_back({ x, y, w, h, kind, a });
    }

    bool hovered(int kind, int a) const
    {
        return fHover >= 0 && fHover < int(fHits.size()) && fHits[fHover].kind == kind && fHits[fHover].a == a;
    }

    float textWidth(const char* s, float size, float spacing, bool bold)
    {
        if (bold)
            fontFace("noto-bold");
        fontSize(size * kNotoLineToEm);
        textLetterSpacing(spacing);
        Rectangle<float> b;
        const float w = textBounds(0, 0, s, nullptr, b);
        textLetterSpacing(0.0f);
        if (bold)
            fontFace("noto");
        return w;
    }

    // a label that shrinks its letter spacing, then its size, to fit maxW
    void fitLabel(float x, float y, int align, float size, float spacing, uint32_t rgb, const char* s, float maxW, bool bold = true)
    {
        float w = textWidth(s, size, spacing, bold);
        if (w > maxW && spacing > 0.0f)
        {
            spacing = 0.3f;
            w = textWidth(s, size, spacing, bold);
        }
        while (w > maxW && size > 5.5f)
        {
            size -= 0.5f;
            w = textWidth(s, size, spacing, bold);
        }
        label(x, y, align, size, spacing, rgb, s, bold);
    }

    // text cut with an ellipsis to fit maxW
    void clipLabel(float x, float y, int align, float size, float spacing, uint32_t rgb, const char* s, float maxW, bool bold)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s", s);
        size_t n = std::strlen(buf);
        while (n > 1 && textWidth(buf, size, spacing, bold) > maxW)
        {
            buf[--n] = '\0';
            if (n > 2)
            {
                buf[n - 1] = '.';
                buf[n - 2] = '.';
            }
        }
        label(x, y, align, size, spacing, rgb, buf, bold);
    }

    void panel(float x, float y, float w, float h, uint32_t fillRgb, uint32_t edge, float radius = 4.0f)
    {
        beginPath();
        roundedRect(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, radius);
        fillColor(col(fillRgb));
        fill();
        strokeColor(col(edge));
        strokeWidth(1.0f);
        stroke();
    }

    void hline(float x0, float x1, float y, uint32_t rgb, float alpha = 1.0f)
    {
        beginPath();
        moveTo(x0, std::round(y) + 0.5f);
        lineTo(x1, std::round(y) + 0.5f);
        strokeColor(col(rgb, alpha));
        strokeWidth(1.0f);
        stroke();
    }

    // knob with label above and value below (the NullKnobsUI cell), plus the modulation ring
    void knobCell(float cx, float top, uint32_t p, const char* name, float r = 14.0f, float cellW = 100.0f,
                  float labelDy = 10.0f, float knobDy = 44.0f, float valueDy = 77.0f, bool dim = false,
                  const char* valueText = nullptr)
    {
        const bool lit = hovered(kHitKnob, int(p)) || fDragParam == int(p);
        fitLabel(cx, top + labelDy, ALIGN_CENTER, 7.5f, 1.5f, theme::kMuted, name, cellW - 6.0f);
        const float ky = top + knobDy;
        if (dim)
            globalAlpha(0.45f);
        drawKnob(cx, ky, r, knobPos(p), bipolar(p), lit);
        globalAlpha(1.0f);
        uint32_t s, v;
        float lo, hi;
        if (isSlotValue(p, &s, &v) && modRange(s, v, lo, hi))
            modRing(cx, ky, r + 16.0f, fParam[p], lo, hi, engine() ? engine()->effective(s, v) : fParam[p]);
        char buf[48];
        formatParam(p, buf, sizeof(buf));
        fitLabel(cx, top + valueDy, ALIGN_CENTER, 8.5f, 0.0f, theme::kText, valueText ? valueText : buf, cellW - 4.0f, false);
        const float hr = r + 12.0f;
        addHit(cx - hr, ky - hr, hr * 2.0f, hr * 2.0f, kHitKnob, int(p));
    }

    // the arc of what modulation can reach (faint) and where it is now (bright dot)
    void modRing(float x, float y, float rr, float base, float lo, float hi, float eff)
    {
        const float amax = 0.75f * float(M_PI), off = -0.5f * float(M_PI);
        const float t0 = clampf(base + lo, 0.0f, 1.0f), t1 = clampf(base + hi, 0.0f, 1.0f);
        const float a0 = (t0 * 2.0f - 1.0f) * amax, a1 = (t1 * 2.0f - 1.0f) * amax;
        beginPath();
        arc(x, y, rr, -amax + off, amax + off, CW);
        strokeColor(col(theme::kBorder, 0.6f));
        strokeWidth(1.0f);
        stroke();
        if (a1 - a0 > 0.001f)
        {
            beginPath();
            arc(x, y, rr, a0 + off, a1 + off, CW);
            strokeColor(col(theme::kAccent, 0.45f));
            strokeWidth(2.0f);
            stroke();
        }
        const float ae = (clampf(eff, 0.0f, 1.0f) * 2.0f - 1.0f) * amax;
        beginPath();
        circle(x + std::sin(ae) * rr, y - std::cos(ae) * rr, 2.6f);
        fillColor(col(theme::kText));
        fill();
    }

    // selector pill with arrows (the NullKnobsUI selector)
    void selector(float cx, float cy, float w, uint32_t p, const char* text = nullptr, float h = 21.0f)
    {
        const bool lit = hovered(kHitSelect, int(p));
        panel(cx - w * 0.5f, cy - h * 0.5f, w, h, theme::kBackground, lit ? theme::kTrackHover : theme::kBorder);
        char buf[48];
        if (!text)
        {
            bool log;
            const SliderInfo in = info(p, log);
            formatPlain(in, plainOf(p), buf, sizeof(buf));
            char up[48];
            upper(buf, up, sizeof(up));
            std::snprintf(buf, sizeof(buf), "%s", up);
            text = buf;
        }
        fitLabel(cx, cy, ALIGN_CENTER, 8.0f, 0.5f, theme::kText, text, w - 26.0f);
        drawArrow(cx - w * 0.5f + 7.0f, cy, -1.0f);
        drawArrow(cx + w * 0.5f - 7.0f, cy, 1.0f);
        addHit(cx - w * 0.5f, cy - h * 0.5f, w, h, kHitSelect, int(p));
    }

    // compact knob: knob on the left, label and value on its right
    void compactKnob(float x, float cy, uint32_t p, const char* name, float w = 92.0f)
    {
        const bool lit = hovered(kHitKnob, int(p)) || fDragParam == int(p);
        drawKnob(x + 19.0f, cy + 1.0f, 8.0f, knobPos(p), bipolar(p), lit);
        fitLabel(x + 42.0f, cy - 7.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, name, w - 44.0f);
        char buf[48];
        formatParam(p, buf, sizeof(buf));
        fitLabel(x + 42.0f, cy + 8.0f, ALIGN_LEFT, 9.0f, 0.0f, theme::kText, buf, w - 44.0f, false);
        addHit(x, cy - 22.0f, w, 44.0f, kHitKnob, int(p));
    }

    void button(float x, float y, float w, float h, const char* text, int kind, int a, bool active = false)
    {
        const bool lit = hovered(kind, a);
        panel(x, y, w, h, active ? kSelFill : theme::kBackground, active || lit ? theme::kTrackHover : theme::kBorder);
        label(x + w * 0.5f, y + h * 0.5f, ALIGN_CENTER, 7.5f, 1.2f, active || lit ? theme::kText : theme::kMuted, text, true);
        addHit(x, y, w, h, kind, a);
    }

    // ---- the window --------------------------------------------------------------------------

    void onNanoDisplay() override
    {
        const float s = uiScale();
        scale(s, s);
        fHits.clear();

        beginPath();
        rect(0, 0, kWidth, kHeight);
        fillColor(col(theme::kBackground));
        fill();
        fontFace("noto");

        if (fSel < 0 || fSel >= int(kSlots))
            fSel = 0;

        drawHeader("RACK", "RACK");
        drawTopBar();
        drawChain();
        if (fModView)
            drawRoutes();
        else
            drawModule();
        drawMacros();
        drawLfo(0, 446.0f);
        drawLfo(1, 614.0f);
        drawEnvelope();
        drawSeq();
        drawLanesLoop();

        if (fPopup == kPopupModules)
            drawModulePicker();
        else if (fPopup == kPopupPresets)
            drawPresetList();
    }

    // preset, meters, crossovers, input, output, mix
    void drawTopBar()
    {
        card(14.0f, 54.0f, 932.0f, 56.0f);
        const float cy = 82.0f;

        // preset selector
        const float px = 24.0f, pw = 276.0f, ph = 30.0f, py = cy - ph * 0.5f;
        const bool lit = hovered(kHitPresetOpen, 0) || fPopup == kPopupPresets;
        panel(px, py, pw, ph, theme::kBackground, lit ? theme::kTrackHover : theme::kBorder, 5.0f);
        addHit(px, py, 26.0f, ph, kHitPresetPrev);
        addHit(px + pw - 26.0f, py, 26.0f, ph, kHitPresetNext);
        addHit(px + 26.0f, py, pw - 52.0f, ph, kHitPresetOpen);
        drawArrow(px + 13.0f, cy, -1.0f);
        drawArrow(px + pw - 13.0f, cy, 1.0f);
        beginPath();
        moveTo(px + 26.5f, py + 6.0f); lineTo(px + 26.5f, py + ph - 6.0f);
        moveTo(px + pw - 26.5f, py + 6.0f); lineTo(px + pw - 26.5f, py + ph - 6.0f);
        strokeColor(col(theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        label(px + 36.0f, cy, ALIGN_LEFT, 7.0f, 1.5f, theme::kMuted, "PRESET", true);
        clipLabel(px + 86.0f, cy, ALIGN_LEFT, 10.0f, 0.3f, theme::kText, fPresetName.empty() ? "Init" : fPresetName.c_str(),
                  pw - 86.0f - 34.0f, true);

        // meters
        RackEngine* e = engine();
        const float in = e ? e->inputPeak() : 0.0f, out = e ? e->outputPeak() : 0.0f;
        meter(314.0f, cy - 8.0f, 112.0f, 5.0f, "IN", nulljsfx::toDb(in), nulljsfx::toDb(in), -60.0f);
        meter(314.0f, cy + 4.0f, 112.0f, 5.0f, "OUT", nulljsfx::toDb(out), nulljsfx::toDb(out), -60.0f);

        // latency
        char buf[48];
        const uint32_t lat = e ? e->latency() : 0;
        label(442.0f, cy - 7.0f, ALIGN_LEFT, 7.0f, 1.2f, theme::kMuted, "LATENCY", true);
        if (lat && e)
            std::snprintf(buf, sizeof(buf), "%.1f ms", 1000.0 * lat / e->sampleRate());
        else
            std::snprintf(buf, sizeof(buf), "0 ms");
        label(442.0f, cy + 8.0f, ALIGN_LEFT, 9.0f, 0.0f, theme::kText, buf);

        vline(510.0f, 64.0f, 100.0f);
        compactKnob(516.0f, cy, kXoverLow, "LOW-MID", 86.0f);
        compactKnob(602.0f, cy, kXoverHigh, "MID-HIGH", 86.0f);
        vline(690.0f, 64.0f, 100.0f);
        compactKnob(696.0f, cy, kInput, "INPUT", 84.0f);
        compactKnob(780.0f, cy, kOutput, "OUTPUT", 84.0f);
        compactKnob(864.0f, cy, kMix, "MIX", 76.0f);
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

    // ---- the chain ---------------------------------------------------------------------------

    static constexpr float kChainX = 14.0f, kChainY = 118.0f, kChainW = 268.0f, kChainH = 318.0f;
    static constexpr float kRowX = 22.0f, kRowW = 252.0f, kRowY0 = 146.0f, kRowH = 35.0f;

    void drawChain()
    {
        card(kChainX, kChainY, kChainW, kChainH);
        label(kRowX + 4.0f, 132.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, "CHAIN", true);
        int used = 0;
        for (uint32_t i = 0; i < kSlots; ++i)
            used += slotDesc(i) ? 1 : 0;
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%d / 8", used);
        label(kRowX + kRowW - 4.0f, 132.0f, ALIGN_RIGHT, 7.5f, 1.2f, theme::kMuted, buf, true);

        for (uint32_t i = 0; i < kSlots; ++i)
            drawSlotRow(i, kRowY0 + float(i) * kRowH);

        // the feedback loop: a bracket left of the rows, from the output of To back to From
        if (fParam[kFeedback] > 0.0f)
        {
            int from, to;
            fbRange(from, to);
            const float h = kRowH - 4.0f, x = kRowX - 4.5f;
            const float y0 = kRowY0 + float(from) * kRowH + h * 0.5f, y1 = kRowY0 + float(to) * kRowH + h * 0.5f;
            beginPath();
            moveTo(kRowX, y1);
            lineTo(x, y1);
            lineTo(x, y0);
            lineTo(kRowX, y0);
            strokeColor(col(theme::kAccent, 0.85f));
            strokeWidth(1.5f);
            lineJoin(ROUND);
            stroke();
            // arrowhead into From
            beginPath();
            moveTo(kRowX + 0.5f, y0);
            lineTo(kRowX - 3.0f, y0 - 2.5f);
            lineTo(kRowX - 3.0f, y0 + 2.5f);
            closePath();
            fillColor(col(theme::kAccent));
            fill();
        }

        if (fSlotDrag >= 0 && fSlotDragging)
        {
            // where the dragged slot will land
            const int to = dropIndex();
            const float y = kRowY0 + float(to) * kRowH + (to > fSlotDrag ? kRowH - 2.0f : -2.0f) + 0.5f;
            beginPath();
            moveTo(kRowX, std::round(y) + 0.5f);
            lineTo(kRowX + kRowW, std::round(y) + 0.5f);
            strokeColor(col(theme::kAccent));
            strokeWidth(2.0f);
            stroke();
        }
    }

    int dropIndex() const
    {
        const int k = int(std::floor((fSlotDragY - kRowY0) / kRowH));
        return std::max(0, std::min(int(kSlots) - 1, k));
    }

    void drawSlotRow(uint32_t i, float y)
    {
        const nulljsfx::ModuleDesc* d = slotDesc(i);
        const bool sel = int(i) == fSel && !fModView;
        const bool hov = hovered(kHitSlot, int(i)) || hovered(kHitSlotOn, int(i)) || hovered(kHitSlotUp, int(i)) ||
                         hovered(kHitSlotDown, int(i)) || hovered(kHitAddModule, int(i));
        const float h = kRowH - 4.0f;
        const bool dragged = fSlotDragging && fSlotDrag == int(i);
        if (dragged)
            globalAlpha(0.5f);
        panel(kRowX, y, kRowW, h, sel ? kSelFill : theme::kBackground, sel || hov ? theme::kTrackHover : theme::kBorder, 5.0f);
        addHit(kRowX, y, kRowW, h, d ? kHitSlot : kHitAddModule, int(i));
        const float cy = y + h * 0.5f;

        // number
        beginPath();
        circle(kRowX + 15.0f, cy, 8.5f);
        fillColor(col(sel ? theme::kTrackHover : theme::kCard));
        fill();
        strokeColor(col(d ? theme::kTrackHover : theme::kBorder));
        strokeWidth(1.0f);
        stroke();
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%u", i + 1);
        label(kRowX + 15.0f, cy, ALIGN_CENTER, 7.5f, 0.0f, d ? theme::kText : theme::kMuted, buf, true);

        if (!d)
        {
            label(kRowX + 32.0f, cy, ALIGN_LEFT, 7.5f, 1.5f, hov ? theme::kText : theme::kMuted, "+ ADD", true);
            globalAlpha(1.0f);
            return;
        }

        const bool on = fParam[slotParam(i, kSlotOn)] >= 0.5f;
        char name[48];
        upper(d->name, name, sizeof(name));
        const float nameW = kRowW - 32.0f - (hov || sel ? 92.0f : 64.0f);
        clipLabel(kRowX + 32.0f, cy - 6.0f, ALIGN_LEFT, 8.5f, 1.0f, on ? theme::kText : theme::kMuted, name, nameW, true);

        // tags: band and channel, or the category
        const int band = int(std::lround(fParam[slotParam(i, kSlotBand)]));
        const int chan = int(std::lround(fParam[slotParam(i, kSlotChannel)]));
        float tx = kRowX + 32.0f;
        if (anyLaneB())
            tx = tag(tx, cy + 7.0f, fParam[laneParam(i)] >= 0.5f ? "LANE B" : "LANE A");
        if (band > 0 || chan > 0)
        {
            if (band > 0)
                tx = tag(tx, cy + 7.0f, band == 1 ? "LOW" : band == 2 ? "MID" : "HIGH");
            if (chan > 0)
                tag(tx, cy + 7.0f, chan == 1 ? "MID" : chan == 2 ? "SIDE" : chan == 3 ? "LEFT" : "RIGHT");
        }
        else
        {
            char cat[24];
            upper(d->category, cat, sizeof(cat));
            label(tx, cy + 7.0f, ALIGN_LEFT, 6.0f, 1.2f, theme::kSlogan, cat, true);
        }

        // reorder arrows (hover / selected)
        float rx = kRowX + kRowW - 14.0f;
        // on toggle: a power glyph
        {
            const bool lit = hovered(kHitSlotOn, int(i));
            const uint32_t c = on ? theme::kText : (lit ? theme::kMuted : theme::kTrackHover);
            beginPath();
            arc(rx, cy + 0.5f, 5.5f, -0.5f * float(M_PI) + 0.7f, 1.5f * float(M_PI) - 0.7f, CW);
            strokeColor(col(c));
            strokeWidth(1.6f);
            lineCap(ROUND);
            stroke();
            beginPath();
            moveTo(rx, cy - 6.0f);
            lineTo(rx, cy - 0.5f);
            stroke();
            lineCap(BUTT);
            addHit(rx - 10.0f, y, 20.0f, h, kHitSlotOn, int(i));
        }
        rx -= 26.0f;
        // mix: a small arc like a knob's
        {
            const float mix = clampf(fParam[slotParam(i, kSlotMix)] / 100.0f, 0.0f, 1.0f);
            const float amax = 0.75f * float(M_PI), off = -0.5f * float(M_PI);
            beginPath();
            arc(rx, cy + 0.5f, 7.0f, -amax + off, amax + off, CW);
            strokeColor(col(theme::kBorder));
            strokeWidth(2.5f);
            stroke();
            if (mix > 0.001f)
            {
                beginPath();
                arc(rx, cy + 0.5f, 7.0f, -amax + off, -amax + off + mix * 2.0f * amax, CW);
                strokeColor(col(on ? theme::kAccent : theme::kMuted));
                stroke();
            }
            std::snprintf(buf, sizeof(buf), "%d", int(std::lround(mix * 100.0f)));
            if (mix < 0.995f)
                label(rx, cy + 1.0f, ALIGN_CENTER, 5.5f, 0.0f, theme::kMuted, buf, true);
        }
        if (hov || sel)
        {
            rx -= 24.0f;
            const bool lu = hovered(kHitSlotUp, int(i)), ld = hovered(kHitSlotDown, int(i));
            chevron(rx, cy - 5.0f, -1.0f, i > 0 ? (lu ? theme::kText : theme::kMuted) : theme::kBorder);
            chevron(rx, cy + 5.0f, 1.0f, i + 1 < kSlots ? (ld ? theme::kText : theme::kMuted) : theme::kBorder);
            addHit(rx - 9.0f, y, 18.0f, h * 0.5f, kHitSlotUp, int(i));
            addHit(rx - 9.0f, y + h * 0.5f, 18.0f, h * 0.5f, kHitSlotDown, int(i));
        }
        globalAlpha(1.0f);
    }

    // small tag pill; returns the x after it
    float tag(float x, float cy, const char* text)
    {
        const float w = textWidth(text, 6.0f, 1.0f, true) + 9.0f;
        panel(x, cy - 5.5f, w, 11.0f, theme::kCard, theme::kTrackHover, 3.0f);
        label(x + w * 0.5f, cy + 0.5f, ALIGN_CENTER, 6.0f, 1.0f, theme::kText, text, true);
        return x + w + 4.0f;
    }

    void chevron(float x, float y, float dir, uint32_t rgb)
    {
        beginPath();
        moveTo(x - 3.5f, y - dir * 1.75f);
        lineTo(x, y + dir * 1.75f);
        lineTo(x + 3.5f, y - dir * 1.75f);
        strokeColor(col(rgb));
        strokeWidth(1.2f);
        stroke();
    }

    // ---- the selected module -----------------------------------------------------------------

    static constexpr float kPanX = 290.0f, kPanY = 118.0f, kPanW = 656.0f, kPanH = 318.0f;

    void drawModule()
    {
        card(kPanX, kPanY, kPanW, kPanH);
        const uint32_t s = uint32_t(fSel);
        const nulljsfx::ModuleDesc* d = slotDesc(s);
        char buf[64];

        // header: slot and category above the name; band, channel and mix on the right
        const float hy = 138.0f;
        char cat[24] = "";
        if (d)
            upper(d->category, cat, sizeof(cat));
        std::snprintf(buf, sizeof(buf), d ? "SLOT %u  \xc2\xb7  %s" : "SLOT %u", s + 1, cat);
        label(kPanX + 14.0f, 129.0f, ALIGN_LEFT, 6.5f, 1.5f, theme::kMuted, buf, true);
        if (!d)
        {
            label(kPanX + 14.0f, 145.0f, ALIGN_LEFT, 13.0f, 1.5f, theme::kMuted, "EMPTY", true);
            hline(kPanX + 10.0f, kPanX + kPanW - 10.0f, 158.0f, theme::kBorder);
            label(kPanX + kPanW * 0.5f, 262.0f, ALIGN_CENTER, 8.5f, 2.0f, theme::kMuted, "EMPTY SLOT", true);
            button(kPanX + kPanW * 0.5f - 70.0f, 280.0f, 140.0f, 28.0f, "+ ADD MODULE", kHitAddModule, int(s));
            label(kPanX + kPanW * 0.5f, 330.0f, ALIGN_CENTER, 7.5f, 0.5f, theme::kSlogan,
                  "Modules run in series, top to bottom. Drag a slot to reorder.");
            return;
        }
        char name[48];
        upper(d->name, name, sizeof(name));
        const bool nameLit = hovered(kHitAddModule, int(s));
        fitLabel(kPanX + 14.0f, 145.0f, ALIGN_LEFT, 13.0f, 1.5f, theme::kText, name, 176.0f);
        const float nw = std::min(176.0f, textWidth(name, 13.0f, 1.5f, true));
        // the name opens the chooser
        chevron(kPanX + 14.0f + nw + 9.0f, 146.0f, 1.0f, nameLit ? theme::kText : theme::kMuted);
        addHit(kPanX + 10.0f, 120.0f, nw + 24.0f, 36.0f, kHitAddModule, int(s));

        label(kPanX + 240.0f, hy, ALIGN_RIGHT, 7.0f, 1.2f, theme::kMuted, "LANE", true);
        selector(kPanX + 268.0f, hy, 48.0f, laneParam(s));
        label(kPanX + 336.0f, hy, ALIGN_RIGHT, 7.0f, 1.2f, theme::kMuted, "BAND", true);
        selector(kPanX + 382.0f, hy, 82.0f, slotParam(s, kSlotBand));
        label(kPanX + 490.0f, hy, ALIGN_RIGHT, 7.0f, 1.2f, theme::kMuted, "CHANNEL", true);
        selector(kPanX + 536.0f, hy, 82.0f, slotParam(s, kSlotChannel));
        compactKnob(kPanX + 580.0f, hy, slotParam(s, kSlotMix), "MIX", 70.0f);
        hline(kPanX + 10.0f, kPanX + kPanW - 10.0f, 158.0f, theme::kBorder);

        // controls: rows of 6, 3 rows a page
        const uint32_t n = std::min<uint32_t>(d->numSliders, kValues);
        const uint32_t perPage = 18;
        const int pages = int((n + perPage - 1) / perPage);
        if (fPage >= pages)
            fPage = 0;
        float top = 164.0f;
        const float rowH = 84.0f;
        if (pages > 1)
        {
            const float tw = (kPanW - 20.0f - float(pages - 1) * 4.0f) / float(pages);
            for (int pg = 0; pg < pages; ++pg)
            {
                const float tx = kPanX + 10.0f + float(pg) * (tw + 4.0f);
                const bool act = pg == fPage, lit = hovered(kHitTab, pg);
                panel(tx, top, tw, 19.0f, act ? kSelFill : theme::kBackground, act || lit ? theme::kTrackHover : theme::kBorder);
                const uint32_t a = uint32_t(pg) * perPage, b = std::min(n, a + perPage) - 1;
                char t0[32], t1[32], tab[80];
                upper(d->sliders[a].name, t0, sizeof(t0));
                upper(d->sliders[b].name, t1, sizeof(t1));
                std::snprintf(tab, sizeof(tab), "%s - %s", t0, t1);
                fitLabel(tx + tw * 0.5f, top + 10.0f, ALIGN_CENTER, 7.0f, 1.2f, act ? theme::kText : theme::kMuted, tab, tw - 10.0f);
                addHit(tx, top, tw, 19.0f, kHitTab, pg);
            }
            top += 24.0f;
        }
        const uint32_t first = uint32_t(fPage) * perPage, last = std::min(n, first + perPage);
        const uint32_t count = last - first;
        const uint32_t rows = (count + 5) / 6;
        // with fewer rows, centre the block vertically
        top += std::max(0.0f, (432.0f - top - float(rows) * rowH) * 0.5f) * (pages > 1 ? 0.0f : 1.0f);
        const float cw = (kPanW - 20.0f) / 6.0f;
        for (uint32_t k = first; k < last; ++k)
        {
            const uint32_t idx = k - first, row = idx / 6, inRow = std::min<uint32_t>(6, count - row * 6);
            const float cx = kPanX + 10.0f + float(6 - inRow) * cw * 0.5f + (float(idx % 6) + 0.5f) * cw;
            const float ry = top + float(row) * rowH;
            const uint32_t p = valueParam(s, k);
            const SliderInfo& in = d->sliders[k];
            char lab[48];
            upper(in.name, lab, sizeof(lab));
            if (in.options)
            {
                fitLabel(cx, ry + 10.0f, ALIGN_CENTER, 7.5f, 1.5f, theme::kMuted, lab, cw - 6.0f);
                selector(cx, ry + 42.0f, cw - 12.0f, p);
                float lo, hi;
                if (modRange(s, k, lo, hi))
                    label(cx, ry + 66.0f, ALIGN_CENTER, 6.5f, 1.0f, theme::kMuted, "MODULATED", true);
            }
            else if (n == 1)
                knobCell(cx, ry - 14.0f, p, lab, 24.0f, cw * 2.0f, 6.0f, 52.0f, 98.0f); // a One Slider module: one big knob
            else
                knobCell(cx, ry, p, lab, 14.0f, cw, 10.0f, 42.0f, 72.0f);
        }
        if (rows < 3)
            label(kPanX + kPanW * 0.5f, kPanY + kPanH - 11.0f, ALIGN_CENTER, 6.5f, 1.2f, theme::kSlogan,
              "DOUBLE-CLICK RESETS  \xc2\xb7  CTRL OR SHIFT DRAGS FINE  \xc2\xb7  MOD ROUTES MACROS, LFOS, THE ENVELOPE AND THE STEP SEQ", true);
    }

    // ---- modulation routes -------------------------------------------------------------------

    void drawRoutes()
    {
        card(kPanX, kPanY, kPanW, kPanH);
        const float hy = 137.0f;
        label(kPanX + 14.0f, hy, ALIGN_LEFT, 13.0f, 1.5f, theme::kText, "MODULATION", true);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%u / %u ROUTES", fNumRoutes, kMaxRoutes);
        label(kPanX + 160.0f, hy + 1.0f, ALIGN_LEFT, 7.0f, 1.5f, theme::kMuted, buf, true);
        button(kPanX + kPanW - 196.0f, hy - 11.0f, 88.0f, 22.0f, "+ ROUTE", kHitRouteAdd, 0);
        button(kPanX + kPanW - 100.0f, hy - 11.0f, 88.0f, 22.0f, "MODULE", kHitModuleView, 0);
        hline(kPanX + 10.0f, kPanX + kPanW - 10.0f, 158.0f, theme::kBorder);

        // column heads
        const float c0 = kPanX + 14.0f, c1 = kPanX + 154.0f, c2 = kPanX + 314.0f, c3 = kPanX + 462.0f;
        label(c0, 170.0f, ALIGN_LEFT, 6.5f, 1.5f, theme::kSlogan, "SOURCE", true);
        label(c1, 170.0f, ALIGN_LEFT, 6.5f, 1.5f, theme::kSlogan, "SLOT", true);
        label(c2, 170.0f, ALIGN_LEFT, 6.5f, 1.5f, theme::kSlogan, "TARGET", true);
        label(c3, 170.0f, ALIGN_LEFT, 6.5f, 1.5f, theme::kSlogan, "DEPTH", true);

        if (fNumRoutes == 0)
        {
            label(kPanX + kPanW * 0.5f, 270.0f, ALIGN_CENTER, 8.5f, 2.0f, theme::kMuted, "NO ROUTES", true);
            label(kPanX + kPanW * 0.5f, 290.0f, ALIGN_CENTER, 7.5f, 0.5f, theme::kSlogan,
                  "+ ROUTE sends a macro, an LFO, the envelope or the step sequencer to any value of any slot.");
            return;
        }
        const float rowH = 26.0f, y0 = 180.0f;
        const int vis = 9;
        fRouteScroll = std::max(0, std::min(fRouteScroll, int(fNumRoutes) - vis));
        RackEngine* e = engine();
        for (int k = 0; k < vis && fRouteScroll + k < int(fNumRoutes); ++k)
        {
            const int i = fRouteScroll + k;
            const Route& r = fRoutes[i];
            const float y = y0 + float(k) * rowH, cy = y + 11.0f;
            const nulljsfx::ModuleDesc* d = slotDesc(uint32_t(r.slot));

            // source pill with its live value
            char src[24];
            upper(kSourceNames[r.source], src, sizeof(src));
            routePill(c0, cy, 130.0f, src, kHitRouteSource, i);
            const float sv = e ? e->sourceValue(r.source) : 0.0f;
            const bool bi = r.source == kSrcLfo1 || r.source == kSrcLfo2;
            miniBar(c0 + 130.0f - 42.0f, cy, 24.0f, bi ? sv * 0.5f + 0.5f : sv);

            // slot
            char sl[64], mn[40];
            upper(d ? d->name : "EMPTY", mn, sizeof(mn));
            std::snprintf(sl, sizeof(sl), "%d  %s", r.slot + 1, mn);
            routePill(c1, cy, 150.0f, sl, kHitRouteSlot, i);

            // target value
            char tv[48] = "-";
            if (d && uint32_t(r.value) < d->numSliders)
                upper(d->sliders[r.value].name, tv, sizeof(tv));
            routePill(c2, cy, 138.0f, tv, kHitRouteValue, i);

            // depth: a bipolar bar
            const float bx = c3 + 6.0f, bw = 104.0f;
            const bool lit = hovered(kHitRouteDepth, i) || fDragRoute == i;
            beginPath();
            roundedRect(bx, cy - 3.0f, bw, 6.0f, 3.0f);
            fillColor(col(lit ? theme::kTrackHover : theme::kBorder));
            fill();
            const float mid = bx + bw * 0.5f, tx = mid + r.depth * bw * 0.5f;
            if (std::fabs(tx - mid) > 0.25f)
            {
                beginPath();
                rect(std::fmin(mid, tx), cy - 3.0f, std::fabs(tx - mid), 6.0f);
                fillColor(col(theme::kAccent));
                fill();
            }
            beginPath();
            moveTo(mid + 0.5f, cy - 6.0f);
            lineTo(mid + 0.5f, cy - 4.0f);
            strokeColor(col(theme::kMuted));
            strokeWidth(1.0f);
            stroke();
            beginPath();
            circle(tx, cy, 5.0f);
            fillPaint(linearGradient(tx, cy - 5.0f, tx, cy + 5.0f, col(theme::kIconHi), col(theme::kIconLo)));
            fill();
            addHit(bx - 6.0f, y, bw + 12.0f, 22.0f, kHitRouteDepth, i);
            std::snprintf(buf, sizeof(buf), r.depth > 0.0005f ? "+%.0f%%" : "%.0f%%", double(r.depth * 100.0f));
            label(bx + bw + 50.0f, cy, ALIGN_RIGHT, 8.5f, 0.0f, theme::kText, buf);

            // delete
            const float dx = kPanX + kPanW - 24.0f;
            const bool ld = hovered(kHitRouteDelete, i);
            beginPath();
            moveTo(dx - 3.5f, cy - 3.5f); lineTo(dx + 3.5f, cy + 3.5f);
            moveTo(dx + 3.5f, cy - 3.5f); lineTo(dx - 3.5f, cy + 3.5f);
            strokeColor(col(ld ? theme::kText : theme::kMuted));
            strokeWidth(1.2f);
            stroke();
            addHit(dx - 9.0f, y, 18.0f, 22.0f, kHitRouteDelete, i);
        }
        if (int(fNumRoutes) > vis)
        {
            std::snprintf(buf, sizeof(buf), "%d-%d OF %u  (SCROLL)", fRouteScroll + 1, std::min(int(fNumRoutes), fRouteScroll + vis), fNumRoutes);
            label(kPanX + kPanW - 14.0f, 426.0f, ALIGN_RIGHT, 6.5f, 1.2f, theme::kMuted, buf, true);
        }
    }

    void routePill(float x, float cy, float w, const char* text, int kind, int a)
    {
        const bool lit = hovered(kind, a);
        panel(x, cy - 10.5f, w, 21.0f, theme::kBackground, lit ? theme::kTrackHover : theme::kBorder);
        clipLabel(x + 15.0f, cy, ALIGN_LEFT, 7.5f, 0.8f, theme::kText, text, w - 50.0f, true);
        drawArrow(x + 7.0f, cy, -1.0f);
        drawArrow(x + w - 7.0f, cy, 1.0f);
        addHit(x, cy - 10.5f, w, 21.0f, kind, a);
    }

    void miniBar(float x, float cy, float w, float v)
    {
        beginPath();
        rect(x, cy - 1.5f, w, 3.0f);
        fillColor(col(theme::kBorder));
        fill();
        beginPath();
        rect(x, cy - 1.5f, w * clampf(v, 0.0f, 1.0f), 3.0f);
        fillColor(col(theme::kAccent));
        fill();
    }

    // ---- bottom strip ------------------------------------------------------------------------

    static constexpr float kBotY = 444.0f, kBotH = 142.0f;

    int routesFrom(int src) const
    {
        int n = 0;
        for (uint32_t i = 0; i < fNumRoutes; ++i)
            n += fRoutes[i].source == src;
        return n;
    }

    void drawMacros()
    {
        card(14.0f, kBotY, 424.0f, kBotH);
        label(26.0f, kBotY + 15.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, "MACROS", true);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "MOD  %u", fNumRoutes);
        button(346.0f, kBotY + 6.0f, 82.0f, 19.0f, buf, kHitModView, 0, fModView);
        for (int m = 0; m < 8; ++m)
        {
            const float cx = 22.0f + 25.5f + float(m) * 51.0f;
            std::snprintf(buf, sizeof(buf), "MACRO %d", m + 1);
            knobCell(cx, kBotY + 26.0f, kMacro1 + uint32_t(m), buf, 12.0f, 50.0f, 6.0f, 46.0f, 80.0f);
            const int n = routesFrom(kSrcMacro1 + m);
            // one dot per route from this macro
            for (int k = 0; k < std::min(n, 5); ++k)
            {
                beginPath();
                circle(cx + (float(k) - float(std::min(n, 5) - 1) * 0.5f) * 6.0f, kBotY + 124.0f, 1.8f);
                fillColor(col(theme::kAccent));
                fill();
            }
        }
    }

    void drawLfo(int k, float x)
    {
        const float y = kBotY, w = 160.0f;
        card(x, y, w, kBotH);
        const uint32_t base = k ? kLfo2Rate : kLfo1Rate;
        label(x + 12.0f, y + 15.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, k ? "LFO 2" : "LFO 1", true);
        RackEngine* e = engine();
        const int src = k ? kSrcLfo2 : kSrcLfo1;
        const int n = routesFrom(src);
        if (n)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d ROUTE%s", n, n > 1 ? "S" : "");
            label(x + w - 12.0f, y + 15.0f, ALIGN_RIGHT, 6.5f, 1.2f, theme::kMuted, buf, true);
        }

        // the waveform with the current phase
        const float wx = x + 10.0f, wy = y + 26.0f, ww = w - 20.0f, wh = 32.0f;
        panel(wx, wy, ww, wh, theme::kBackground, theme::kBorder);
        const int shape = int(std::lround(fParam[base + 2]));
        beginPath();
        const int pts = 64;
        uint32_t seed = 7;
        float held = 0.0f;
        for (int i = 0; i <= pts; ++i)
        {
            const float ph = float(i) / float(pts);
            float v;
            switch (shape)
            {
            case 0: v = std::sin(2.0f * float(M_PI) * ph); break;
            case 1: v = ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f); break;
            case 2: v = 2.0f * ph - 1.0f; break;
            case 3: v = 1.0f - 2.0f * ph; break;
            case 4: v = ph < 0.5f ? 1.0f : -1.0f; break;
            default:
                if (i % 8 == 0)
                {
                    seed = seed * 1103515245u + 12345u;
                    held = float((seed >> 16) & 0x7fff) / 16383.5f - 1.0f;
                }
                v = held;
                break;
            }
            const float px = wx + 4.0f + ph * (ww - 8.0f), py = wy + wh * 0.5f - v * (wh * 0.5f - 5.0f);
            if (i == 0)
                moveTo(px, py);
            else
                lineTo(px, py);
        }
        strokeColor(col(theme::kAccent, 0.75f));
        strokeWidth(1.5f);
        lineJoin(ROUND);
        stroke();
        if (e)
        {
            const float ph = e->lfoPhase(k), v = e->sourceValue(src);
            const float px = wx + 4.0f + ph * (ww - 8.0f), py = wy + wh * 0.5f - v * (wh * 0.5f - 5.0f);
            beginPath();
            circle(px, py, 2.8f);
            fillColor(col(theme::kText));
            fill();
        }

        const bool synced = fParam[base + 1] >= 0.5f;
        const int sync = int(std::lround(fParam[base + 1]));
        knobCell(x + 36.0f, y + 64.0f, base, "RATE", 11.0f, 60.0f, 5.0f, 36.0f, 66.0f, synced,
                 synced && sync > 0 && sync < 16 ? kSyncOptions[sync] : nullptr);
        label(x + 108.0f, y + 70.0f, ALIGN_CENTER, 7.0f, 1.2f, theme::kMuted, "SYNC", true);
        selector(x + 108.0f, y + 85.0f, 80.0f, base + 1, nullptr, 19.0f);
        label(x + 108.0f, y + 107.0f, ALIGN_CENTER, 7.0f, 1.2f, theme::kMuted, "SHAPE", true);
        selector(x + 108.0f, y + 122.0f, 80.0f, base + 2, nullptr, 19.0f);
    }

    void drawEnvelope()
    {
        const float x = 782.0f, y = kBotY, w = 164.0f;
        card(x, y, w, kBotH);
        label(x + 12.0f, y + 15.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, "ENVELOPE", true);
        RackEngine* e = engine();
        const int n = routesFrom(kSrcEnv);
        if (n)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d ROUTE%s", n, n > 1 ? "S" : "");
            label(x + w - 12.0f, y + 15.0f, ALIGN_RIGHT, 6.5f, 1.2f, theme::kMuted, buf, true);
        }
        // the follower's level (what the routes get, 0..1)
        const float v = e ? e->sourceValue(kSrcEnv) : 0.0f;
        const float bx = x + 12.0f, bw = w - 24.0f, by = y + 30.0f;
        beginPath();
        roundedRect(bx, by, bw, 6.0f, 3.0f);
        fillColor(col(theme::kBorder));
        fill();
        if (v > 0.002f)
        {
            beginPath();
            roundedRect(bx, by, bw * clampf(v, 0.0f, 1.0f), 6.0f, 3.0f);
            fillColor(col(theme::kAccent));
            fill();
        }
        label(bx, by + 15.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kSlogan, "INPUT LEVEL", true);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f%%", double(v * 100.0f));
        label(bx + bw, by + 15.0f, ALIGN_RIGHT, 6.5f, 0.5f, theme::kMuted, buf, true);

        const float cw = (w - 12.0f) / 3.0f;
        knobCell(x + 6.0f + cw * 0.5f, y + 54.0f, kEnvAttack, "ATTACK", 11.0f, cw, 6.0f, 40.0f, 72.0f);
        knobCell(x + 6.0f + cw * 1.5f, y + 54.0f, kEnvRelease, "RELEASE", 11.0f, cw, 6.0f, 40.0f, 72.0f);
        knobCell(x + 6.0f + cw * 2.5f, y + 54.0f, kEnvGain, "GAIN", 11.0f, cw, 6.0f, 40.0f, 72.0f);
    }

    // ---- second strip: step sequencer, lanes and the feedback loop ---------------------------

    static constexpr float kRow2Y = 594.0f, kRow2H = 72.0f;
    static constexpr float kBarsX = 104.0f, kBarsY = kRow2Y + 8.0f, kBarsW = 296.0f, kBarsH = 56.0f;

    void drawSeq()
    {
        const float x = 14.0f, y = kRow2Y, w = 600.0f;
        card(x, y, w, kRow2H);
        label(x + 12.0f, y + 15.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, "STEP SEQ", true);
        RackEngine* e = engine();
        char buf[32];
        const int nr = routesFrom(kSrcSeq);
        if (nr)
            std::snprintf(buf, sizeof(buf), "%d ROUTE%s", nr, nr > 1 ? "S" : "");
        else
            std::snprintf(buf, sizeof(buf), "NO ROUTES");
        label(x + 12.0f, y + 31.0f, ALIGN_LEFT, 6.5f, 1.2f, nr ? theme::kMuted : theme::kSlogan, buf, true);
        const int steps = std::max(1, std::min(int(kSeqSteps), int(std::lround(fParam[kSeqLength]))));
        const int cur = e ? e->seqStep() : 0;
        miniBar(x + 12.0f, y + 46.0f, 72.0f, e ? e->sourceValue(kSrcSeq) : 0.0f);
        std::snprintf(buf, sizeof(buf), "STEP %d / %d", std::min(cur, steps - 1) + 1, steps);
        label(x + 12.0f, y + 59.0f, ALIGN_LEFT, 6.5f, 1.0f, theme::kMuted, buf, true);

        // the bars: drawn and edited with the mouse
        const bool lit = hovered(kHitSeqBars, 0) || fSeqDraw;
        panel(kBarsX, kBarsY, kBarsW, kBarsH, theme::kBackground, lit ? theme::kTrackHover : theme::kBorder);
        const float sw = (kBarsW - 8.0f) / float(kSeqSteps), inner = kBarsH - 8.0f;
        for (uint32_t k = 0; k < kSeqSteps; ++k)
        {
            const float bx = kBarsX + 4.0f + float(k) * sw + 1.5f, bw = sw - 3.0f;
            const float v = clampf(fParam[kSeqStep1 + k], 0.0f, 1.0f);
            const bool active = int(k) < steps, now = active && int(k) == cur && e;
            // the slot's track, then the value
            beginPath();
            rect(bx, kBarsY + 4.0f, bw, inner);
            fillColor(col(theme::kCard, active ? 1.0f : 0.4f));
            fill();
            if (v > 0.002f)
            {
                beginPath();
                rect(bx, kBarsY + 4.0f + inner * (1.0f - v), bw, inner * v);
                fillColor(col(now ? theme::kText : theme::kAccent, active ? (now ? 0.95f : 0.7f) : 0.18f));
                fill();
            }
            if (k % 4 == 0 && k) // beat groups
                vline(bx - 2.0f, kBarsY + 4.0f, kBarsY + kBarsH - 4.0f);
        }
        addHit(kBarsX, kBarsY, kBarsW, kBarsH, kHitSeqBars, 0);

        // Steps, Rate (the note when synced), Glide, Sync
        const float top = y + 6.0f;
        knobCell(432.0f, top, kSeqLength, "STEPS", 9.0f, 46.0f, 6.0f, 30.0f, 54.0f);
        const int sync = int(std::lround(fParam[kSeqSync]));
        const bool synced = sync > 0 && sync < 16;
        knobCell(478.0f, top, kSeqRate, "RATE", 9.0f, 46.0f, 6.0f, 30.0f, 54.0f, synced, synced ? kSyncOptions[sync] : nullptr);
        knobCell(524.0f, top, kSeqGlide, "GLIDE", 9.0f, 46.0f, 6.0f, 30.0f, 54.0f);
        label(574.0f, y + 15.0f, ALIGN_CENTER, 7.0f, 1.2f, theme::kMuted, "SYNC", true);
        selector(574.0f, y + 38.0f, 66.0f, kSeqSync, nullptr, 19.0f);
    }

    // sets the step under x (and y) while drawing on the bars
    void drawStep(float x, float y)
    {
        const float sw = (kBarsW - 8.0f) / float(kSeqSteps), inner = kBarsH - 8.0f;
        const int k = int(std::floor((x - kBarsX - 4.0f) / sw));
        if (k < 0 || k >= int(kSeqSteps))
            return;
        const float v = clampf(1.0f - (y - kBarsY - 4.0f) / inner, 0.0f, 1.0f);
        const float q = std::round(v * 100.0f) / 100.0f;
        if (q != fParam[kSeqStep1 + uint32_t(k)])
        {
            setParamGesture(kSeqStep1 + uint32_t(k), q);
            fSeqEdited = true;
        }
    }

    void drawLanesLoop()
    {
        const float x = 622.0f, y = kRow2Y, w = 324.0f;
        card(x, y, w, kRow2H);
        char buf[32];

        // lanes: how many slots run on lane B, and the blend
        int onB = 0;
        for (uint32_t i = 0; i < kSlots; ++i)
            onB += !fIds[i].empty() && fParam[laneParam(i)] >= 0.5f;
        label(x + 12.0f, y + 15.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, "LANES", true);
        if (onB)
            std::snprintf(buf, sizeof(buf), "%d ON B", onB);
        else
            std::snprintf(buf, sizeof(buf), "ONE LANE");
        label(x + 12.0f, y + 31.0f, ALIGN_LEFT, 6.5f, 1.2f, onB ? theme::kMuted : theme::kSlogan, buf, true);
        // Lane Mix as how much of lane B is heard
        char mixText[24];
        std::snprintf(mixText, sizeof(mixText), "%d%% B", int(std::lround(fParam[kLaneMix])));
        knobCell(x + 90.0f, y + 6.0f, kLaneMix, "LANE MIX", 9.0f, 56.0f, 6.0f, 30.0f, 54.0f, onB == 0, mixText);
        vline(x + 126.0f, y + 10.0f, y + kRow2H - 10.0f);

        // the feedback loop
        const bool fbOn = fParam[kFeedback] > 0.0f;
        int from, to;
        fbRange(from, to);
        label(x + 138.0f, y + 15.0f, ALIGN_LEFT, 8.5f, 2.0f, theme::kMuted, "LOOP", true);
        if (fbOn)
            std::snprintf(buf, sizeof(buf), from == to ? "SLOT %d" : "SLOTS %d-%d", from + 1, to + 1);
        else
            std::snprintf(buf, sizeof(buf), "OFF");
        fitLabel(x + 138.0f, y + 31.0f, ALIGN_LEFT, 6.5f, 1.2f, fbOn ? theme::kMuted : theme::kSlogan, buf, 36.0f);
        knobCell(x + 206.0f, y + 6.0f, kFeedback, "FEEDBACK", 9.0f, 50.0f, 6.0f, 30.0f, 54.0f);
        std::snprintf(buf, sizeof(buf), "FROM %d", int(std::lround(fParam[kFbFrom])));
        selector(x + 274.0f, y + 22.0f, 84.0f, kFbFrom, buf, 19.0f);
        std::snprintf(buf, sizeof(buf), "TO %d", int(std::lround(fParam[kFbTo])));
        selector(x + 274.0f, y + 50.0f, 84.0f, kFbTo, buf, 19.0f);
    }

    // ---- popups ------------------------------------------------------------------------------

    void dim()
    {
        beginPath();
        rect(0, 0, kWidth, kHeight);
        fillColor(Color(0, 0, 0, 0.55f));
        fill();
    }

    void drawModulePicker()
    {
        dim();
        const float x = 40.0f, y = 62.0f, w = 880.0f, h = 516.0f;
        beginPath();
        roundedRect(x + 0.5f, y + 4.0f, w - 1.0f, h - 1.0f, 8.0f);
        fillColor(Color(0, 0, 0, 0.4f));
        fill();
        panel(x, y, w, h, theme::kCard, theme::kTrackHover, 8.0f);
        addHit(x, y, w, h, kHitPopupBack);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "SLOT %d", fPickSlot + 1);
        label(x + 18.0f, y + 20.0f, ALIGN_LEFT, 7.5f, 1.5f, theme::kMuted, buf, true);
        label(x + 74.0f, y + 20.0f, ALIGN_LEFT, 12.0f, 1.5f, theme::kText, "CHOOSE A MODULE", true);
        const bool has = fPickSlot >= 0 && !fIds[fPickSlot].empty();
        if (has)
            button(x + w - 200.0f, y + 9.0f, 90.0f, 22.0f, "REMOVE", kHitPick, -1);
        button(x + w - 100.0f, y + 9.0f, 82.0f, 22.0f, "CLOSE", kHitPickClose, 0);
        hline(x + 12.0f, x + w - 12.0f, y + 40.0f, theme::kBorder);

        RackEngine* e = engine();
        if (!e)
            return;
        static const char* const cats[] = { "One Slider", "Essentials", "Advanced" };
        // columns sized by how many modules each category has
        int counts[3] = {};
        for (uint32_t i = 0; i < e->numModules(); ++i)
            for (int c = 0; c < 3; ++c)
                counts[c] += std::strcmp(e->modules()[i]->category, cats[c]) == 0;
        const float itemH = 17.0f, top = y + 72.0f;
        const int perCol = int((y + h - 14.0f - top) / itemH);
        int cols[3], totalCols = 0;
        for (int c = 0; c < 3; ++c)
        {
            cols[c] = std::max(1, (counts[c] + perCol - 1) / perCol);
            totalCols += cols[c];
        }
        const float gap = 14.0f, colW = (w - 36.0f - gap * 2.0f) / float(totalCols);
        float cx = x + 18.0f;
        for (int c = 0; c < 3; ++c)
        {
            char cat[24];
            upper(cats[c], cat, sizeof(cat));
            label(cx + 4.0f, y + 56.0f, ALIGN_LEFT, 7.5f, 2.0f, theme::kMuted, cat, true);
            std::snprintf(buf, sizeof(buf), "%d", counts[c]);
            label(cx + colW * float(cols[c]) - 8.0f, y + 56.0f, ALIGN_RIGHT, 7.0f, 1.0f, theme::kSlogan, buf, true);
            hline(cx, cx + colW * float(cols[c]) - 4.0f, y + 64.0f, theme::kBorder);
            int k = 0;
            for (uint32_t i = 0; i < e->numModules(); ++i)
            {
                const nulljsfx::ModuleDesc* d = e->modules()[i];
                if (std::strcmp(d->category, cats[c]) != 0)
                    continue;
                const float ix = cx + colW * float(k / perCol), iy = top + float(k % perCol) * itemH;
                const bool cur = has && fIds[fPickSlot] == d->id, lit = hovered(kHitPick, int(i));
                if (cur || lit)
                    panel(ix, iy, colW - 6.0f, itemH - 1.0f, cur ? kSelFill : theme::kBackground, theme::kTrackHover, 3.0f);
                clipLabel(ix + 7.0f, iy + itemH * 0.5f, ALIGN_LEFT, 8.5f, 0.2f, cur || lit ? theme::kText : 0xc8c8c8, d->name,
                          colW - 16.0f, false);
                addHit(ix, iy, colW - 6.0f, itemH - 1.0f, kHitPick, int(i));
                ++k;
            }
            cx += colW * float(cols[c]) + gap;
        }
    }

    void drawPresetList()
    {
        dim();
        static const char* const cats[] = { "Chain", "Routing", "Macro" };
        static const char* const titles[] = { "CHAINS", "ROUTING", "MACROS" };
        const float x = 24.0f, y = 100.0f, w = 560.0f, itemH = 19.0f;
        int counts[3] = {};
        for (uint32_t i = 0; i < kNumPresets; ++i)
            for (int c = 0; c < 3; ++c)
                counts[c] += std::strcmp(kPresets[i].category, cats[c]) == 0;
        const float h = 74.0f + float(std::max(counts[0], std::max(counts[1], counts[2]))) * itemH;
        beginPath();
        roundedRect(x + 0.5f, y + 4.0f, w - 1.0f, h - 1.0f, 8.0f);
        fillColor(Color(0, 0, 0, 0.4f));
        fill();
        panel(x, y, w, h, theme::kCard, theme::kTrackHover, 8.0f);
        addHit(x, y, w, h, kHitPopupBack);
        const int cur = presetIndex();
        // Init on its own line
        presetItem(x + 12.0f, y + 10.0f, 170.0f, itemH, 0, cur == 0);
        label(x + w - 16.0f, y + 20.0f, ALIGN_RIGHT, 6.5f, 1.2f, theme::kSlogan, "ALSO IN THE HOST'S PRESET MENU", true);
        const float colW = (w - 24.0f) / 3.0f;
        for (int c = 0; c < 3; ++c)
        {
            const float cx = x + 12.0f + float(c) * colW;
            label(cx + 6.0f, y + 44.0f, ALIGN_LEFT, 7.5f, 2.0f, theme::kMuted, titles[c], true);
            hline(cx, cx + colW - 8.0f, y + 52.0f, theme::kBorder);
            int k = 0;
            for (uint32_t i = 0; i < kNumPresets; ++i)
                if (std::strcmp(kPresets[i].category, cats[c]) == 0)
                    presetItem(cx, y + 58.0f + float(k++) * itemH, colW - 8.0f, itemH, int(i), int(i) == cur);
        }
    }

    void presetItem(float x, float y, float w, float h, int i, bool cur)
    {
        const bool lit = hovered(kHitPresetPick, i);
        if (cur || lit)
            panel(x, y, w, h - 1.0f, cur ? kSelFill : theme::kBackground, theme::kTrackHover, 3.0f);
        label(x + 7.0f, y + h * 0.5f, ALIGN_LEFT, 8.5f, 0.2f, cur || lit ? theme::kText : 0xc8c8c8, kPresets[i].name);
        addHit(x, y, w, h - 1.0f, kHitPresetPick, i);
    }

    // ---- interaction -------------------------------------------------------------------------

    int hitAt(float x, float y) const
    {
        // popups take everything; else the last drawn (topmost) area wins
        for (int i = int(fHits.size()) - 1; i >= 0; --i)
            if (fHits[i].contains(x, y))
                return i;
        return -1;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        if (!ev.press)
        {
            if (ev.button != 1)
                return false;
            if (fDragParam >= 0)
            {
                editParameter(uint32_t(fDragParam), false);
                fDragParam = -1;
            }
            if (fDragRoute >= 0)
            {
                fDragRoute = -1;
                markEdited();
            }
            if (fSeqDraw)
            {
                fSeqDraw = false;
                if (fSeqEdited)
                    markEdited();
                fSeqEdited = false;
            }
            if (fSlotDrag >= 0)
            {
                if (fSlotDragging)
                    moveSlot(uint32_t(fSlotDrag), uint32_t(dropIndex()));
                fSlotDrag = -1;
                fSlotDragging = false;
            }
            repaint();
            return true;
        }
        if (ev.button != 1)
            return false;
        const int hi = hitAt(x, y);
        const Hit h = hi >= 0 ? fHits[hi] : Hit { 0, 0, 0, 0, kHitNone, 0 };
        const bool dbl = fLastClickTime != 0 && ev.time - fLastClickTime < 350 && fLastClickKind == h.kind && fLastClickA == h.a;
        fLastClickTime = ev.time;
        fLastClickKind = h.kind;
        fLastClickA = h.a;

        if (fPopup != kPopupNone)
        {
            const bool inside = h.kind != kHitNone;
            if (fPopup == kPopupModules && h.kind == kHitPick)
            {
                RackEngine* e = engine();
                if (h.a < 0)
                    setModule(uint32_t(fPickSlot), "");
                else if (e && uint32_t(h.a) < e->numModules())
                    setModule(uint32_t(fPickSlot), e->modules()[h.a]->id);
                fSel = fPickSlot;
                fPage = 0;
                fModView = false;
                fPopup = kPopupNone;
            }
            else if (fPopup == kPopupPresets && h.kind == kHitPresetPick)
            {
                applyPreset(uint32_t(h.a));
                fPopup = kPopupNone;
            }
            else if (h.kind == kHitPickClose || !inside)
                fPopup = kPopupNone;
            repaint();
            return true;
        }

        switch (h.kind)
        {
        case kHitKnob:
            if (dbl)
            {
                setParamGesture(uint32_t(h.a), defaultOf(uint32_t(h.a)));
                fLastClickTime = 0;
                break;
            }
            fDragParam = h.a;
            fDragX = x;
            fDragY = y;
            fDragPos = knobPos(uint32_t(h.a));
            editParameter(uint32_t(h.a), true);
            fEditFrame[h.a] = fFrame;
            break;
        case kHitSelect:
            stepParam(uint32_t(h.a), x < h.x + h.w * 0.5f ? -1 : 1, true);
            break;
        case kHitSlot:
            fSel = h.a;
            fModView = false;
            fPage = 0;
            fSlotDrag = h.a;
            fSlotDragY0 = y;
            fSlotDragY = y;
            fSlotDragging = false;
            if (dbl)
            {
                fPickSlot = h.a;
                fPopup = kPopupModules;
                fSlotDrag = -1;
            }
            break;
        case kHitAddModule:
            fSel = h.a;
            fPickSlot = h.a;
            fPopup = kPopupModules;
            break;
        case kHitSlotOn:
        {
            const uint32_t p = slotParam(uint32_t(h.a), kSlotOn);
            setParamGesture(p, fParam[p] >= 0.5f ? 0.0f : 1.0f);
            break;
        }
        case kHitSlotUp:
            if (h.a > 0)
                moveSlot(uint32_t(h.a), uint32_t(h.a - 1));
            break;
        case kHitSlotDown:
            if (h.a + 1 < int(kSlots))
                moveSlot(uint32_t(h.a), uint32_t(h.a + 1));
            break;
        case kHitPresetPrev:
        case kHitPresetNext:
        {
            const int cur = presetIndex();
            const int dir = h.kind == kHitPresetNext ? 1 : -1;
            const int n = int(kNumPresets);
            applyPreset(uint32_t(((cur < 0 ? (dir > 0 ? -1 : 0) : cur) + dir + n) % n));
            break;
        }
        case kHitPresetOpen:
            fPopup = kPopupPresets;
            break;
        case kHitTab:
            fPage = h.a;
            break;
        case kHitModView:
            fModView = !fModView;
            break;
        case kHitModuleView:
            fModView = false;
            break;
        case kHitRouteAdd:
            addRoute();
            break;
        case kHitRouteSource:
        case kHitRouteSlot:
        case kHitRouteValue:
            stepRoute(h.kind, h.a, x < h.x + h.w * 0.5f ? -1 : 1);
            break;
        case kHitRouteDepth:
            if (dbl)
            {
                fRoutes[h.a].depth = 0.0f;
                sendRoutes();
                fLastClickTime = 0;
                break;
            }
            fDragRoute = h.a;
            fDragX = x;
            fDragY = y;
            fDragPos = fRoutes[h.a].depth;
            break;
        case kHitRouteDelete:
            deleteRoute(h.a);
            break;
        case kHitSeqBars:
            fSeqDraw = true;
            fSeqEdited = false;
            drawStep(x, y);
            break;
        default:
            return false;
        }
        repaint();
        return true;
    }

    void stepRoute(int kind, int i, int dir)
    {
        if (i < 0 || i >= int(fNumRoutes))
            return;
        Route& r = fRoutes[i];
        if (kind == kHitRouteSource)
            r.source = (r.source + dir + kNumSources) % kNumSources;
        else if (kind == kHitRouteSlot)
        {
            const int s = nextModuleSlot(r.slot, dir);
            if (s < 0 || s == r.slot)
                return;
            r.slot = s;
            const nulljsfx::ModuleDesc* d = slotDesc(uint32_t(s));
            if (d && uint32_t(r.value) >= d->numSliders)
                r.value = 0;
        }
        else
        {
            const nulljsfx::ModuleDesc* d = slotDesc(uint32_t(r.slot));
            if (!d)
                return;
            const int n = int(std::min<uint32_t>(d->numSliders, kValues));
            r.value = (r.value + dir + n) % n;
        }
        sendRoutes();
        markEdited();
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        const int h = hitAt(x, y);
        if (h != fHover)
        {
            fHover = h;
            repaint();
        }
        const bool fine = (ev.mod & (kModifierControl | kModifierShift)) != 0;
        if (fDragParam >= 0)
        {
            const float mv = (x - fDragX) + (fDragY - y);
            fDragX = x;
            fDragY = y;
            fDragPos = clampf(fDragPos + mv / 200.0f * (fine ? 0.1f : 1.0f), 0.0f, 1.0f);
            setParam(uint32_t(fDragParam), fromKnobPos(uint32_t(fDragParam), fDragPos));
            return true;
        }
        if (fDragRoute >= 0 && fDragRoute < int(fNumRoutes))
        {
            const float mv = (x - fDragX) + (fDragY - y);
            fDragX = x;
            fDragY = y;
            fDragPos = clampf(fDragPos + mv / 62.0f * (fine ? 0.1f : 1.0f), -1.0f, 1.0f);
            const float d = std::round(fDragPos * 100.0f) / 100.0f;
            if (d != fRoutes[fDragRoute].depth)
            {
                fRoutes[fDragRoute].depth = d;
                sendRoutes();
            }
            repaint();
            return true;
        }
        if (fSeqDraw)
        {
            drawStep(clampf(x, kBarsX + 4.0f, kBarsX + kBarsW - 4.01f), y);
            return true;
        }
        if (fSlotDrag >= 0)
        {
            fSlotDragY = y;
            if (!fSlotDragging && std::fabs(y - fSlotDragY0) > 6.0f)
                fSlotDragging = true;
            repaint();
            return true;
        }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float s = uiScale();
        const float x = float(ev.pos.getX()) / s, y = float(ev.pos.getY()) / s;
        const float d = float(ev.delta.getY()) + float(ev.delta.getX());
        if (d == 0.0f || fPopup != kPopupNone)
            return false;
        const int dir = d > 0.0f ? 1 : -1;
        const int hi = hitAt(x, y);
        if (hi >= 0)
        {
            const Hit& h = fHits[hi];
            if (h.kind == kHitKnob)
            {
                stepParam(uint32_t(h.a), dir, false);
                return true;
            }
            if (h.kind == kHitSelect)
            {
                stepParam(uint32_t(h.a), dir, false);
                return true;
            }
            if (h.kind == kHitSeqBars)
            {
                // the step under the mouse, in 5 % steps
                const int k = int(std::floor((x - kBarsX - 4.0f) / ((kBarsW - 8.0f) / float(kSeqSteps))));
                if (k >= 0 && k < int(kSeqSteps))
                {
                    const uint32_t p = kSeqStep1 + uint32_t(k);
                    setParamGesture(p, clampf(std::round((fParam[p] + float(dir) * 0.05f) * 100.0f) / 100.0f, 0.0f, 1.0f));
                    markEdited();
                }
                return true;
            }
            if (h.kind == kHitRouteDepth)
            {
                Route& r = fRoutes[h.a];
                r.depth = clampf(std::round((r.depth + float(dir) * 0.01f) * 100.0f) / 100.0f, -1.0f, 1.0f);
                sendRoutes();
                repaint();
                return true;
            }
        }
        if (fModView && x >= kPanX && x < kPanX + kPanW && y >= kPanY && y < kPanY + kPanH)
        {
            fRouteScroll = std::max(0, fRouteScroll - dir);
            repaint();
            return true;
        }
        return false;
    }

private:
    enum Popup { kPopupNone, kPopupModules, kPopupPresets };

    float fParam[kNumParams] = {};
    uint32_t fEditFrame[kNumParams] = {};
    uint32_t fFrame = 1000;
    uint32_t fSeenVersion = 0;
    std::string fIds[kSlots];
    Route fRoutes[kMaxRoutes];
    uint32_t fNumRoutes = 0;
    std::string fPresetName = "Init";

    std::vector<Hit> fHits;
    int fHover = -1;
    int fSel = 0, fPage = 0;
    bool fModView = false;
    int fRouteScroll = 0;
    Popup fPopup = kPopupNone;
    int fPickSlot = 0;

    int fDragParam = -1, fDragRoute = -1;
    float fDragX = 0.0f, fDragY = 0.0f, fDragPos = 0.0f;
    int fSlotDrag = -1;
    bool fSlotDragging = false;
    bool fSeqDraw = false, fSeqEdited = false;
    float fSlotDragY0 = 0.0f, fSlotDragY = 0.0f;
    uint fLastClickTime = 0;
    int fLastClickKind = kHitNone, fLastClickA = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIRack)
};

UI* createUI()
{
    return new UIRack();
}

END_NAMESPACE_DISTRHO
