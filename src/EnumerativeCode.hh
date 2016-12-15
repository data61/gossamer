// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ENUMERATIVECODE_HH
#define ENUMERATIVECODE_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

template<int Bits>
class EnumerativeCode
{
private:
    static const uint64_t s_wordBits = Bits;
    static const uint64_t s_tableSize = (s_wordBits + 1) * (s_wordBits + 2) / 2;

    // BOOST_STATIC_ASSERT(s_wordBits <= 64);

    uint64_t mChoose[s_tableSize];
    uint64_t mChooseLog2[s_wordBits+1];
public:

    EnumerativeCode();

    uint64_t choose(uint64_t n, uint64_t k) const
    {
        return k>n ? 0 : mChoose[n*(n+1)/2 + k];
    }

    uint64_t numCodeBits(uint64_t ones) const
    {
        return mChooseLog2[ones];
    }

    uint64_t numCodes(uint64_t ones) const
    {
        return choose(s_wordBits, ones);
    }

    uint64_t encode(uint64_t ones, uint64_t bits) const
    {
        uint64_t ordinal = 0;
        for (uint64_t bit = s_wordBits - 1; ones > 0; --bit)
        {
            if (bits & (1ull << bit))
            {
                ordinal += choose(bit, ones);
                --ones;
            }
        }
        return ordinal;
    }

    uint64_t decode(uint64_t ones, uint64_t ordinal) const
    {
        uint64_t bits = 0;
        for (uint64_t bit = s_wordBits - 1; ones > 0; --bit)
        {
            uint64_t nCk = choose(bit, ones);
            if (ordinal >= nCk)
            {
                ordinal -= nCk;
                bits |= 1ull << bit;
                --ones;
            }
        }
        return bits;
    }
};

#include "EnumerativeCode.tcc"

#endif
