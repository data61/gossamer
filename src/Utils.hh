// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef UTILS_HH
#define UTILS_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef MATH_H
#include <math.h>
#define MATH_H
#endif

#ifndef STRING_H
#include <string.h>
#define STRING_H
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_STATIC_ASSERT_HPP
#include <boost/static_assert.hpp>
#define BOOST_STATIC_ASSERT_HPP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef STD_ITERATOR
#include <iterator>
#define STD_ITERATOR
#endif

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif

#ifndef BOOST_MATH_CONSTANTS_CONSTANTS_HPP
#include <boost/math/constants/constants.hpp>
#define BOOST_MATH_CONSTANTS_CONSTANTS_HPP
#endif

#ifndef BOOST_OPERATORS_HPP
#include <boost/operators.hpp>
#define BOOST_OPERATORS_HPP
#endif

#ifndef SIGNAL_H
#include <signal.h>
#define SIGNAL_H
#endif

// Identify the platform.  At the moment, we only have three
// platforms:
//
//     - Linux x86-64 (here called GOSS_LINUX_X64), GCC or Clang
//     - Mac OSX x86-64 (here called GOSS_MACOSX_X64), Clang
//     - Windows x86-64 (here called GOSS_WINDOWS_X64), MSVC (untested)

#undef GOSS_WINDOWS_X64
#undef GOSS_MACOSX_X64
#undef GOSS_LINUX_X64

#if defined(GOSS_PLATFORM_WINDOWS)

#ifndef GOSS_COMPILER_MSVC
#warning "This appears to be a build on Windows with a compiler other than MSVC. This may not work."
#endif

#define GOSS_WINDOWS_X64
#include "MachDepWindows.hh"

#elif defined(GOSS_PLATFORM_OSX)

#ifndef GOSS_COMPILER_CLANG
#error "This appears to be a build on OS X with a compiler other than Clang. This is an untested combination."
#endif

#if defined(__x86_64__)
#define GOSS_MACOSX_X64
#include "MachDepMacOSX.hh"
#else
#error "This appears to be a build on non-Intel OS X."
#endif

#elif defined(GOSS_PLATFORM_UNIX)

#if defined(linux) || defined(__linux__)
#define GOSS_PLATFORM_LINUX
#if !defined(GOSS_COMPILER_GNU) && !defined(GOSS_COMPILER_CLANG)
#error "This appears to be a build on Linux with an unknown compiler."
#endif
#else
#error "This appears to be a build on an unknown Unix-like OS. This is an untested combination."
#endif

#if !defined(__x86_64__)
#error "This appears to be a build on a non-Intel Unix-like platform. This is an untested combination."
#endif

#define GOSS_LINUX_X64
#include "MachDepLinux.hh"

#else

#error "Can't work out what platform this is. This build will probably fail."

#endif


class MachineAutoSetup
{
public:
    MachineAutoSetup(bool pDelaySetup = false)
    {
        if (!pDelaySetup)
        {
            setup();        
        }
    }

    void operator()()
    {
        setup();
    }

private:
    static void setup();
    static void setupMachineSpecific();
};

