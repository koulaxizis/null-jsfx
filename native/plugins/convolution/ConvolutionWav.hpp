/*
 * NULL JSFX - Convolution: a small WAV reader for the IR File option
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Reads a RIFF/WAVE file (PCM 8/16/24/32-bit, IEEE float 32/64-bit, also in
 * WAVE_FORMAT_EXTENSIBLE) into interleaved samples scaled to -1..1, like the JSFX's
 * file_riff + file_mem. Not real-time safe: call it off the audio thread.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace nullconv {

struct IrFile {
    std::vector<double> data; // interleaved
    int channels = 0;
    double sampleRate = 0.0;
};

inline bool readWav(const char* path, IrFile& out, size_t maxValues = size_t(1) << 24)
{
    out = IrFile();
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return false;
    auto u32 = [](const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; };
    auto u16 = [](const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; };
    unsigned char hdr[12];
    bool ok = std::fread(hdr, 1, 12, f) == 12 && !std::memcmp(hdr, "RIFF", 4) && !std::memcmp(hdr + 8, "WAVE", 4);
    uint32_t format = 0, channels = 0, rate = 0, bits = 0;
    bool haveFmt = false;
    while (ok)
    {
        unsigned char ch[8];
        if (std::fread(ch, 1, 8, f) != 8)
        {
            ok = false;
            break;
        }
        const uint32_t size = u32(ch + 4);
        if (!std::memcmp(ch, "fmt ", 4))
        {
            unsigned char fmt[40] = {};
            const uint32_t n = size < 40 ? size : 40;
            if (size < 16 || std::fread(fmt, 1, n, f) != n)
            {
                ok = false;
                break;
            }
            format = u16(fmt);
            channels = u16(fmt + 2);
            rate = u32(fmt + 4);
            bits = u16(fmt + 14);
            if (format == 0xFFFE && n >= 26)
                format = u16(fmt + 24); // the sub-format GUID starts with the format tag
            haveFmt = true;
            if (size > n)
                std::fseek(f, long(size - n + (size & 1)), SEEK_CUR);
            else if (size & 1)
                std::fseek(f, 1, SEEK_CUR);
        }
        else if (!std::memcmp(ch, "data", 4))
        {
            const uint32_t bytes = (bits + 7) / 8;
            if (!haveFmt || channels == 0 || rate == 0 || bytes == 0 || (format != 1 && format != 3) ||
                (format == 3 && bits != 32 && bits != 64) || (format == 1 && bits > 32))
            {
                ok = false;
                break;
            }
            size_t count = size / bytes;
            count -= count % channels;
            if (count > maxValues)
                count = maxValues - maxValues % channels;
            std::vector<unsigned char> raw(count * bytes);
            count = std::fread(raw.data(), bytes, count, f);
            count -= count % channels;
            out.data.resize(count);
            const unsigned char* p = raw.data();
            for (size_t i = 0; i < count; ++i, p += bytes)
            {
                double v = 0.0;
                if (format == 3 && bits == 32)
                {
                    float x;
                    std::memcpy(&x, p, 4);
                    v = x;
                }
                else if (format == 3)
                    std::memcpy(&v, p, 8);
                else if (bits <= 8)
                    v = (double(p[0]) - 128.0) / 128.0;
                else
                {
                    uint32_t u = 0;
                    for (uint32_t b = 0; b < bytes; ++b)
                        u |= uint32_t(p[b]) << (8 * b);
                    const uint32_t shift = 32 - 8 * bytes;
                    const int32_t s = int32_t(u << shift); // sign-extend via the top bits
                    v = double(s) / 2147483648.0;
                }
                out.data[i] = v;
            }
            out.channels = int(channels);
            out.sampleRate = double(rate);
            break;
        }
        else
            std::fseek(f, long(size + (size & 1)), SEEK_CUR);
    }
    std::fclose(f);
    if (!ok || out.channels == 0)
    {
        out = IrFile();
        return false;
    }
    return true;
}

} // namespace nullconv
