/*
 * NULL JSFX - the host's musical position, exact in every format
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * VST3 and CLAP hosts report the beat position every block. LV2 hosts send a time:Position only
 * when the transport starts, jumps or changes tempo, and DPF advances bar/beat between them in
 * float, which drifts by whole samples within a second (a Drum Machine step lands late). The
 * frame it advances stays exact, so for LV2 this clock keeps the beat the host last sent and
 * counts on from the frame at the host's tempo; the bar/beat/tick it hands on give the beat
 * position the plugins already compute, as the JSFX gets it from REAPER.
 */

#pragma once

#include "DistrhoPlugin.hpp"
#include "DistrhoPluginUtils.hpp"

#include <cmath>
#include <cstring>

START_NAMESPACE_DISTRHO

class NullBeatClock
{
public:
    NullBeatClock() : fLv2(std::strcmp(getPluginFormatName(), "LV2") == 0) {}

    // the position for a block of `frames` frames starting now; call once per block
    const TimePosition& update(const TimePosition& host, double sampleRate, uint32_t frames)
    {
        fTime = host;
        const TimePosition::BarBeatTick& b = host.bbt;
        if (!fLv2 || !host.playing || !b.valid || b.beatsPerMinute <= 0.0 || b.beatsPerBar <= 0.0f || b.ticksPerBeat <= 0.0)
        {
            fRunning = false;
            return fTime;
        }
        const double perBar = b.beatsPerBar;
        if (!fRunning || int64_t(host.frame) != fNextFrame || b.beatsPerMinute != fBpm)
        {
            // the host has just sent a position (or this is the first block): start from it
            fBeat0 = (b.bar - 1) * perBar + (b.beat - 1) + b.tick / b.ticksPerBeat;
            fFrame0 = int64_t(host.frame);
            fBpm = b.beatsPerMinute;
            fRunning = true;
        }
        const double beat = fBeat0 + double(int64_t(host.frame) - fFrame0) * fBpm / (60.0 * sampleRate);
        const double bars = std::floor(beat / perBar);
        const double inBar = beat - bars * perBar;
        const double whole = std::floor(inBar);
        fTime.bbt.bar = int32_t(bars) + 1;
        fTime.bbt.beat = int32_t(whole) + 1;
        fTime.bbt.tick = (inBar - whole) * b.ticksPerBeat;
        fTime.bbt.barStartTick = b.ticksPerBeat * perBar * bars;
        fNextFrame = int64_t(host.frame) + frames;
        return fTime;
    }

    const TimePosition& time() const { return fTime; }

private:
    const bool fLv2;
    bool fRunning = false;
    double fBeat0 = 0.0, fBpm = 0.0;
    int64_t fFrame0 = 0, fNextFrame = 0;
    TimePosition fTime;
};

END_NAMESPACE_DISTRHO
