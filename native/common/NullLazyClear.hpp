/*
 * NULL JSFX - an audio buffer that clears itself chunk by chunk, on first touch
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * A JSFX's @init clears its memory, and REAPER re-runs @init when the transport starts; the
 * native ports repeat that in reset(). Clearing seconds of audio at once costs milliseconds on
 * the audio thread, so clearAll() only marks every 1024-sample chunk as "still to clear": a read
 * from a marked chunk returns 0 (what the cleared memory would hold) and the first write into
 * one clears it then. Every read therefore sees exactly what it would after a full clear.
 * Memory is allocated in allocate() (setSampleRate), never in clearAll(), get() or set().
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nulljsfx {

class LazyClearBuffer {
public:
    static constexpr size_t kChunkBits = 10, kChunk = size_t(1) << kChunkBits;

    // n values, all 0 (not for the audio thread)
    void allocate(size_t n)
    {
        fData.assign(n, 0.0);
        fDirty.assign((n + kChunk - 1) / kChunk, 0);
    }

    // every value reads as 0 from now on: a few hundred bytes to mark, nothing cleared yet
    void clearAll() noexcept { std::fill(fDirty.begin(), fDirty.end(), uint8_t(1)); }

    size_t size() const noexcept { return fData.size(); }

    double get(size_t i) const noexcept { return fDirty[i >> kChunkBits] ? 0.0 : fData[i]; }

    void set(size_t i, double v) noexcept
    {
        const size_t c = i >> kChunkBits;
        if (fDirty[c])
        {
            const size_t a = c << kChunkBits;
            std::fill(fData.begin() + std::ptrdiff_t(a), fData.begin() + std::ptrdiff_t(std::min(a + kChunk, fData.size())),
                      0.0);
            fDirty[c] = 0;
        }
        fData[i] = v;
    }

private:
    std::vector<double> fData;
    std::vector<uint8_t> fDirty;
};

} // namespace nulljsfx
