// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef VWORD32CODEC_HH
#define VWORD32CODEC_HH

namespace VWord32Codec
{
    uint64_t encodingLength(uint64_t pX)
    {
        if (pX < (1ULL << 31))
        {
            return 1;
        }
        if (pX < (1ULL << 62))
        {
            return 2;
        }
        return 3;
    }

    template <typename Vec>
    void encode(uint64_t pX, Vec& pVec)
    {
        if (pX < (1ULL << 31))
        {
            pVec.push_back(static_cast<uint32_t>(pX) << 1);
            return;
        }
        if (pX < (1ULL << 62))
        {
            pVec.push_back((static_cast<uint32_t>(pX >> 31) << 1) | 1);
            pVec.push_back(static_cast<uint32_t>(pX) << 1);
        }
        pVec.push_back((static_cast<uint32_t>(pX >> 62) << 1) | 1);
        pVec.push_back((static_cast<uint32_t>(pX >> 31) << 1) | 1);
        pVec.push_back(static_cast<uint32_t>(pX) << 1);
    }

    template <typename Itr>
    uint64_t decode(Itr& pItr)
    {
        uint32_t w = *pItr;
        ++pItr;
        uint64_t r = w >> 1;
        while (w & 1)
        {
            w = *pItr;
            ++pItr;
            r = (r << 31) | (w >> 1);
        }
        return r;
    }
}
// namespace VWord32Codec

#endif // VWORD32CODEC_HH
