/*
 * NULL JSFX - shared palette for the VST3/CLAP plugins
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The nulljsfx.tech dark theme (assets/css/style.css), the same values as the GFX JSFX
 * library (null_gfx.jsfx-inc). Values are 0xRRGGBB; alpha is applied at the call site.
 */

#pragma once

#include <cstdint>

namespace theme {

static constexpr uint32_t kBackground = 0x1b1a18; // --bg
static constexpr uint32_t kCard       = 0x232220; // --card-bg
static constexpr uint32_t kBorder     = 0x333333; // --border
static constexpr uint32_t kText       = 0xe0e0e0; // --text
static constexpr uint32_t kMuted      = 0x999999; // --muted
static constexpr uint32_t kAccent     = 0xc4c4c4; // silver (the site purple belongs to Noxpress)
static constexpr uint32_t kIconLo     = 0x888888; // icon gradient, bottom
static constexpr uint32_t kIconHi     = 0xaaaaaa; // icon gradient, top
static constexpr uint32_t kLogoTile   = 0x2d2d2d; // logo tile
static constexpr uint32_t kLogoEdge   = 0x4a4a4a; // logo tile outline
static constexpr uint32_t kTrackHover = 0x45433f; // knob track under the mouse, 0 dB line
static constexpr uint32_t kCapTop     = 0x34322f; // knob cap highlight

} // namespace theme
