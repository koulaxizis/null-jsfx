/*
 * NULL JSFX - Rack (VST3/CLAP): the plugin class, shared by Plugin.cpp and UI.cpp (the interface
 * reads the engine directly for module names, modulation and meters; DISTRHO_PLUGIN_WANT_DIRECT_ACCESS)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "DistrhoPlugin.hpp"
#include "RackEngine.hpp"

#include <atomic>
#include <memory>

START_NAMESPACE_DISTRHO

class RackPluginBase : public Plugin
{
public:
    RackPluginBase(const nulljsfx::ModuleDesc* const* modules, uint32_t numModules)
        : Plugin(rack::kNumParams, rack::kNumPresets, 3),
          fEngine(new rack::RackEngine(modules, numModules)) {}

    rack::RackEngine& engine() { return *fEngine; }

    // bumped whenever the slots, routes or preset name change (state, program or UI)
    uint32_t stateVersion() const { return fStateVersion.load(std::memory_order_acquire); }

protected:
    void bumpState() { fStateVersion.fetch_add(1, std::memory_order_release); }

    std::unique_ptr<rack::RackEngine> fEngine;
    std::atomic<uint32_t> fStateVersion { 1 };
};

END_NAMESPACE_DISTRHO
