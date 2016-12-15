// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SIMPLERANGESET_HH
#define SIMPLERANGESET_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_SSTREAM
#include <sstream>
#define STD_SSTREAM
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

#ifndef DELTACODEC_HH
#include "DeltaCodec.hh"
#endif

class SimpleRangeSet
{
private:
    class Range
    {
    public:
        uint64_t begin() const
        {
            return unpack().first;
        }

        uint64_t end() const
        {
            return unpack().second;
        }

        /**
         * True iff all the elements in this range are before all the elements in pRhs.
         */
        bool before(const Range& pRhs) const
        {
            return end() <= pRhs.begin();
        }

        void join(const Range& pRhs)
        {
            std::pair<uint64_t,uint64_t> l = unpack();
            std::pair<uint64_t,uint64_t> r = pRhs.unpack();
            l.first = std::min(l.first, r.first);
            l.second = std::max(l.second, r.second);
            pack(l.first, l.second);
        }

        Range(uint64_t pBegin, uint64_t pEnd)
            : mWord(0)
        {
            BOOST_ASSERT(pBegin < pEnd);
            pack(pBegin, pEnd);
        }


    private:
        void pack(uint64_t pBegin, uint64_t pEnd)
        {
            uint64_t bits;
            bits = DeltaCodec::encode(pBegin + 1, mWord);
            bits += DeltaCodec::encode(pEnd - pBegin, mWord);
            BOOST_ASSERT(bits <= 64);
        }

        std::pair<uint64_t,uint64_t> unpack() const
        {
            uint64_t w = mWord;
            std::pair<uint64_t,uint64_t> x;
            x.second = DeltaCodec::decode(w);
            x.first = DeltaCodec::decode(w) - 1;
            x.second += x.first;
            return x;
        }

        uint64_t mWord;
    };

public:

    /**
     * Return true iff the set contains zero elements.
     */
    bool empty() const
    {
        return mRanges.empty();
    }

    /**
     * Return the number of elements in the set.
     * The running time is proportional to the number of
     * discontiguous ranges of elements in the set.
     */
    uint64_t size() const
    {
        uint64_t s = 0;
        for (uint64_t i = 0; i < mRanges.size(); ++i)
        {
            const Range& r(mRanges[i]);
            s += r.end() - r.begin();
        }
        return s;
    }

    /**
     * Return the number of ranges in the set.
     */
    uint64_t count() const
    {
        return mRanges.size();
    }

    /**
     * Retrieve a range from the set.
     */
    std::pair<uint64_t,uint64_t> operator[](uint64_t pIdx) const
    {
        return std::pair<uint64_t,uint64_t>(mRanges[pIdx].begin(), mRanges[pIdx].end());
    }

    /**
     * Add an element to a set. It must be > than every other element in the set.
     */
    void insert(const uint64_t& pElem)
    {
        BOOST_ASSERT(mRanges.empty() || mRanges.back().end() <= pElem);
        append(Range(pElem, pElem + 1));
    }

    /**
     * Update a set to contain its union with pRhs.
     */
    SimpleRangeSet& operator|=(const SimpleRangeSet& pRhs)
    {
        uint64_t l = 0;
        uint64_t r = 0;
        SimpleRangeSet t;
        while (l < mRanges.size() && r < pRhs.mRanges.size())
        {
            const Range& lhs(mRanges[l]);
            const Range& rhs(pRhs.mRanges[r]);
            if (lhs.before(rhs))
            {
                t.append(lhs);
                ++l;
                continue;
            }
            if (rhs.before(lhs))
            {
                t.append(rhs);
                ++r;
                continue;
            }
            Range x(std::min(lhs.begin(), rhs.begin()), std::max(lhs.end(), rhs.end()));
            t.append(x);
            ++l;
            ++r;
        }
        while (l < mRanges.size())
        {
            const Range& lhs(mRanges[l]);
            t.append(lhs);
            ++l;
        }
        while (r < pRhs.mRanges.size())
        {
            const Range& rhs(pRhs.mRanges[r]);
            t.append(rhs);
            ++r;
        }
        swap(t);
        return *this;
    }

