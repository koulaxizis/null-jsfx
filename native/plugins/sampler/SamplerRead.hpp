/*
 * NULL JSFX - Sampler: reads a WAV or FLAC file as the JSFX's file_riff + file_mem do
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * The values are 32-bit floats, converted from the file's format with the formulas of
 * dr_wav / dr_flac (which the JSFX engine uses), so the native plugin plays the same numbers
 * as the JSFX. WAV: PCM 8/16/24/32-bit and IEEE float 32/64-bit, also in
 * WAVE_FORMAT_EXTENSIBLE; FLAC through dr_flac.h (public domain / MIT-0, by David Reid).
 * Not real-time safe: call it off the audio thread.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
#endif

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
# pragma GCC diagnostic ignored "-Wunused-parameter"
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#ifdef _MSC_VER
# pragma warning(push, 0)
#endif
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
#define DRFLAC_API static
#define DRFLAC_PRIVATE static
#include "dr_flac.h"
#ifdef _MSC_VER
# pragma warning(pop)
#endif
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

namespace nullsmp {

// fopen for a UTF-8 path (the state holds UTF-8; Windows needs the wide form)
inline FILE* openUtf8(const char* path)
{
#ifdef _WIN32
    const int wn = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wn <= 0)
        return nullptr;
    std::wstring w(size_t(wn), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &w[0], wn);
    return _wfopen(w.c_str(), L"rb");
#else
    return std::fopen(path, "rb");
#endif
}

struct SampleFile {
    std::vector<float> data; // interleaved
    int channels = 0;
    double sampleRate = 0.0;
};

// dr_wav's conversions to float (drwav__pcm_to_f32 and friends)
inline float wavPcmToF32(const unsigned char* p, uint32_t bytes)
{
    if (bytes == 1)
    {
        float x = float(p[0]);
        x = x * 0.00784313725490196078f;
        x = x - 1;
        return x;
    }
    if (bytes == 2)
    {
        const int16_t s = int16_t(uint16_t(p[0]) | uint16_t(p[1]) << 8);
        return float(s) * 0.000030517578125f;
    }
    if (bytes == 3)
    {
        const uint32_t a = uint32_t(p[0]) << 8, b = uint32_t(p[1]) << 16, c = uint32_t(p[2]) << 24;
        const double x = double(int32_t(a | b | c) >> 8);
        return float(x * 0.00000011920928955078125);
    }
    if (bytes == 4)
    {
        const int32_t s = int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24);
        return float(s / 2147483648.0);
    }
    uint64_t sample = 0;
    unsigned shift = (8 - bytes) * 8;
    for (uint32_t j = 0; j < bytes; ++j, shift += 8)
        sample |= uint64_t(p[j]) << shift;
    return float(int64_t(sample) / 9223372036854775807.0);
}

// a RIFF/WAVE file; at most maxValues values (a whole number of frames)
inline bool readWavF32(const char* path, SampleFile& out, size_t maxValues)
{
    FILE* f = openUtf8(path);
    if (!f)
        return false;
    auto u32 = [](const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; };
    auto u16 = [](const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; };
    unsigned char hdr[12];
    bool ok = std::fread(hdr, 1, 12, f) == 12 && !std::memcmp(hdr, "RIFF", 4) && !std::memcmp(hdr + 8, "WAVE", 4);
    uint32_t format = 0, channels = 0, rate = 0, bits = 0, align = 0;
    bool haveFmt = false, done = false;
    while (ok && !done)
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
            align = u16(fmt + 12);
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
            // the bytes per sample as dr_wav takes them: the block align over the channels
            const uint32_t bytes = channels ? (align ? align / channels : (bits + 7) / 8) : 0;
            if (!haveFmt || channels == 0 || rate == 0 || bytes == 0 || (format != 1 && format != 3) ||
                (format == 3 && bytes != 4 && bytes != 8) || (format == 1 && bytes > 8))
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
                if (format == 3 && bytes == 4)
                    std::memcpy(&out.data[i], p, 4);
                else if (format == 3)
                {
                    double v;
                    std::memcpy(&v, p, 8);
                    out.data[i] = float(v);
                }
                else
                    out.data[i] = wavPcmToF32(p, bytes);
            }
            out.channels = int(channels);
            out.sampleRate = double(rate);
            done = true;
        }
        else
            std::fseek(f, long(size + (size & 1)), SEEK_CUR);
    }
    std::fclose(f);
    return ok && done;
}

inline bool readFlacF32(const char* path, SampleFile& out, size_t maxValues)
{
#ifdef _WIN32
    // dr_flac opens wide paths on Windows; the state holds UTF-8
    const int wn = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring w(size_t(wn > 0 ? wn : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &w[0], wn);
    drflac* fl = drflac_open_file_w(w.c_str(), nullptr);
#else
    drflac* fl = drflac_open_file(path, nullptr);
#endif
    if (!fl)
        return false;
    const uint32_t ch = fl->channels;
    if (ch == 0 || fl->sampleRate == 0)
    {
        drflac_close(fl);
        return false;
    }
    size_t want = size_t(fl->totalPCMFrameCount) * ch;
    if (want > maxValues)
        want = maxValues - maxValues % ch;
    out.data.resize(want);
    const drflac_uint64 got = drflac_read_pcm_frames_f32(fl, want / ch, out.data.data());
    out.data.resize(size_t(got) * ch);
    out.channels = int(ch);
    out.sampleRate = double(fl->sampleRate);
    drflac_close(fl);
    return true;
}

// the file as the JSFX reads it: false when it is no audio file the reader knows
inline bool readSample(const char* path, SampleFile& out, size_t maxValues)
{
    out = SampleFile();
    if (!path || !path[0])
        return false;
    FILE* f = openUtf8(path);
    if (!f)
        return false;
    char magic[4] = {};
    const bool got = std::fread(magic, 1, 4, f) == 4;
    std::fclose(f);
    if (!got)
        return false;
    const bool ok = !std::memcmp(magic, "fLaC", 4) ? readFlacF32(path, out, maxValues)
                                                    : readWavF32(path, out, maxValues);
    if (!ok)
        out = SampleFile();
    return ok;
}

} // namespace nullsmp
