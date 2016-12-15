// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PAIRLINK_HH
#define PAIRLINK_HH

#ifndef VBYTECODEC_HH
#include "VByteCodec.hh"
#endif

#ifndef SUPERPATHID_HH
#include "SuperPathId.hh"
#endif

struct PairLink
{
    SuperPathId lhs;
    SuperPathId rhs;
    int64_t lhsOffset;
    int64_t rhsOffset;

    PairLink(const SuperPathId& pLhs, const SuperPathId& pRhs,
             const uint64_t& pLhsOffset, const uint64_t& pRhsOffset)
        : lhs(pLhs), rhs(pRhs), lhsOffset(pLhsOffset), rhsOffset(pRhsOffset)
    {
    }

    static const uint64_t maxBytes = 4 * 9 / 8 + 1;

    template <typename Vec>
    static void encode(const PairLink& pLink, Vec& pVec)
    {

        VByteCodec::encode(pLink.lhs.value(), pVec);

        VByteCodec::encode(pLink.rhs.value(), pVec);

        uint64_t x;

        if (pLink.lhsOffset >= 0)
        {
            x = pLink.lhsOffset << 1;
        }
        else
        {
            x = ((-pLink.lhsOffset) << 1) | 1;
        }
        VByteCodec::encode(x, pVec);

        if (pLink.rhsOffset >= 0)
        {
            x = pLink.rhsOffset << 1;
        }
        else
        {
            x = ((-pLink.rhsOffset) << 1) | 1;
        }
        VByteCodec::encode(x, pVec);
    }

    template <typename Itr>
    static void decode(Itr& pItr, PairLink& pLink)
    {
        pLink.lhs.value() = VByteCodec::decode(pItr);

        pLink.rhs.value() = VByteCodec::decode(pItr);

        uint64_t x;

        x = VByteCodec::decode(pItr);
        if (x & 1)
        {
            pLink.lhsOffset = -static_cast<int64_t>(x >> 1);
        }
        else
        {
            pLink.lhsOffset = static_cast<int64_t>(x >> 1);
        }

        x = VByteCodec::decode(pItr);
        if (x & 1)
        {
            pLink.rhsOffset = -static_cast<int64_t>(x >> 1);
        }
        else
        {
            pLink.rhsOffset = static_cast<int64_t>(x >> 1);
        }
    }
};

#endif // PAIRLINK_HH
