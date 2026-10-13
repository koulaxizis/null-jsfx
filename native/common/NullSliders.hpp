/*
 * NULL JSFX - the description of one JSFX slider, shared by the plugins, their interfaces and
 * the Rack's modules (kept free of DPF so the module tests can use it without a plugin)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace DISTRHO {

struct SliderInfo {
    const char* name;   // e.g. "Amount"
    const char* symbol; // e.g. "amount"
    float min, max, def;
    const char* unit;   // e.g. "%" or ""
    float step = 0;     // the JSFX slider step; 1 or more makes an integer parameter
    const char* const* options = nullptr; // enum labels for min, min+1, ... max (JSFX {A,B,C})
};

} // namespace DISTRHO
