// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef MACHDEPWINDOWS_HH
#define MACHDEPWINDOWS_HH

#include <windows.h>
#include <intrin.h>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/math/constants/constants.hpp>

// Windows stupidly defines max and min, which interferes with cstdlib.

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#pragma intrinsic(_InterlockedCompareExchange64)
#pragma intrinsic(_InterlockedIncrement64)
#pragma intrinsic(_InterlockedExchange64)
#pragma intrinsic(_ReadWriteBarrier)

namespace Gossamer { namespace Windows {
    extern uint32_t (*sPopcnt64)(uint64_t pWord);
} }

// Emulated gettimeofday().
extern void gettimeofday(timeval* t, void *);

// emulate gettimeofday()
void gettimeofday(timeval* t, void *);

// emulate log2
//const double LN_OF_2 = 0.693147180559945309417;


inline double log2(double x)
{
    //return std::log(x)/LN_OF_2; // ln(x)/ln(2)
    return std::log(x) / boost::math::constants::ln_two<double>(); // ln(x)/ln(2)
}

//inline float log2(float x)
//{
//    //return std::log(x)/(float)LN_OF_2; // ln(x)/ln(2)
//    return std::log(x) / boost::math::constants::ln_two<float>(); // ln(x)/ln(2)
//}
//
//inline long double log2(long double x)
//{
//    //return std::log(x)/(float)LN_OF_2; // ln(x)/ln(2)
//    return std::log(x) / boost::math::constants::ln_two<long double>(); // ln(x)/ln(2)
//}


#endif
