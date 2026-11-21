/*
 * NULL JSFX - Noise Reduction: the noise profile from a file and in the saved state (4.2)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Off the audio thread only. Mirrors noise_reduction.jsfx:
 *  - WavStream reads a RIFF/WAVE file in chunks (PCM 8/16/24/32-bit, IEEE float 32/64-bit, also
 *    in WAVE_FORMAT_EXTENSIBLE), scaled to -1..1 like the JSFX's file_riff + file_mem.
 *  - analyseFile() is nf_analyse(): the first 60 s of the file, resampled to the plugin's rate
 *    (linear, as Convolution's IR File), averaged per bin exactly as Learn averages it while the
 *    file plays from its first sample (2048-point Hann frames every 512 samples once 2048 have
 *    arrived, the magnitudes of both channels averaged; a mono file feeds both).
 *  - mapProfile() is nr_map(): a profile at another sample rate, by frequency.
 *  - the "noiseprofile" state string: base64 of version 1, source (1 file, 2 Learn), Use Noise
 *    File (0/1), cut at 60 s (0/1), sample rate (float32), length in seconds (float32) and the
 *    1025 average magnitudes (float32), all little-endian: the values the JSFX's @serialize
 *    writes, in the same single precision.
 *  - the files the interface steps through: Data/null_jsfx_noise in REAPER's resource folder,
 *    and null_jsfx_noise in the home folder for other hosts (as Convolution's IR files).
 */

#pragma once

#include "NoiseReductionDSP.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
#else
# include <dirent.h>
#endif

namespace nullnr {

// ---- WAV in chunks ---------------------------------------------------------------------------

class WavStream {
public:
    WavStream() = default;
    WavStream(const WavStream&) = delete;
    WavStream& operator=(const WavStream&) = delete;
    ~WavStream() { close(); }

    bool open(const char* path)
    {
        close();
        fFile = path && path[0] ? std::fopen(path, "rb") : nullptr;
        if (!fFile)
            return false;
        unsigned char hdr[12];
        if (std::fread(hdr, 1, 12, fFile) != 12 || std::memcmp(hdr, "RIFF", 4) || std::memcmp(hdr + 8, "WAVE", 4))
            return fail();
        uint32_t format = 0;
        bool haveFmt = false;
        for (;;)
        {
            unsigned char ch[8];
            if (std::fread(ch, 1, 8, fFile) != 8)
                return fail();
            const uint32_t size = u32(ch + 4);
            if (!std::memcmp(ch, "fmt ", 4))
            {
                unsigned char fmt[40] = {};
                const uint32_t n = size < 40 ? size : 40;
                if (size < 16 || std::fread(fmt, 1, n, fFile) != n)
                    return fail();
                format = u16(fmt);
                fChannels = int(u16(fmt + 2));
                fRate = double(u32(fmt + 4));
                fBits = u16(fmt + 14);
                if (format == 0xFFFE && n >= 26)
                    format = u16(fmt + 24); // the sub-format GUID starts with the format tag
                haveFmt = true;
                if (!skip(uint64_t(size - n) + (size & 1)))
                    return fail();
            }
            else if (!std::memcmp(ch, "data", 4))
            {
                fBytes = (fBits + 7) / 8;
                if (!haveFmt || fChannels <= 0 || fRate <= 0.0 || fBytes == 0 || (format != 1 && format != 3) ||
                    (format == 3 && fBits != 32 && fBits != 64) || (format == 1 && fBits > 32))
                    return fail();
                fFloat = format == 3;
                fLeft = uint64_t(size / fBytes);
                fLeft -= fLeft % uint64_t(fChannels);
                return true;
            }
            else if (!skip(uint64_t(size) + (size & 1)))
                return fail();
        }
    }

    int channels() const { return fChannels; }
    double sampleRate() const { return fRate; }
    // values (frames x channels) not yet read, as the JSFX's file_avail()
    uint64_t avail() const { return fLeft; }

