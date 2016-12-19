// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BLENDEDSORT_HH
#define BLENDEDSORT_HH

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_FUNCTIONAL
#include <functional>
#define STD_FUNCTIONAL
#endif

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif

#include "WorkQueue.hh"

template <typename T>
class BlendedSort
{
public:
    /// Radix to use.
    static const uint64_t R = 8;
    static const uint64_t S = 1ULL << R;

    template <uint64_t N>
    static uint64_t roundUp(uint64_t pX)
    {
        return ((pX + N - 1) / N) * N;
    }

    template <typename Cmp>
    static void sort(uint64_t pThreads, std::vector<T>& pItems, uint64_t pRadixBits, const Cmp& pCmp)
    {
        WorkQueue q(pThreads);
        BOOST_ASSERT(pRadixBits >= 1 && pRadixBits <= 64);
        //std::cerr << "pRadixBits = " << pRadixBits << std::endl;
        rsort(&q, &pItems, roundUp<R>(pRadixBits) - pRadixBits, (pRadixBits - 1) / R, 0, pItems.size(), &pCmp);
        q.wait();
    }

    template <typename Cmp>
    static void rsort(WorkQueue* pQueue, std::vector<T>* pItems, uint64_t pUpShift, uint64_t pDownShift, uint64_t pBegin, uint64_t pEnd, const Cmp* pCmp)
    {
        //std::cerr << "rsort " << (pEnd - pBegin) << '\t' << pUpShift << '\t' << pDownShift << std::endl;
        if (pEnd - pBegin < 1024)
        {
            std::sort(pItems->begin() + pBegin, pItems->begin() + pEnd, *pCmp);
            return;
        }

        uint64_t counts[S];
        for (uint64_t i = 0; i < S; ++i)
        {
            counts[i] = 0;
        }

        // Do the frequency counting step.
        //
        for (uint64_t i = pBegin; i < pEnd; ++i)
        {
            uint64_t r = ((pCmp->radix((*pItems)[i]) << pUpShift) >> (R * pDownShift)) % S;
            counts[r]++;
        }

        // Compute the cumulative frequencies to give the offset of
        // each of the buckets, relative to pBegin.
        uint64_t cumu[S + 1];
        cumu[0] = 0;
        for (uint64_t i = 1; i < S + 1; ++i)
        {
            cumu[i] = cumu[i - 1] + counts[i - 1];
        }

        {
            std::vector<T> tmp(pEnd - pBegin, pCmp->zero());
            for (uint64_t i = pBegin; i < pEnd; ++i)
            {
                uint64_t r = ((pCmp->radix((*pItems)[i]) << pUpShift) >> (R * pDownShift)) % S;
                tmp[cumu[r]++] = (*pItems)[i];
            }
            for (uint64_t i = 0; i < pEnd - pBegin; ++i)
            {
                (*pItems)[pBegin + i] = tmp[i];
            }
            
#if 0
            // Check that we've partitioned correctly
            std::vector<std::pair<T,T> > bounds;
            for (uint64_t i = 0, p = 0; i < S; ++i)
            {
                if (counts[i] == 0)
                {
                    continue;
                }
                std::pair<T,T> b = minMax(pItems, pCmp, pBegin + p, pBegin + p + counts[i]);
                bounds.push_back(b);

                p += counts[i];
            }
            for (uint64_t i = 1; i < bounds.size(); ++i)
            {
                BOOST_ASSERT((*pCmp)(bounds[i - 1].second, bounds[i].first));
            }
#endif
        }

        if (pDownShift > 0)
        {
            // Do a recursive radix sort.
            //
            for (uint64_t i = 0, p = 0; i < S; ++i)
            {
                if (counts[i] == 0)
                {
                    continue;
                }
                //BlendedSort<T>::sort(pItems, pShift - 1, pBegin + p, pBegin + p + counts[i], pCmp, pTmp);
                //std::cerr << "queuing " << (pBegin + p) << "\t" << (pBegin + p + counts[i]) << std::endl;
                WorkQueue::Item itm = std::bind(&BlendedSort<T>::template rsort<Cmp>, pQueue, pItems, pUpShift, pDownShift - 1, pBegin + p, pBegin + p + counts[i], pCmp);
                pQueue->push_back(itm);

                p += counts[i];
            }
        }
        else
        {
            // We've run out of radix bits, so use std::sort.
            //
            for (uint64_t i = 0, p = 0; i < S; ++i)
            {
                if (counts[i] == 0)
                {
                    continue;
                }
                //std::cerr << "invoking std::sort on " << counts[i] << " items." << std::endl;
                std::sort(pItems->begin() + pBegin + p, pItems->begin() + pBegin + p + counts[i], *pCmp);
                p += counts[i];
            }
        }
    }

    template <typename Cmp>
    static std::pair<T,T> minMax(const std::vector<T>* pItems, const Cmp* pCmp, uint64_t pBegin, uint64_t pEnd)
    {
        BOOST_ASSERT(pBegin < pEnd);
        std::pair<T,T> p((*pItems)[pBegin], (*pItems)[pBegin]);
        for (uint64_t i = pBegin + 1; i < pEnd; ++i)
        {
            if ((*pCmp)((*pItems)[i], p.first))
            {
                p.first = (*pItems)[i];
            }
            if ((*pCmp)(p.second, (*pItems)[i]))
            {
                p.second = (*pItems)[i];
            }
        }
        return p;
    }
};

#endif // BLENDEDSORT_HH
