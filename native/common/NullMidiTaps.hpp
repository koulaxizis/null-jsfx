/*
 * NULL JSFX - the plugin side of the MIDI effects (Chords, Scale): what the note tracking sends
 * goes out through DPF, and the notes sounding and the keys held go to the display
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "NullMidiNotes.hpp"
#include "NullTaps.hpp"

START_NAMESPACE_DISTRHO

namespace nullmidi {

// the messages processor.end() sends, plus the SysEx of the block (which the JSFX never sees:
// it passes through at its frame), in time order
template <class P, class Proc>
static void sendMidi(P& plugin, Proc& proc, uint32_t frames, const MidiEvent* midi, uint32_t midiCount)
{
    uint32_t sx = 0;
    auto sysexUpTo = [&](uint32_t frame) {
        for (; sx < midiCount; ++sx)
        {
            if (midi[sx].size <= 3)
                continue;
            if (midi[sx].frame > frame)
                break;
            plugin.writeMidiEvent(midi[sx]);
        }
    };
    proc.end(frames, [&](uint32_t ofs, int a, int b, int c) {
        sysexUpTo(ofs);
        MidiEvent ev {};
        ev.frame = ofs;
        ev.size = ::nulljsfx::MidiNotes::size(a);
        ev.data[0] = uint8_t(a);
        ev.data[1] = uint8_t(b);
        ev.data[2] = uint8_t(c);
        ev.dataExt = nullptr;
        plugin.writeMidiEvent(ev);
    });
    sysexUpTo(frames);
}

// aux[0..7]: the output notes sounding on any channel, aux[8..15]: the keys held on any channel,
// 16 notes per value as bits (exact in a float)
static void midiTaps(::nulljsfx::Taps& taps, const ::nulljsfx::MidiNotes& notes)
{
    for (int g = 0; g < 8; ++g)
    {
        uint32_t on = 0, held = 0;
        for (int i = 0; i < 16; ++i)
        {
            const int n = g * 16 + i;
            for (int ch = 0; ch < 16; ++ch)
            {
                if (notes.sounding(ch * 128 + n))
                    on |= 1u << i;
                if (notes.held(ch * 128 + n))
                    held |= 1u << i;
            }
        }
        taps.aux[g].store(float(on), std::memory_order_relaxed);
        taps.aux[8 + g].store(float(held), std::memory_order_relaxed);
    }
}

} // namespace nullmidi

END_NAMESPACE_DISTRHO