    // reads up to n values into dst; returns how many, as the JSFX's file_mem()
    uint64_t read(double* dst, uint64_t n)
    {
        if (!fFile)
            return 0;
        n = std::min(n, fLeft);
        uint64_t done = 0;
        unsigned char raw[4096 * 8];
        while (done < n)
        {
            const uint64_t want = std::min<uint64_t>(n - done, sizeof(raw) / fBytes);
            const uint64_t got = std::fread(raw, fBytes, size_t(want), fFile);
            const unsigned char* p = raw;
            for (uint64_t i = 0; i < got; ++i, p += fBytes)
                dst[done + i] = decode(p);
            done += got;
            if (got < want)
            {
                fLeft = 0;
                break;
            }
        }
        if (fLeft >= done)
            fLeft -= done;
        return done;
    }

    void close()
    {
        if (fFile)
            std::fclose(fFile);
        fFile = nullptr;
        fChannels = 0;
        fRate = 0.0;
        fLeft = 0;
    }

private:
    static uint32_t u32(const unsigned char* p)
    {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
    static uint32_t u16(const unsigned char* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }

    bool fail()
    {
        close();
        return false;
    }

    // forward in steps that fit a 32-bit long (Windows)
    bool skip(uint64_t n)
    {
        while (n > 0)
        {
            const uint64_t step = n < 0x40000000u ? n : 0x40000000u;
            if (std::fseek(fFile, long(step), SEEK_CUR) != 0)
                return false;
            n -= step;
        }
        return true;
    }

    double decode(const unsigned char* p) const
    {
        if (fFloat && fBits == 32)
        {
            float x;
            std::memcpy(&x, p, 4);
            return x;
        }
        if (fFloat)
        {
            double x;
            std::memcpy(&x, p, 8);
            return x;
        }
        if (fBits <= 8)
            return (double(p[0]) - 128.0) / 128.0;
        uint32_t u = 0;
        for (uint32_t b = 0; b < fBytes; ++b)
            u |= uint32_t(p[b]) << (8 * b);
        const int32_t s = int32_t(u << (32 - 8 * fBytes)); // sign-extend via the top bits
        return double(s) / 2147483648.0;
    }

    std::FILE* fFile = nullptr;
    int fChannels = 0;
    double fRate = 0.0;
    uint32_t fBits = 0, fBytes = 0;
    bool fFloat = false;
    uint64_t fLeft = 0;
};

// ---- the profile of a file -----------------------------------------------------------------

// nf_analyse() of the JSFX: the summed magnitudes and the number of frames, at sample rate sr;
// ok is false when the file gave no frame (unreadable or shorter than 2048 samples)
inline void analyseFile(const char* path, double sr, FileProfile& out)
{
    out.ok = false;
    out.cut = false;
    out.frames = 0.0;
    out.sampleRate = sr;
    std::fill(out.sum, out.sum + kNB, 0.0);
    WavStream w;
    if (!w.open(path))
        return;
    const double nch = double(w.channels()), fsr = w.sampleRate();
    static constexpr double kNFV = 65536.0; // values read per chunk
    double total = std::floor(double(w.avail()) / nch);
    if (total > std::floor(60.0 * fsr))
    {
        total = std::floor(60.0 * fsr);
        out.cut = true;
    }
    const double ratio = fsr / sr;
    const double nout = std::floor(total / ratio);
    const double cpf = std::floor(kNFV / nch);
    std::vector<double> buf(static_cast<size_t>(kNFV)), ringL(kN), ringR(kN), win(kN), fl(2 * kN), fr(2 * kN);
    for (int i = 0; i < kN; ++i)
        win[size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kN);
    const int ich = w.channels();
    double b0 = 0.0, bn = 0.0, nfr = 0.0;
    int ip = 0, fill = 0, fc = 0;
    for (double i = 0.0; i < nout; i += 1.0)
    {
        double p = i * ratio;
        const double f = p - std::floor(p);
        p = std::floor(p);
        double q = std::min(p + 1.0, total - 1.0);
        // the chunk holds the frames b0 .. b0 + bn - 1: read on when q is past it
        if (q >= b0 + bn)
        {
            if (p > b0 + bn)
            {
                w.read(buf.data(), uint64_t((p - b0 - bn) * nch));
                bn = p - b0;
            }
            const double keep = b0 + bn - p;
            std::memmove(buf.data(), buf.data() + size_t((p - b0) * nch), size_t(keep * nch) * sizeof(double));
            b0 = p;
            const double got = double(w.read(buf.data() + size_t(keep * nch),
                                              uint64_t(std::min(cpf - keep, total - b0 - keep) * nch)));
            bn = keep + std::floor(got / nch);
            q = std::min(q, b0 + bn - 1.0);
        }
        const size_t a = size_t((p - b0) * nch), c = size_t((q - b0) * nch);
        const double xl = buf[a] * (1.0 - f) + buf[c] * f;
        const double xr = ich > 1 ? buf[a + 1] * (1.0 - f) + buf[c + 1] * f : xl;
        ringL[size_t(ip)] = xl;
        ringR[size_t(ip)] = xr;
        if (++ip >= kN)
            ip = 0;
        if (fill < kN)
            ++fill;
        if (++fc >= kH)
        {
            fc = 0;
            if (fill >= kN)
            {
                // nf_frame(ip)
                for (int j = 0; j < kN; ++j)
                {
                    const size_t pp = size_t((ip + j) % kN);
                    const double ww = win[size_t(j)];
                    fl[size_t(2 * j)] = ringL[pp] * ww;
                    fl[size_t(2 * j + 1)] = 0.0;
                    fr[size_t(2 * j)] = ringR[pp] * ww;
                    fr[size_t(2 * j + 1)] = 0.0;
                }
                eelfft::fft(fl.data(), kN);
                eelfft::fftPermute(fl.data(), kN);
                eelfft::fft(fr.data(), kN);
                eelfft::fftPermute(fr.data(), kN);
                for (int k = 0; k < kNB; ++k)
                {
                    const double rl = fl[size_t(2 * k)], il = fl[size_t(2 * k + 1)];
                    const double rr = fr[size_t(2 * k)], ir = fr[size_t(2 * k + 1)];
                    out.sum[k] += (std::sqrt(rl * rl + il * il) + std::sqrt(rr * rr + ir * ir)) * 0.5;
                }
                nfr += 1.0;
            }
        }
    }
    out.frames = nfr;
    out.ok = nfr > 0.0;
}

// ---- the saved profile ("noiseprofile") ------------------------------------------------------

static constexpr size_t kBlobBytes = 4 + 4 + 4 + 4 * size_t(kNB);

inline std::string encodeBase64(const unsigned char* d, size_t n)
{
    static const char* const t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string s;
    s.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3)
    {
        const uint32_t v = uint32_t(d[i]) << 16 | (i + 1 < n ? uint32_t(d[i + 1]) << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        s += t[v >> 18 & 63];
        s += t[v >> 12 & 63];
        s += i + 1 < n ? t[v >> 6 & 63] : '=';
        s += i + 2 < n ? t[v & 63] : '=';
    }
    return s;
}

inline bool decodeBase64(const char* s, std::vector<unsigned char>& out)
{
    out.clear();
    uint32_t acc = 0;
    int bits = 0;
    for (; s && *s; ++s)
    {
        const char c = *s;
        int v;
        if (c >= 'A' && c <= 'Z')
            v = c - 'A';
        else if (c >= 'a' && c <= 'z')
            v = c - 'a' + 26;
        else if (c >= '0' && c <= '9')
            v = c - '0' + 52;
        else if (c == '+')
            v = 62;
        else if (c == '/')
            v = 63;
        else if (c == '=' || c == '\n' || c == '\r' || c == ' ')
            continue;
        else
            return false;
        acc = acc << 6 | uint32_t(v);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<unsigned char>(acc >> bits & 0xFF));
        }
    }
    return true;
}