namespace Gossamer
{


/// Get the number of logical processors on this machine.
uint32_t logicalProcessorCount();


inline uint32_t popcnt(uint64_t pWord)
{
    BOOST_STATIC_ASSERT(sizeof(long) == 8 || sizeof(unsigned long long) == 8);

#if defined(GOSS_WINDOWS_X64)
    return (*Windows::sPopcnt64)(pWord);
#elif defined(GOSS_LINUX_X64) || defined(GOSS_MACOSX_X64)
    if (sizeof(long) == 8)
    {
        return __builtin_popcountl(pWord);
    }
    else if (sizeof(unsigned long long) == 8)
    {
        return __builtin_popcountll(pWord);
    }
#else
#error "This platform is not yet supported"
#endif
}

inline uint64_t count_leading_zeroes(uint64_t pWord)
{
    BOOST_STATIC_ASSERT(sizeof(long) == 8 || sizeof(unsigned long long) == 8);

#if defined(GOSS_LINUX_X64) || defined(GOSS_MACOSX_X64)
    if (sizeof(long) == 8)
    {
        // Returns the number of leading 0-bits in x, starting at
        // the most significant bit position. If x is 0, the result
        // is undefined
        return pWord ? __builtin_clzl(pWord) : 64;
    }
    else if (sizeof(unsigned long long) == 8)
    {
        return pWord ? __builtin_clzll(pWord) : 64;
    }
#elif defined(GOSS_WINDOWS_X64)
    // Note that the GNUC intrinsic function will return undefined
    // values if the input is zero. So lets be sane here and return
    // 64 which should mean all zeros.
    if( pWord == 0 )
    {
        return 64;
    }
    else
    {
        unsigned long idx;
        // If a set bit is found, the bit position (zero based) of the first
        // set bit found is returned in the first parameter.
        // If no set bit is found, 0 is returned; otherwise, 1 is returned.
        _BitScanReverse64(&idx, pWord);
        // Should return the number of leading zeros.
        return (63-idx);
    }
#else
    pWord |= pWord >> 1;
    pWord |= pWord >> 2;
    pWord |= pWord >> 4;
    pWord |= pWord >> 8;
    pWord |= pWord >> 16;
    pWord |= pWord >> 32;

    return popcnt(~pWord);
#endif
}

inline uint64_t find_first_set(uint64_t pWord)
{
    BOOST_STATIC_ASSERT(sizeof(long) == 8 || sizeof(unsigned long long) == 8);

#if defined(GOSS_LINUX_X64) || defined(GOSS_MACOSX_X64)
    if (sizeof(long) == 8)
    {
        // Returns one plus the index of the least significant 1-bit
        // of x, or if x is zero, returns zero.
        return __builtin_ffsl(pWord);
    }
    else if (sizeof(unsigned long long) == 8)
    {
        // Returns one plus the index of the least significant 1-bit
        // of x, or if x is zero, returns zero.
        return __builtin_ffsll(pWord);
    }
#elif defined(GOSS_WINDOWS_X64)
 
    // _BitScanForward64 returns the zero based
    // index of the first set bit. We want 1 
    // Note that VC++'s documentation does not
    // say whether idx will be set to zero or remain
    // unmodified when input is zero.
    if( pWord == 0 )
    {
        return 0;
    }
    else
    {
        unsigned long idx;
        _BitScanForward64(&idx, pWord);
        return idx + 1;
    }

#else
    // ffsl() is in XSB as of POSIX:2004.
    // This may need to be ported to non-compliant platforms.
    // GOSS_WINDOWS_X64 seems to work fine.
    //
    // Some platforms provide ffsll(), but at the moment
    // they all seem to be __GNUC__ platforms too.
    if (sizeof(long) == 8)
    {
        return ffsl(static_cast<long>(pWord));
    }
    else if (sizeof(long) == 4)
    {
        return pWord & ((1ull<<32)-1)
            ? ffsl(static_cast<long>(pWord))
            : ffsl(static_cast<long>(pWord >> 32));
    }
#endif
}


inline uint64_t select_by_ffs(uint64_t pWord, uint64_t pR)
{
    uint64_t bit = 0;
    for (++pR; pR > 0; --pR)
    {
        bit = pWord & -pWord;
        pWord &= ~bit;
    }
    return find_first_set(bit) - 1;
}


// The following algorithm is from Vigna, "Broadword implementation of
// rank/select queries".

const uint64_t sMSBs8 = 0x8080808080808080ull;
const uint64_t sLSBs8 = 0x0101010101010101ull;


inline uint64_t
leq_bytes(uint64_t pX, uint64_t pY)
{
    return ((((pY | sMSBs8) - (pX & ~sMSBs8)) ^ pX ^ pY) & sMSBs8) >> 7;
}


inline uint64_t
gt_zero_bytes(uint64_t pX)
{
    return ((pX | ((pX | sMSBs8) - sLSBs8)) & sMSBs8) >> 7;
}


inline uint64_t select_by_vigna(uint64_t pWord, uint64_t pR)
{
    const uint64_t sOnesStep4  = 0x1111111111111111ull;
    const uint64_t sIncrStep8  = 0x8040201008040201ull;

    uint64_t byte_sums = pWord - ((pWord & 0xA*sOnesStep4) >> 1);
    byte_sums = (byte_sums & 3*sOnesStep4) + ((byte_sums >> 2) & 3*sOnesStep4);
    byte_sums = (byte_sums + (byte_sums >> 4)) & 0xF*sLSBs8;
    byte_sums *= sLSBs8;

    const uint64_t k_step_8 = pR * sLSBs8;
    const uint64_t place
        = (leq_bytes( byte_sums, k_step_8 ) * sLSBs8 >> 53) & ~0x7;

    const int byte_rank = pR - (((byte_sums << 8) >> place) & 0xFF);

    const uint64_t spread_bits = (pWord >> place & 0xFF) * sLSBs8 & sIncrStep8;
    const uint64_t bit_sums = gt_zero_bytes(spread_bits) * sLSBs8;

    const uint64_t byte_rank_step_8 = byte_rank * sLSBs8;

    return place + (leq_bytes( bit_sums, byte_rank_step_8 ) * sLSBs8 >> 56);
}


inline uint64_t select1(uint64_t pWord, uint64_t pRank)
{
    return select_by_vigna(pWord, pRank);
}


inline uint64_t log2(uint64_t pX)
{
#if defined(GOSS_WINDOWS_X64) || defined(GOSS_LINUX_X64) || defined(GOSS_MACOSX_X64)
    return pX == 1 ? 0 : (64 - count_leading_zeroes(pX - 1));
#else
    pWord |= pWord >> 1;
    pWord |= pWord >> 2;
    pWord |= pWord >> 4;
    pWord |= pWord >> 8;
    pWord |= pWord >> 16;
    pWord |= pWord >> 32;
    return popcnt(pWord) - 1;
#endif
}


inline uint64_t roundUpToNextPowerOf2(uint64_t pX)
{
    if (pX == 0)
    {
        return 1ull;
    }
    uint64_t lowerBound = 1ull << log2(pX); 
    return (pX == lowerBound) ? pX : (lowerBound << 1);
}


// Base-4 hamming distance
inline uint32_t ham(uint64_t x, uint64_t y)
{
    static const uint64_t m = 0x5555555555555555ULL;
    uint64_t v = x ^ y;
    return popcnt((v & m) | ((v >> 1) & m));
}


// Base-4 reverse
inline uint64_t rev(uint64_t x)
{
    static const uint64_t m2  = 0x3333333333333333ULL;
    static const uint64_t m2p = m2 << 2;
    static const uint64_t m4  = 0x0F0F0F0F0F0F0F0FULL;
    static const uint64_t m4p = m4 << 4;
    static const uint64_t m8  = 0x00FF00FF00FF00FFULL;
    static const uint64_t m8p = m8 << 8;
    static const uint64_t m16 = 0x0000FFFF0000FFFFULL;
    static const uint64_t m16p = m16 << 16;
    static const uint64_t m32 = 0x00000000FFFFFFFFULL;
    static const uint64_t m32p = m32 << 32;

    x = ((x & m2) << 2) | ((x & m2p) >> 2);
    x = ((x & m4) << 4) | ((x & m4p) >> 4);
    x = ((x & m8) << 8) | ((x & m8p) >> 8);
    x = ((x & m16) << 16) | ((x & m16p) >> 16);
    x = ((x & m32) << 32) | ((x & m32p) >> 32);
    return x;
}


// Reverse complement
inline uint64_t reverseComplement(const uint64_t k, uint64_t x)
{
    x = rev(~x);
    return x >> (2 * (32 - k));
}


template <int X>
struct Log2
{
};

template <> struct Log2<1> { static const uint64_t value = 0; };
template <> struct Log2<2> { static const uint64_t value = 1; };
template <> struct Log2<3> { static const uint64_t value = 2; };
template <> struct Log2<4> { static const uint64_t value = 2; };
template <> struct Log2<5> { static const uint64_t value = 3; };
template <> struct Log2<6> { static const uint64_t value = 3; };
template <> struct Log2<7> { static const uint64_t value = 3; };
template <> struct Log2<8> { static const uint64_t value = 3; };
template <> struct Log2<9> { static const uint64_t value = 4; };
template <> struct Log2<10> { static const uint64_t value = 4; };
template <> struct Log2<11> { static const uint64_t value = 4; };
template <> struct Log2<12> { static const uint64_t value = 4; };
template <> struct Log2<13> { static const uint64_t value = 4; };
template <> struct Log2<14> { static const uint64_t value = 4; };
template <> struct Log2<15> { static const uint64_t value = 4; };
template <> struct Log2<16> { static const uint64_t value = 4; };
template <> struct Log2<17> { static const uint64_t value = 5; };
template <> struct Log2<18> { static const uint64_t value = 5; };
template <> struct Log2<19> { static const uint64_t value = 5; };


// Ramanujan's approximation to the logarithm of factorial.
inline double logFac(uint64_t n)
{
    if (n < 2)
    {
        return 0;
    }
    return n * log((long double)n) - n + log((long double)(n * (1 + 4 * n * (1 + 2 * n)))) / 6 + log(boost::math::constants::pi<long double>()) / 2;
}


// Approximation to the logarithm of { n \choose k }
inline double logChoose(uint64_t n, uint64_t k)
{
    return logFac(n) - logFac(k) - logFac(n - k);
}


template<typename T>
inline
T clamp(T pMin, T pX, T pMax)
{
    if (pX < pMin)
    {
        return pMin;
    }
    if (pX > pMax)
    {
        return pMax;
    }
    return pX;
}


// Ensure that a container has enough capacity for some new
// elements, doing a geometric resize if not.
//
template<class Container>
inline void
ensureCapacity(Container& pContainer, uint64_t pNewElements = 1)
{
    uint64_t oldSize = pContainer.size();
    uint64_t oldCapacity = pContainer.capacity();
    uint64_t newCapacity = oldSize + pNewElements;
    if (newCapacity < oldCapacity)
    {
        newCapacity = std::min<uint64_t>(newCapacity,
                        (oldCapacity * 3 + 1) / 2);
        newCapacity = std::max<uint64_t>(newCapacity, 4);
        pContainer.reserve(newCapacity);
    }
}


// Sort-and-unique a container.
//
template<class Container>
inline void
sortAndUnique(Container& pContainer)
{
    using namespace std;
    sort(pContainer.begin(), pContainer.end());
    pContainer.erase(unique(pContainer.begin(), pContainer.end()),
            pContainer.end());
}


std::string defaultTmpDir(); // OS dependent


// Helper class to implement the empty member optimisation.
// See http://www.cantrip.org/emptyopt.html for details.
//
template<class Base, class Member>
struct BaseOpt : Base
{
    Member m;