    /**
     * Update a set to contain its intersection with pRhs.
     */
    SimpleRangeSet& operator&=(const SimpleRangeSet& pRhs)
    {
        uint64_t l = 0;
        uint64_t r = 0;
        SimpleRangeSet t;
        while (l < mRanges.size() && r < pRhs.mRanges.size())
        {
            Range& lhs(mRanges[l]);
            const Range& rhs(pRhs.mRanges[r]);
            if (lhs.before(rhs))
            {
                ++l;
            }
            else if (rhs.before(lhs))
            {
                ++r;
            }
            else if (lhs.begin() < rhs.begin())
            {
                lhs = Range(rhs.begin(), lhs.end());
            }
            else // if (lhs.begin() >= rhs.begin())
            {
                if (lhs.end() < rhs.end())
                {
                    t.append(lhs);
                    ++l;
                }
                else if (lhs.end() >= rhs.end())
                {
                    Range x(lhs.begin(), rhs.end());
                    t.append(x);
                    ++r;
                }
            }
        }
        swap(t);
        return *this;
    }

    /**
     * Update a set to eleminate any elements that are also in pRhs.
     */
    SimpleRangeSet& operator-=(const SimpleRangeSet& pRhs)
    {
        uint64_t l = 0;
        uint64_t r = 0;
        SimpleRangeSet t;
        if (mRanges.size() == 0)
        {
            return *this;
        }
        while (l < mRanges.size() && r < pRhs.mRanges.size())
        {
            Range& lhs = mRanges[l];
            const Range& rhs(pRhs.mRanges[r]);
            if (lhs.before(rhs))
            {
                t.append(lhs);
                ++l;
            }
            else if (rhs.before(lhs))
            {
                ++r;
            }
            else if (lhs.begin() < rhs.begin())
            {
                // The two ranges overlap.

                // There is at least a segment of the lhs
                // which needs to be included, so slice it off
                // and advance the lhs at least to the end of the rhs.

                Range x(lhs.begin(), rhs.begin());
                t.append(x);
                if (lhs.end() < rhs.end())
                {
                    ++l;
                }
                else if (lhs.end() > rhs.end())
                {
                    lhs = Range(rhs.end(), lhs.end());
                    ++r;
                }
                else
                {
                    ++l;
                    ++r;
                }
            }
            else if (rhs.end() < lhs.end())
            {
                // The rhs ends before the lhs, so
                // update lhs to be the projecting range,
                // but don't add it to the output since
                // next rhs segment might overlap.
                lhs = Range(rhs.end(), lhs.end());
                ++r;
            }
            else
            {
                // There is no projecting piece of the
                // lhs range.

                if (rhs.end() == lhs.end())
                {
                    ++r;
                }
                ++l;
            }
        }
        while (l < mRanges.size())
        {
            const Range& lhs = mRanges[l];
            t.append(lhs);
            ++l;
        }
        swap(t);
        return *this;
    }

    void clear()
    {
        mRanges.clear();
    }

    void swap(SimpleRangeSet& pRhs)
    {
        mRanges.swap(pRhs.mRanges);
    }

    SimpleRangeSet()
    {
    }

    SimpleRangeSet(uint64_t pBegin, uint64_t pEnd)
    {
        mRanges.push_back(Range(pBegin, pEnd));
    }

private:

    void append(const Range& pRng)
    {
        if (mRanges.empty() || mRanges.back().end() < pRng.begin())
        {
            mRanges.push_back(pRng);
        }
        else
        {
            mRanges.back().join(pRng);
        }
    }

    std::vector<Range> mRanges;
};

namespace boost
{
template<>
inline std::string lexical_cast<std::string,SimpleRangeSet>(const SimpleRangeSet& pVec)
{
    std::ostringstream out;
    for (uint64_t i = 0; i < pVec.count(); ++i)
    {
        std::pair<uint64_t,uint64_t> r = pVec[i];
        if (i > 0)
        {
            out << ' ';
        }
        out << '[' << r.first << ", " << r.second << ')';
    }
    return out.str();
}
} // namespace boost

#endif // SIMPLERANGESET_HH
