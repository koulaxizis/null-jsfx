/*
 * NULL JSFX - Drum Machine: the patterns, shared by the DSP and the interface
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The same patterns as the pat_set() calls in drum_machine.jsfx: a pattern is 10 tracks x 16
 * steps (0 off, 1 hit, 2 accent, 3 flam, 4 roll), then the 10 track lengths.
 */

#pragma once

namespace nulldrum {

static constexpr int kTracks = 10;
static constexpr int kSteps = 16;
static constexpr int kPatternSize = kTracks * kSteps + kTracks; // 170
static constexpr int kNumFactory = 10;

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
    default: patFill(b, nullptr, 0); break;
    }
}

} // namespace nulldrum
