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
