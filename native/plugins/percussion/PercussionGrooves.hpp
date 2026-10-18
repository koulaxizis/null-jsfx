/*
 * NULL JSFX - Percussion: the built-in grooves (the g_def / g_set calls of percussion.jsfx)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace nullperc {

static constexpr int kTracks = 11;
static constexpr int kMaxSteps = 32;
static constexpr int kNumGrooves = 8;

// the groove table (generated from the same data as the JSFX block): per groove the length in steps, the steps per beat and
// 11 tracks of step codes (0 off, 1 hit, 2 accent, 3 ghost, 4 mute, 5 slap, 6 long)
struct Groove { const char* name; int len, spb; const char* tracks[11]; };
static const Groove kGrooves[] = {
    { "Salsa", 32, 4, { "m.g.s.g.m.g.x...", "..............x.", "x.x.x...x.x.x...", "......x.......x.", "x.g.x.g.x.g.x.g.", nullptr, "x...x.x.X.x.x...", "x.....x.....x.......x...x.......", "l...x.x.l...x.x.", nullptr, nullptr } },
    { "Bossa", 32, 4, { "....g.......g...", "x......gx......g", nullptr, nullptr, "XgxgXgxgXgxgXgxg", nullptr, nullptr, "x..x..x...x..x....x..x...x......", nullptr, "..x...x...x...x.", nullptr } },
    { "Samba", 16, 4, { "..s...x...s.x.x.", "m...X...m...X...", nullptr, nullptr, "xxxXxxxXxxxXxxxX", "X.xxX.xxX.xxX.xx", "x.x.x..x.x.x.x..", nullptr, nullptr, nullptr, nullptr } },
    { "Cumbia", 16, 4, { "m...s.g.m...s.g.", "..x...x...x...x.", nullptr, nullptr, "x.X.x.X.x.X.x.X.", nullptr, nullptr, nullptr, "l.xxl.xxl.xxl.xx", nullptr, nullptr } },
    { "Afro Bell", 12, 3, { "m.gs.gm.gx.x", "X.....x.....", nullptr, "...x.....x..", "XgxXgxXgxXgx", nullptr, "x.x.xx.x.x.x", nullptr, nullptr, nullptr, nullptr } },
    { "Afrobeat", 16, 4, { "..s...x.m.s...x.", "X.....x.....x...", nullptr, nullptr, "xgxgXgxgxgxgXgxg", "....x.......x...", "x.x.xx.xx.x.xx.x", "x..x..x...x.x...", nullptr, nullptr, nullptr } },
    { "Reggaeton", 16, 4, { "...s..s....s..s.", "X...x...X...x...", nullptr, "......x.......x.", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, "..x...x...x...x.", nullptr } },
    { "Pop", 16, 4, { "......x.m.....x.", "x.........x.....", nullptr, nullptr, "xgXgxgXgxgXgxgXg", "....X.......X...", nullptr, nullptr, nullptr, nullptr, "x.......m......." } },
};

// step codes from a track string: ". x X g m s l" (off, hit, accent, ghost, mute, slap, long)
inline int stepCode(char c)
{
    return c == 'x' ? 1 : c == 'X' ? 2 : c == 'g' ? 3 : c == 'm' ? 4 : c == 's' ? 5 : c == 'l' ? 6 : 0;
}

// groove k (1..8) as the JSFX holds it: track t, step i at [t * 32 + i]
inline void grooveSteps(int k, double* out)
{
    const Groove& g = kGrooves[k - 1];
    for (int i = 0; i < kTracks * kMaxSteps; ++i)
        out[i] = 0;
    for (int t = 0; t < kTracks; ++t)
    {
        const char* s = g.tracks[t];
        if (!s)
            continue;
        int n = 0;
        while (s[n])
            ++n;
        for (int i = 0; i < g.len; ++i)
            out[t * kMaxSteps + i] = stepCode(s[i % n]);
    }
}

} // namespace nullperc
