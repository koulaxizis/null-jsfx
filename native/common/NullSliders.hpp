/*
 * NULL JSFX - the description of one JSFX slider, shared by the plugins, their interfaces and
 * the Rack's modules (kept free of DPF so the module tests can use it without a plugin)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cmath>

namespace DISTRHO {

struct SliderInfo {
    const char* name;   // e.g. "Amount"
    const char* symbol; // e.g. "amount"
    float min, max, def;
    const char* unit;   // e.g. "%" or ""
    float step = 0;     // the JSFX slider step; 1 or more makes an integer parameter
    const char* const* options = nullptr; // enum labels for min, min+1, ... max (JSFX {A,B,C})
};

// a float as the decimal number the JSFX header writes (7 significant digits): 0.1f -> 0.1
inline double sliderDecimal(float x)
{
    if (x == 0.0f)
        return 0.0;
    const double d = x, scale = std::pow(10.0, 6.0 - std::floor(std::log10(std::fabs(d))));
    return std::round(d * scale) / scale;
}

// The host passes parameters as float, the JSFX keeps the slider in double. Rebuild that double:
// the exact min / max / default, else the nearest point of the slider's step grid.
inline double jsfxValue(const SliderInfo& s, float v)
{
    const double mn = sliderDecimal(s.min), mx = sliderDecimal(s.max);
    if (v == s.min) return mn;
    if (v == s.max) return mx;
    if (v == s.def) return sliderDecimal(s.def);
    if (!(s.step > 0.0f)) return v;
    const double step = sliderDecimal(s.step);
    return std::fmax(mn, std::fmin(mx, mn + std::round((double(v) - mn) / step) * step));
}

} // namespace DISTRHO
