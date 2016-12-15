// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef DELTACODEC_HH
#define DELTACODEC_HH

#ifndef GAMMACODEC_HH
#include "GammaCodec.hh"
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

class DeltaCodec
{
public:
    static uint64_t decode(uint64_t& w)
    {
        uint64_t b = GammaCodec::decode(w) - 1;
        uint64_t s = 1ULL << b;
        uint64_t m = s - 1;
        uint64_t x = s | (w & m);
        w >>= b;
        return x;
    }

    static uint64_t encode(uint64_t x, uint64_t& w)
    {
        uint64_t i = 0;
        uint64_t t = x;
        while (t > 0)
        {
            ++i;
            t >>= 1;
        }
        --i;
        uint64_t j = 1ULL << i;
        w = (w << i) | (x & (j - 1));
        uint64_t l = GammaCodec::encode(i + 1, w);
        return i + l;
    }
};

#endif // DELTACODEC_HH
