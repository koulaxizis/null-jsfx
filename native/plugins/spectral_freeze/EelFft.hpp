/*
 * NULL JSFX - EEL2's fft() / ifft() / fft_permute() / fft_ipermute() / convolve_c() in C++
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * EEL2 (WDL fft.c) works on interleaved re/im doubles, never scales, and leaves the forward
 * transform's bins in its own order: bin k of the DFT sits at complex slot perm[k], where perm
 * is WDL_fft_permute (built here with WDL's own fftfreq_c / idx_perm_calc recursion). ifft()
 * takes that order back and returns time samples in natural order, unscaled (so ifft(fft(x))
 * = N * x). fft_permute() puts the bins in natural order, fft_ipermute() back. Sizes are
 * powers of two from 16 to 32768 as in eel_fft.h (others round down; under 16 do nothing).
 * The transforms are plain radix-2 DFTs in double: the same maths as WDL's split-radix code,
 * equal to within rounding (~1e-15 relative).
 */

#pragma once

#include <cmath>
#include <utility>
#include <vector>

namespace eelfft {

namespace detail {

inline unsigned fftfreqC(unsigned i, unsigned n)
{
    if (n <= 2)
        return i;
    unsigned m = n >> 1;
    if (i < m)
        return fftfreqC(i, m) << 1;
    i -= m;
    m >>= 1;
    if (i < m)
        return (fftfreqC(i, m) << 2) + 1;
    i -= m;
    return ((fftfreqC(i, m) << 2) - 1) & (n - 1);
}

struct Tables {
    // perm[bits][k]: slot of bin k (WDL_fft_permute); tw[bits]: e^{-2 pi i k / n}, k < n/2
    std::vector<int> perm[16];
    std::vector<double> twRe[16], twIm[16];
    std::vector<int> leaders[16]; // the smallest index of every cycle of perm longer than 1
    Tables()
    {
        for (int b = 1; b <= 15; ++b)
        {
            const int n = 1 << b;
            std::vector<int>& p = perm[b];
            p.assign(size_t(n), 0);
            for (int i = 1; i < n; ++i)
                p[size_t(n - int(fftfreqC(unsigned(i), unsigned(n))))] = i; // idx_perm_calc
            std::vector<char> seen(size_t(n), 0);
            for (int i = 0; i < n; ++i)
            {
                if (seen[size_t(i)])
                    continue;
                int k = i, len = 0;
                do { seen[size_t(k)] = 1; k = p[size_t(k)]; ++len; } while (k != i);
                if (len > 1)
                    leaders[b].push_back(i);
            }
            twRe[b].resize(size_t(n / 2));
            twIm[b].resize(size_t(n / 2));
            for (int k = 0; k < n / 2; ++k)
            {
                const double a = -2.0 * M_PI * double(k) / double(n);
                twRe[b][size_t(k)] = std::cos(a);
                twIm[b][size_t(k)] = std::sin(a);
            }
        }
    }
};

inline const Tables& tables()
{
    static const Tables t;
    return t;
}

// EEL2's size rule: round down to a power of two, at most 2^15; 0 = do nothing
inline int bitsFor(int len, int minBits)
{
    int b = 0;
    while (len > 1 && b < 15)
    {
        ++b;
        len >>= 1;
    }
    return b < minBits ? 0 : b;
}

// in-place natural-order DFT of n = 2^b interleaved complex values; sign -1 forward, +1 inverse
inline void dft(double* d, int b, int sign)
{
    const int n = 1 << b;
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap(d[2 * i], d[2 * j]);
            std::swap(d[2 * i + 1], d[2 * j + 1]);
        }
    }
    const Tables& t = tables();
    const double* wr = t.twRe[b].data();
    const double* wi = t.twIm[b].data();
    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len >> 1, stride = n / len;
        for (int s = 0; s < n; s += len)
            for (int k = 0; k < half; ++k)
            {
                // twiddle e^{sign * 2 pi i k / len}
                const double tr = wr[k * stride], ti = sign < 0 ? wi[k * stride] : -wi[k * stride];
                double* a = d + 2 * (s + k);
                double* q = d + 2 * (s + k + half);
                const double xr = q[0] * tr - q[1] * ti, xi = q[0] * ti + q[1] * tr;
                q[0] = a[0] - xr;
                q[1] = a[1] - xi;
                a[0] += xr;
                a[1] += xi;
            }
    }
}

// in place, along the permutation's cycles (no allocation, like eel_fft.h's reordering)
inline void reorder(double* d, int b, bool toNatural)
{
    const Tables& t = tables();
    const int* p = t.perm[b].data();
    for (const int s : t.leaders[b])
    {
        if (toNatural) // d[k] = old d[p[k]]
        {
            const double sr = d[2 * s], si = d[2 * s + 1];
            int k = s;
            for (;;)
            {
                const int nk = p[k];
                if (nk == s)
                    break;
                d[2 * k] = d[2 * nk];
                d[2 * k + 1] = d[2 * nk + 1];
                k = nk;
            }
            d[2 * k] = sr;
            d[2 * k + 1] = si;
        }
        else // d[p[k]] = old d[k]
        {
            double ar = d[2 * s], ai = d[2 * s + 1];
            int k = s;
            for (;;)
            {
                k = p[k];
                if (k == s)
                    break;
                std::swap(ar, d[2 * k]);
                std::swap(ai, d[2 * k + 1]);
            }
            d[2 * s] = ar;
            d[2 * s + 1] = ai;
        }
    }
}

} // namespace detail

// fft(buf, size): forward transform, bins left in WDL order
inline void fft(double* buf, int size)
{
    if (const int b = detail::bitsFor(size, 4))
    {
        detail::dft(buf, b, -1);
        detail::reorder(buf, b, false);
    }
}

// ifft(buf, size): bins in WDL order in, time samples in natural order out (unscaled)
inline void ifft(double* buf, int size)
{
    if (const int b = detail::bitsFor(size, 4))
    {
        detail::reorder(buf, b, true);
        detail::dft(buf, b, +1);
    }
}

// fft_permute(buf, size): WDL order -> natural bin order
inline void fftPermute(double* buf, int size)
{
    if (const int b = detail::bitsFor(size, 3))
        detail::reorder(buf, b, true);
}

// fft_ipermute(buf, size): natural bin order -> WDL order
inline void fftIpermute(double* buf, int size)
{
    if (const int b = detail::bitsFor(size, 3))
        detail::reorder(buf, b, false);
}

// convolve_c(dest, src, size): dest[k] *= src[k] for size complex values (an even count)
inline void convolveC(double* dest, const double* src, int size)
{
    const int n = size & ~1;
    for (int k = 0; k < n; ++k)
    {
        const double ar = dest[2 * k], ai = dest[2 * k + 1], br = src[2 * k], bi = src[2 * k + 1];
        dest[2 * k] = ar * br - ai * bi;
        dest[2 * k + 1] = ai * br + ar * bi;
    }
}

} // namespace eelfft
