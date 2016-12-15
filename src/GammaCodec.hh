// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GAMMACODEC_HH
#define GAMMACODEC_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

class GammaCodec
{
public:
    static uint64_t decode(uint64_t& w)
    {
        if (w == 0)
        {
            return 0;
        } 
        uint64_t i = 0;
        while ((w & 1) == 0)
        {
            ++i;
            w >>= 1;
        }
        uint64_t r = 1ULL << i;
        w >>= 1;
        r = r | (w & (r - 1));
        w >>= i;
        return r;
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
        w = (w << (i + 1)) | j;
        return 2 * i + 1;
    }
};

#endif // GAMMACODEC_HH
