// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FASTAKMEREXTRACTOR_HH
#define FASTAKMEREXTRACTOR_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_ISTREAM
#include <istream>
#define STD_ISTREAM
#endif

#ifndef FASTQPARSER_HH
#include "FastaParser.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

class FastaKmerExtractor
{
public:
    bool valid() const
    {
        return mCurr != mEnd;
    }

    uint64_t operator*() const
    {
        return *mCurr;
    }

    void operator++()
    {
        ++mCurr;
        getNext();
    }

    FastaKmerExtractor(const std::string& pFn, uint64_t pK, FileFactory& pFactory)
        : mK(pK), mIn(pFactory.in(pFn)), mSrc(**mIn),
          mCurr(mEdges.begin()), mEnd(mEdges.end())
    {
        getNext();
    }

private:

    void getNext()
    {

        if (mCurr != mEnd)
        {
            return;
        }
        if (!mSrc.valid())
        {
            return;
        }

        while (mCurr == mEnd && mSrc.valid())
        {
            const std::string& r(mSrc.read());
            //std::cerr << r << std::endl;

            mEdges.clear();
            for (uint64_t i = 0; i <= r.size() - mK; ++i)
            {
                uint64_t x;
                if (!getEdge(r, i, mK, x))
                {
                    continue;
                }
                mEdges.push_back(x);
                uint64_t y = 0;
                for (uint64_t j = 0; j < mK; ++j)
                {
                    y = (y << 2) | (3 - (x & 3));
                    x >>= 2;
                }
                mEdges.push_back(y);
            }
            mCurr = mEdges.begin();
            mEnd = mEdges.end();
            mSrc.next();
        }
    }

    static bool getEdge(const std::string& pStr, uint64_t pOff, uint64_t pLen,
                        uint64_t& pRes)
    {
        uint64_t x = 0;
        for (uint64_t i = pOff; i < pOff + pLen; ++i)
        {
            switch (pStr[i])
            {
                case 'A':
                case 'a':
                {
                    x = (x << 2) | 0UL;
                    break;
                }
                case 'C':
                case 'c':
                {
                    x = (x << 2) | 1UL;
                    break;
                }
                case 'G':
                case 'g':
                {
                    x = (x << 2) | 2UL;
                    break;
                }
                case 'T':
                case 't':
                {
                    x = (x << 2) | 3UL;
                    break;
                }
                default:
                {
                    return false;
                }
            }
        }
        pRes = x;
        return true;
    }

    const uint64_t mK;
    FileFactory::InHolderPtr mIn;
    FastaParser mSrc;
    std::vector<uint64_t> mEdges;
    std::vector<uint64_t>::const_iterator mCurr;
    std::vector<uint64_t>::const_iterator mEnd;
};

#endif // FASTAKMEREXTRACTOR_HH
