/*
 * NULL JSFX - Drum Machine: the patterns, shared by the DSP and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The same patterns as the pat_set() calls in drum_machine.jsfx: a pattern is 10 tracks x 16
 * steps (0 off, 1 hit, 2 accent, 3 flam, 4 roll), then the 10 track lengths. Also the kits
 * (kit_set) and the pattern generator (gen_make), shared by the DSP and the interface.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nulldrum {

static constexpr int kTracks = 10;
static constexpr int kSteps = 16;
static constexpr int kPatternSize = kTracks * kSteps + kTracks; // 170
static constexpr int kNumFactory = 18;

struct Row {
    int track;
    const char* steps; // ". x X f r"
    int len;
};

// track t of pattern b from a string of 16 steps
inline void patSet(double* b, int t, const char* str, int len)
{
    for (int i = 0; i < kSteps; ++i)
    {
        const char c = str[i];
        b[t * kSteps + i] = c == 'x' ? 1 : c == 'X' ? 2 : c == 'f' ? 3 : c == 'r' ? 4 : 0;
    }
    b[kTracks * kSteps + t] = len;
}

// pattern b: every track off and 16 steps long, then the given rows
inline void patFill(double* b, const Row* rows, int n)
{
    for (int t = 0; t < kTracks; ++t)
        patSet(b, t, "................", 16);
    for (int i = 0; i < n; ++i)
        patSet(b, rows[i].track, rows[i].steps, rows[i].len);
}

static const Row kUserRows[] = {
    { 0, "x...x...x...x...", 16 },
    { 1, "....x.......x...", 16 },
    { 4, "x.x.x.x.x.x.x.x.", 16 },
};

// House
static const Row kFactory1[] = {
    { 0, "X...x...X...x...", 16 },
    { 2, "....x.......x...", 16 },
    { 3, "..............x.", 16 },
    { 4, "x...x...x...x...", 16 },
    { 5, "..x...x...x...x.", 16 },
};

// Techno
static const Row kFactory2[] = {
    { 0, "X...x...X...x...", 16 },
    { 3, "..x..x..........", 6 },
    { 4, "xxXxxxXxxxXxxxXx", 16 },
    { 6, "..........x.....", 16 },
    { 9, "..x...x...x...x.", 16 },
};

// Boom Bap
static const Row kFactory3[] = {
    { 0, "X......xx.x.....", 16 },
    { 1, "....X.......X...", 16 },
    { 4, "x.x.x.x.x.x.x...", 16 },
    { 5, "..............x.", 16 },
};

// Electro
static const Row kFactory4[] = {
    { 0, "X.....x...x..x..", 16 },
    { 1, "....x.......x...", 16 },
    { 2, "....x.......X...", 16 },
    { 4, "xxxxxxxxxxxxxxxx", 16 },
    { 8, "..x..x....x..x..", 16 },
};

// Breakbeat
static const Row kFactory5[] = {
    { 0, "X.x.......xx....", 16 },
    { 1, "....X..x.x..X..x", 16 },
    { 4, "x.x.x.x.x.x.x.x.", 16 },
    { 9, "X...............", 16 },
};

// Trap
static const Row kFactory6[] = {
    { 0, "X......x..x.....", 16 },
    { 1, "........X.......", 16 },
    { 2, "........x.......", 16 },
    { 4, "x.x.x.r.x.x.rrx.", 16 },
};

// Disco
static const Row kFactory7[] = {
    { 0, "X...x...X...x...", 16 },
    { 1, "....x.......x...", 16 },
    { 4, "x...x...x...x...", 16 },
    { 5, "..X...X...X...X.", 16 },
    { 6, ".............x..", 16 },
    { 7, "..............x.", 16 },
};

// Synth Pop
static const Row kFactory8[] = {
    { 0, "X.......x.x.....", 16 },
    { 1, "....X.......X...", 16 },
    { 2, "....x.......x...", 16 },
    { 4, "x.x.x.x.x.x.x.x.", 16 },
    { 9, "X...............", 16 },
};

// Dembow
static const Row kFactory9[] = {
    { 0, "X...x...X...x...", 16 },
    { 1, "...x..x....x..x.", 16 },
    { 4, "x.x.x.x.x.x.x.x.", 16 },
    { 3, "...x..f....x..x.", 16 },
};

// Bossa
static const Row kFactory10[] = {
    { 0, "X..xx..xX..xx..x", 16 },
    { 3, "x..x..x...x.x...", 16 },
    { 4, "xxXxxxXxxxXxxxXx", 16 },
    { 6, "...........x....", 16 },
};

// Rock
static const Row kFactory11[] = {
    { 0, "X.....x.X.x.....", 16 },
    { 1, "....X.......X...", 16 },
    { 4, "x.x.x.x.x.x.x.x.", 16 },
    { 9, "X...............", 16 },
};

// Funk: ghost notes on the snare
static const Row kFactory12[] = {
    { 0, "X.x...x...x..x..", 16 },
    { 1, "....X..x.x.xX...", 16 },
    { 4, "xxXxxxXxxxXxxx.x", 16 },
    { 5, "..............x.", 16 },
};

// UK Garage: two-step kick
static const Row kFactory13[] = {
    { 0, "X.........x.....", 16 },
    { 1, "....X.......X...", 16 },
    { 3, "...x......x..x..", 16 },
    { 4, "..x...x...x...x.", 16 },
};

// Jungle: a chopped break
static const Row kFactory14[] = {
    { 0, "X.x.......X.....", 16 },
    { 1, "....X..x.x..X.r.", 16 },
    { 4, "x.xxx.x.x.xxx.x.", 16 },
    { 9, "X...............", 16 },
};

// Afrobeat: the bell runs over 12 steps against the bar
static const Row kFactory15[] = {
    { 0, "X.....x...X..x..", 16 },
    { 1, "....x.......x..x", 16 },
    { 4, "x.xxx.xxx.xxx.xx", 16 },
    { 6, ".............x..", 16 },
    { 8, "x.x.xx.x.x.x....", 12 },
};

// Reggae: one drop
static const Row kFactory16[] = {
    { 0, "........X.......", 16 },
    { 3, "........X.......", 16 },
    { 4, "x.x.x.x.x.x.x...", 16 },
    { 5, "..............x.", 16 },
};

// Footwork: the rim every third step
static const Row kFactory17[] = {
    { 0, "X..x..x...X..x..", 16 },
    { 2, "....x.......X...", 16 },
    { 3, "..x.............", 3 },
    { 4, "x.x.x.x.x.x.x.x.", 16 },
    { 6, "x..x..x..x..x...", 16 },
};

// Industrial: the rim over 7 steps, the bell over 9
static const Row kFactory18[] = {
    { 0, "X..xX...X..xX.x.", 16 },
    { 1, "....X.......X...", 16 },
    { 2, "....x.......x.x.", 16 },
    { 3, "x.x.xx..........", 7 },
    { 4, "xxxxxxxxxxxxxxxx", 16 },
    { 8, "..x..x..x.......", 9 },
    { 9, "X...............", 16 },
};

// Kit k (1..8): per voice, the offsets added to Level (dB), Tune (st), Decay (%), Tone (%)
static constexpr int kNumKits = 8;
static const double kKits[kNumKits][kTracks][4] = {
    { // Deep Boom
        { 0, -2, 40, -50 }, { 0, 0, 0, -10 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, -30 },
        { 0, 0, 0, -30 }, { 0, 0, 20, -30 }, { 0, 0, 20, -30 }, { 0, 0, 0, 0 }, { 0, 0, 10, -20 },
    },
    { // Punch Box
        { 0, 0, -10, 50 }, { 0, 1, 0, 25 }, { 0, 0, -10, 10 }, { 0, 0, 0, 0 }, { 0, 0, -10, 40 },
        { 0, 0, 0, 40 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { -3, 0, 0, 0 }, { 0, 0, 0, 30 },
    },
    { // Vintage
        { -2, 3, -30, -40 }, { -2, 2, -20, -20 }, { -6, 0, -20, -20 }, { 0, 0, -10, 100 }, { -2, 0, -20, -30 },
        { -3, 0, -20, -30 }, { 0, 3, -20, -30 }, { 0, 3, -20, -30 }, { 0, 2, -10, -20 }, { -6, 0, -20, -30 },
    },
    { // Pop Studio
        { 0, 0, 0, 10 }, { 0, -1, 20, 10 }, { 0, 0, 0, 30 }, { -3, 0, 0, 0 }, { 0, 0, 0, 20 },
        { 0, 0, 0, 20 }, { 0, 0, 10, 20 }, { 0, 0, 10, 20 }, { -6, 0, 0, 0 }, { 0, 0, 20, 40 },
    },
    { // Lo-Fi Dust
        { 0, -1, -15, 10 }, { 0, -2, -15, 10 }, { -3, -1, -20, 0 }, { -3, 0, 0, 0 }, { -6, -2, -20, 20 },
        { -6, -2, -20, 20 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { -6, 0, 0, 0 }, { -9, -2, -20, 0 },
    },
    { // Industrial
        { 2, -1, -20, 50 }, { 2, -3, 10, 50 }, { 0, -2, 20, 50 }, { 0, -3, 20, 0 }, { 0, -3, 0, 70 },
        { 0, -3, 0, 70 }, { 0, -3, 30, 70 }, { 0, -3, 30, 70 }, { 0, -5, 20, 50 }, { 0, -3, 30, 50 },
    },
    { // Minimal
        { 0, 1, -30, 30 }, { -3, 3, -30, 20 }, { -3, 2, -30, 0 }, { 0, 2, -20, 50 }, { 0, 2, -30, 50 },
        { -3, 2, -30, 40 }, { 0, 0, -20, 0 }, { 0, 0, -20, 0 }, { -6, 2, -20, 0 }, { -6, 0, -30, 0 },
    },
    { // Big Room
        { 2, -2, 20, 30 }, { 1, -2, 30, 30 }, { 1, -1, 40, 40 }, { 0, 0, 0, 0 }, { 0, 0, 0, 30 },
        { 0, 0, 20, 30 }, { 2, -2, 40, 50 }, { 2, -2, 40, 50 }, { -3, 0, 0, 0 }, { 1, 0, 40, 50 },
    },
};

static constexpr int kNumStyles = 6;
// Generate: per style and voice, the weight of each step (1..9, '.' never)
static const char* const kStyles[kNumStyles][kTracks] = {
    { // Four Floor
        "9..29...9..29.3.", "....3.......3...",
        "....8.......8...", "..2...2...2..3.2",
        "5353535353535353", "..8...8...8...8.",
        "...............2", "..............2.",
        "................", "1...............",
    },
    { // Breakbeat
        "9.4.....27.4..2.", "....9..3.3..9..3",
        "................", "..2.....2.....2.",
        "7.6.7.6.7.6.7.6.", "...........4....",
        ".............3..", "..............3.",
        "................", "6...............",
    },
    { // Half Time
        "9......4..6.....", "........9.......",
        "........6.......", "...2......2...2.",
        "6.6.6.5.6.6.5.5.", "..............3.",
        "................", "................",
        "................", "3...............",
    },
    { // Electro
        "9.....6...7..5..", "....9.......9...",
        "....7.......7...", "................",
        "8686868686868686", "................",
        "..........3.....", "............3...",
        "..5..5....5..5..", "................",
    },
    { // Latin
        "9..69..69..69..6", "................",
        "................", "8..8..8...8.8...",
        "6666666666666666", "................",
        "......5.......5.", "...4.......4....",
        "5.5.5.5.5.5.5.5.", "................",
    },
    { // Random
        "9.3.3.3.6.3.3.3.", "2.2.7.2.2.2.7.2.",
        "1.1.3.1.1.1.3.1.", "2222222222222222",
        "5555555555555555", "1.2.1.2.1.2.1.2.",
        "1111111111111111", "1111111111111111",
        "1.1.1.1.1.1.1.1.", "3...............",
    },
};

// the User pattern a new instance starts with
inline void defaultUser(double* b) { patFill(b, kUserRows, int(sizeof(kUserRows) / sizeof(kUserRows[0]))); }

// factory pattern k (1..kNumFactory: the Pattern slider's value minus 1)
inline void factory(int k, double* b)
{
    switch (k)
    {
    case 1: patFill(b, kFactory1, int(sizeof(kFactory1) / sizeof(kFactory1[0]))); break;
    case 2: patFill(b, kFactory2, int(sizeof(kFactory2) / sizeof(kFactory2[0]))); break;
    case 3: patFill(b, kFactory3, int(sizeof(kFactory3) / sizeof(kFactory3[0]))); break;
    case 4: patFill(b, kFactory4, int(sizeof(kFactory4) / sizeof(kFactory4[0]))); break;
    case 5: patFill(b, kFactory5, int(sizeof(kFactory5) / sizeof(kFactory5[0]))); break;
    case 6: patFill(b, kFactory6, int(sizeof(kFactory6) / sizeof(kFactory6[0]))); break;
    case 7: patFill(b, kFactory7, int(sizeof(kFactory7) / sizeof(kFactory7[0]))); break;
    case 8: patFill(b, kFactory8, int(sizeof(kFactory8) / sizeof(kFactory8[0]))); break;
    case 9: patFill(b, kFactory9, int(sizeof(kFactory9) / sizeof(kFactory9[0]))); break;
    case 10: patFill(b, kFactory10, int(sizeof(kFactory10) / sizeof(kFactory10[0]))); break;
    case 11: patFill(b, kFactory11, int(sizeof(kFactory11) / sizeof(kFactory11[0]))); break;
    case 12: patFill(b, kFactory12, int(sizeof(kFactory12) / sizeof(kFactory12[0]))); break;
    case 13: patFill(b, kFactory13, int(sizeof(kFactory13) / sizeof(kFactory13[0]))); break;
    case 14: patFill(b, kFactory14, int(sizeof(kFactory14) / sizeof(kFactory14[0]))); break;
    case 15: patFill(b, kFactory15, int(sizeof(kFactory15) / sizeof(kFactory15[0]))); break;
    case 16: patFill(b, kFactory16, int(sizeof(kFactory16) / sizeof(kFactory16[0]))); break;
    case 17: patFill(b, kFactory17, int(sizeof(kFactory17) / sizeof(kFactory17[0]))); break;
    case 18: patFill(b, kFactory18, int(sizeof(kFactory18) / sizeof(kFactory18[0]))); break;
    default: patFill(b, nullptr, 0); break;
    }
}

// one Park-Miller step (the JSFX's pm(), the same arithmetic as the noise)
inline double pm(double x)
{
    x *= 16807;
    return x - std::floor(x / 2147483647) * 2147483647;
}

// the generated pattern (gen_make in the JSFX) from Seed (1..999), Density (0..100) and Style:
// two draws per step (hit, then accent, roll or flam), always the same count
inline void generate(double* b, double seed, double density, double style)
{
    const int st = int(std::max(0.0, std::min(double(kNumStyles - 1), std::floor(style + 0.5))));
    const double d = density / 100;
    double x = seed * 7 + st * 1000 + 12345;
    for (int k = 0; k < 4; ++k)
        x = pm(x);
    for (int t = 0; t < kTracks; ++t)
    {
        const char* str = kStyles[st][t];
        for (int i = 0; i < kSteps; ++i)
        {
            x = pm(x);
            const double r1 = x / 2147483647;
            x = pm(x);
            const double r2 = x / 2147483647;
            const char c = str[i];
            const double w = c >= '1' && c <= '9' ? double(c - '0') / 9 : 0;
            const double p = std::min(1.0, w * 2 * d);
            double v = 0;
            if (r1 < p)
                v = w > 0.99 && r2 < 0.5 ? 2 : t == 4 && r2 > 0.92 ? 4 : t == 1 && r2 > 0.94 ? 3 : 1;
            b[t * kSteps + i] = v;
        }
        b[kTracks * kSteps + t] = 16;
    }
}

} // namespace nulldrum