    BaseOpt(const Base& pBase, const Member& pMember)
        : Base(pBase), m(pMember)
    {
    }

    BaseOpt(const Base& pBase)
        : Base(pBase)
    {
    }

    BaseOpt(const Member& pMember)
        : m(pMember)
    {
    }

    BaseOpt(const BaseOpt& pOpt)
        : Base(pOpt), m(pOpt.m)
    {
    }

    BaseOpt()
    {
    }

    void swap(BaseOpt& pOpt)
    {
        std::swap<Base>(*this, pOpt);
        std::swap(m, pOpt.m);
    }
};


// Custom version of lower_bound, optimised for pointers.
//
template<typename T>
const T*
lower_bound(const T* pBegin, const T* pEnd, const T& pKey)
{
    ptrdiff_t len = pEnd - pBegin;

    while (len > 0)
    {
        ptrdiff_t half = len >> 1;
        const T* middle = pBegin + half;
        if (*middle < pKey)
        {
            pBegin = middle + 1;
            len -= half + 1;
        }
        else
        {
            len = half;
        }
    }

    return pBegin;
}


// Custom version of lower_bound, which converts from binary
// search to linear search once the gap is small enough.
//
template<typename It, typename T, typename Comp, int Min>
It
tuned_lower_bound(const It& pBegin, const It& pEnd, const T& pKey, Comp pComp)
{
    auto len = std::distance(pBegin, pEnd);
    It s = pBegin;

    while (len > Min)
    {
        auto half = len >> 1;
        It m = s;
        std::advance(m, half);
        if (pComp(*m, pKey))
        {
            s = m;
            std::advance(s, 1);
            len -= half + 1;
        }
        else
        {
            len = half;
        }
    }

    while (len > 0 && pComp(*s,pKey)) {
        --len;
        ++s;
    }

    return s;
}


// Custom version of upper_bound, optimised for pointers.
//
template<typename T>
const T*
upper_bound(const T* pBegin, const T* pEnd, const T& pKey)
{
    ptrdiff_t len = pEnd - pBegin;

    while (len > 0)
    {
        ptrdiff_t half = len >> 1;
        const T* middle = pBegin + half;
        if (pKey < *middle)
        {
            len = half;
        }
        else
        {
            pBegin = middle + 1;
            len -= half + 1;
        }
    }

    return pBegin;
}



// Custom version of upper_bound, which converts from binary
// search to linear search once the gap is small enough.
//
template<typename It, typename T, typename Comp, int Min>
It
tuned_upper_bound(const It& pBegin, const It& pEnd, const T& pKey, Comp pComp)
{
    auto len = std::distance(pBegin, pEnd);
    It s = pBegin;

    while (len > Min)
    {
        auto half = len >> 1;
        It m = s;
        std::advance(m, half);
        if (pComp(pKey,*m))
        {
            len = half;
        }
        else
        {
            s = m;
            std::advance(s, 1);
            len -= half + 1;
        }
    }

    std::advance(s, len);
    while (len > 0) {
        --s;
        if (!pComp(pKey, *s)) {
            ++s;
            break;
        }
        --len;
    }

    return s;
}


} // namespace Gossamer

#endif // UTILS_HH
