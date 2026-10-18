/*
 * NULL JSFX - Rack: modules that listen to a key signal (framework-free)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A plugin with a sidechain input (Sidechain Ducker) becomes a Rack module that derives from
 * nulljsfx::Module and from KeyedModule. Before each process() the Rack calls setKey with the
 * rack's input (after the input gain, delayed by the latency of the slots before it in its lane,
 * so it lines up with what the module hears): "duck the effect by the dry signal". The pointers
 * hold the same number of frames as the next process() call and are valid only during it. Without
 * a setKey (the module_<id> test, which feeds the JSFX the track on its sidechain pins too), the
 * module uses its own input as the key.
 */

#pragma once

namespace rack {

class KeyedModule {
public:
    virtual ~KeyedModule() {}
    // the key for the next process() call only
    virtual void setKey(const float* keyL, const float* keyR) = 0;
};

} // namespace rack
