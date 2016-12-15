// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef EDGECOMPILER_HH
#define EDGECOMPILER_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef RANKSELECT_HH
#include "RankSelect.hh"
#endif

template <typename Dest>
class EdgeCompiler
{
public:
    void prepare(uint64_t pNumEdges)
    {
        mDest.prepare(pNumEdges);
    }

    void push_back(Gossamer::position_type pEdge)
    {
        if (pEdge == mPrev)
        {
            ++mCount;
        }
        else
        {
            if (mCount > 0)
            {
                mDest.push_back(mPrev, mCount);
            }
            mPrev = pEdge;
            mCount = 1;
        }
    }

    void end()
    {
        if (mCount > 0)
        {
            mDest.push_back(mPrev, mCount);
            mCount = 0;
        }
    }

    explicit EdgeCompiler(Dest& pDest)
        : mPrev(0ULL), mCount(0), mDest(pDest)
    {
    }

    ~EdgeCompiler()
    {
        end();
    }

private:
    Gossamer::position_type mPrev;
    uint64_t mCount;
    Dest& mDest;
};

#endif // EDGECOMPILER_HH