inline void putF32(unsigned char* p, float f)
{
    uint32_t u;
    std::memcpy(&u, &f, 4);
    for (int b = 0; b < 4; ++b)
        p[b] = static_cast<unsigned char>(u >> (8 * b) & 0xFF);
}

inline float getF32(const unsigned char* p)
{
    const uint32_t u = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

// the state string of a saved profile ("" when there is none)
inline std::string encodeProfile(const SavedProfile& s)
{
    if (!s.ok || s.src < 1 || s.src > 2)
        return std::string();
    std::vector<unsigned char> b(kBlobBytes);
    b[0] = 1;
    b[1] = static_cast<unsigned char>(s.src);
    b[2] = s.gate ? 1 : 0;
    b[3] = s.cut ? 1 : 0;
    putF32(&b[4], s.sampleRate);
    putF32(&b[8], s.length);
    for (int k = 0; k < kNB; ++k)
        putF32(&b[12 + 4 * size_t(k)], s.avg[k]);
    return encodeBase64(b.data(), b.size());
}

// false for an empty, unknown or damaged string (then out.ok is false: no saved profile)
inline bool decodeProfile(const char* str, SavedProfile& out)
{
    out = SavedProfile();
    std::vector<unsigned char> b;
    if (!str || !str[0] || !decodeBase64(str, b) || b.size() != kBlobBytes || b[0] != 1 || b[1] < 1 || b[1] > 2)
        return false;
    const float sr = getF32(&b[4]), len = getF32(&b[8]);
    if (!(sr > 0.0f) || !std::isfinite(sr) || !std::isfinite(len))
        return false;
    for (int k = 0; k < kNB; ++k)
    {
        const float v = getF32(&b[12 + 4 * size_t(k)]);
        if (!std::isfinite(v) || v < 0.0f)
            return false;
        out.avg[k] = v;
    }
    out.src = b[1];
    out.gate = b[2] != 0;
    out.cut = b[3] != 0;
    out.sampleRate = sr;
    out.length = len;
    out.ok = true;
    return true;
}

// ---- the files the interface offers ----------------------------------------------------------

inline std::vector<std::string> noiseFolders()
{
    std::vector<std::string> dirs;
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA"))
        dirs.push_back(std::string(a) + "\\REAPER\\Data\\null_jsfx_noise");
    if (const char* h = std::getenv("USERPROFILE"))
        dirs.push_back(std::string(h) + "\\null_jsfx_noise");
#else
    const char* h = std::getenv("HOME");
    const std::string home = h ? h : "";
# ifdef __APPLE__
    dirs.push_back(home + "/Library/Application Support/REAPER/Data/null_jsfx_noise");
# else
    const char* x = std::getenv("XDG_CONFIG_HOME");
    dirs.push_back((x && x[0] ? std::string(x) : home + "/.config") + "/REAPER/Data/null_jsfx_noise");
# endif
    dirs.push_back(home + "/null_jsfx_noise");
#endif
    return dirs;
}

// full paths of the .wav files in those folders, sorted by file name
inline std::vector<std::string> listNoiseFiles()
{
    auto isWav = [](const std::string& name) {
        if (name.size() < 5 || name[0] == '.')
            return false;
        std::string ext = name.substr(name.size() - 4);
        for (char& c : ext)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return ext == ".wav";
    };
    std::vector<std::pair<std::string, std::string>> found; // (name, path)
    for (const std::string& dir : noiseFolders())
    {
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        HANDLE hd = FindFirstFileA((dir + "\\*.wav").c_str(), &fd);
        if (hd == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && isWav(fd.cFileName))
                found.emplace_back(fd.cFileName, dir + "\\" + fd.cFileName);
        } while (FindNextFileA(hd, &fd));
        FindClose(hd);
#else
        DIR* d = opendir(dir.c_str());
        if (!d)
            continue;
        while (dirent* e = readdir(d))
            if (isWav(e->d_name))
                found.emplace_back(e->d_name, dir + "/" + e->d_name);
        closedir(d);
#endif
    }
    std::sort(found.begin(), found.end());
    std::vector<std::string> paths;
    for (auto& f : found)
        paths.push_back(f.second);
    return paths;
}

} // namespace nullnr
