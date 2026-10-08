/*
 * NULL Tilt Native - provisional "carbon" palette
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * All UI colours live here so the palette can be swapped in one place.
 * Values are 0xRRGGBB; alpha is applied at the call site where needed.
 */

#pragma once

#include <cstdint>

namespace theme {

static constexpr uint32_t kBackground = 0x232323; // window background
static constexpr uint32_t kPanel      = 0x2b2b2b; // knob well / graph panel
static constexpr uint32_t kPanelEdge  = 0x363636; // 1px panel outline
static constexpr uint32_t kAccentLo   = 0x888888; // knob / curve gradient start
static constexpr uint32_t kAccentHi   = 0xaaaaaa; // knob / curve gradient end
static constexpr uint32_t kText       = 0xd0d0d0; // primary text
static constexpr uint32_t kTextMuted  = 0x808080; // labels, grid captions
static constexpr uint32_t kGrid       = 0x3a3a3a; // graph grid lines
static constexpr uint32_t kKnobBody   = 0x1c1c1c; // knob cap (darker than panel)
static constexpr uint32_t kTrack      = 0x3c3c3c; // unfilled knob arc

} // namespace theme
