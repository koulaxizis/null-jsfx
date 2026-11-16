/*
 * NULL JSFX - MIDI Monitor, framework-independent core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/midi_monitor.jsfx: the MIDI passes unchanged (the plugin sends
 * every event on as it came in); this keeps what the display shows, as the JSFX's @block: the
 * latest 64 messages with their time since the start (unless Freeze is on), every channel's held
 * notes (velocity), pitch bend and pressure. The audio thread writes and the interface reads
 * (direct access); a torn read only draws one odd frame.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace nullmidimonitor {

struct Params {
    int channel = 0, show = 0, freeze = 0, middleC = 0;
};

class Monitor {
public:
    static constexpr uint32_t kLog = 64;

    struct Entry { double t; int a, b, c; };

    void reset() noexcept
    {
        std::memset(fLog, 0, sizeof(fLog));
        std::memset(fHeld, 0, sizeof(fHeld));
        std::memset(fBend, 0, sizeof(fBend));
        std::memset(fPrs, 0, sizeof(fPrs));
        fLogged.store(0, std::memory_order_relaxed);
        fSeen.store(0, std::memory_order_relaxed);
        fNow = 0.0;
    }

    // one short message of the block, at frame ofs
    void input(uint32_t ofs, int a, int b, int c, bool freeze, double sr) noexcept
    {
        fSeen.fetch_add(1, std::memory_order_relaxed);
        const int st = a & 0xF0, ch = a & 15;
        if (st == 0x90 && c > 0)
            fHeld[ch * 128 + b] = uint8_t(c);
        else if (st == 0x80 || st == 0x90)
            fHeld[ch * 128 + b] = 0;
        else if (st == 0xE0)
            fBend[ch] = float((c * 128 + b - 8192) / 8192.0);
        else if (st == 0xD0)
            fPrs[ch] = float(b / 127.0);
        else if (st == 0xB0 && (b == 120 || b == 123))
            std::memset(fHeld + ch * 128, 0, 128);
        if (!freeze)
        {
            const uint32_t n = fLogged.load(std::memory_order_relaxed);
            fLog[n % kLog] = { (fNow + ofs) / sr, a, b, c };
            fLogged.store(n + 1, std::memory_order_release);
        }
    }

    void end(uint32_t frames) noexcept { fNow += frames; }

    // for the interface
    uint32_t logged() const noexcept { return fLogged.load(std::memory_order_acquire); }
    uint32_t seen() const noexcept { return fSeen.load(std::memory_order_relaxed); }
    const Entry& entry(uint32_t i) const noexcept { return fLog[i % kLog]; } // i < logged()
    int held(int ch, int note) const noexcept { return fHeld[ch * 128 + note]; }
    float bend(int ch) const noexcept { return fBend[ch]; }
    float pressure(int ch) const noexcept { return fPrs[ch]; }

private:
    Entry fLog[kLog] = {};
    uint8_t fHeld[2048] = {};
    float fBend[16] = {}, fPrs[16] = {};
    std::atomic<uint32_t> fLogged { 0 }, fSeen { 0 };
    double fNow = 0.0;
};

// note n's name with Middle C = C4 (0) or C3 (1)
inline void noteName(int n, int middleC, char* buf, size_t len)
{
    static const char* const kNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    std::snprintf(buf, len, "%s%d", kNames[n % 12], n / 12 - (middleC ? 2 : 1));
}

// what message a b c is, for the list: its kind and its values (as midi_monitor_gfx.jsfx)
inline void describe(int a, int b, int c, int middleC, char* kind, size_t nk, char* data, size_t nd)
{
    char nn[16];
    const int st = a & 0xF0;
    noteName(b, middleC, nn, sizeof(nn));
    if (st == 0x90 && c > 0)
    {
        std::snprintf(kind, nk, "NOTE ON");
        std::snprintf(data, nd, "%s  %d  VEL %d", nn, b, c);
    }
    else if (st == 0x80 || st == 0x90)
    {
        std::snprintf(kind, nk, "NOTE OFF");
        std::snprintf(data, nd, "%s  %d", nn, b);
    }
    else if (st == 0xA0)
    {
        std::snprintf(kind, nk, "KEY PRESSURE");
        std::snprintf(data, nd, "%s  %d  %d", nn, b, c);
    }
    else if (st == 0xB0)
    {
        const char* name = b == 1 ? "MOD WHEEL" : b == 7 ? "VOLUME" : b == 10 ? "PAN" : b == 11 ? "EXPRESSION"
                         : b == 64 ? "SUSTAIN" : b == 120 ? "ALL SOUND OFF" : b == 121 ? "RESET ALL"
                         : b == 123 ? "ALL NOTES OFF" : "";
        std::snprintf(kind, nk, "CC %d", b);
        std::snprintf(data, nd, "%d%s%s", c, *name ? "  " : "", name);
    }
    else if (st == 0xC0)
    {
        std::snprintf(kind, nk, "PROGRAM");
        std::snprintf(data, nd, "%d", b);
    }
    else if (st == 0xD0)
    {
        std::snprintf(kind, nk, "PRESSURE");
        std::snprintf(data, nd, "%d", b);
    }
    else if (st == 0xE0)
    {
        std::snprintf(kind, nk, "PITCH BEND");
        std::snprintf(data, nd, "%+.3f  %d", (c * 128 + b - 8192) / 8192.0, c * 128 + b);
    }
    else
    {
        std::snprintf(kind, nk, "%02X", a);
        std::snprintf(data, nd, "%d %d", b, c);
    }
}

} // namespace nullmidimonitor
