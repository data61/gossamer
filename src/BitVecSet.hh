// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BITVECSET_HH
#define BITVECSET_HH

#ifndef TAGGEDNUM_HH
#include "TaggedNum.hh"
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef COMPACTDYNAMICBITVECTOR_HH
#include "CompactDynamicBitVector.hh"
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

class BitVecSet
{
public:

    struct VecNumTag {};
    typedef TaggedNum<VecNumTag> VecNum;

    struct VecPosTag {};
    typedef TaggedNum<VecPosTag> VecPos;

    uint64_t size() const
    {
        return mToc.count() - 1;
    }

    uint64_t size(const VecNum& pVecNum) const
    {
        uint64_t v = pVecNum.value();
        uint64_t i = mToc.select(v) - v;
        uint64_t j = mToc.select(v + 1) - v - 1;
        return j - i;
    }

    uint64_t count(const VecNum& pVecNum) const
    {
        uint64_t v = pVecNum.value();
        uint64_t i = mToc.select(v) - v;
        uint64_t j = mToc.select(v + 1) - v - 1;
        return mBits.rank(j) - mBits.rank(i);
    }

    bool access(const VecNum& pVecNum, const VecPos& pVecPos) const
    {
        BOOST_ASSERT(pVecPos.value() < size(pVecNum));
        uint64_t j = mToc.select(pVecNum.value()) - pVecNum.value();
        return mBits.access(j + pVecPos.value());
    }

    void insert(const VecNum& pVecNum)
    {
        BOOST_ASSERT(pVecNum.value() <= mToc.count() - 1);
        uint64_t p = mToc.select(pVecNum.value());
        mToc.insert(p, 1);
    }

    void insert(const VecNum& pVecNum, const VecPos& pVecPos, bool pVal)
    {
        BOOST_ASSERT(pVecNum.value() < size());
        BOOST_ASSERT(pVecPos.value() <= size(pVecNum));
        uint64_t p = mToc.select(pVecNum.value());
        uint64_t j = p - pVecNum.value();
        mToc.insert(p + 1, 0);
        mBits.insert(j + pVecPos.value(), pVal);
    }

    void update(const VecNum& pVecNum, const VecPos& pVecPos, bool pVal)
    {
        BOOST_ASSERT(pVecPos.value() < size(pVecNum));
        uint64_t j = mToc.select(pVecNum.value()) - pVecNum.value();
        return mBits.update(j + pVecPos.value(), pVal);
    }

    void erase(const VecNum& pVecNum)
    {
        BOOST_ASSERT(size(pVecNum) == 0);
        uint64_t p = mToc.select(pVecNum.value());
        mToc.erase(p);
    }

    void erase(const VecNum& pVecNum, const VecPos& pVecPos)
    {
        BOOST_ASSERT(pVecNum.value() < size());
        BOOST_ASSERT(pVecPos.value() <= size(pVecNum));
        uint64_t p = mToc.select(pVecNum.value());
        uint64_t j = p - pVecNum.value();
        mToc.erase(p + 1);
        mBits.erase(j + pVecPos.value());
    }

    void clear(const VecNum& pVecNum)
    {
        uint64_t z = size(pVecNum);
        for (uint64_t i = 0; i < z; ++i)
        {
            erase(pVecNum, VecPos(0));
        }
    }

    void dump(std::ostream& pOut) const
    {
        pOut << "toc: ";
        for (uint64_t i = 0; i < mToc.size(); ++i)
        {
            pOut << ("01"[mToc.access(i)]);
        }
        pOut << std::endl;
        pOut << "bit: ";
        for (uint64_t i = 0; i < mBits.size(); ++i)
        {
            pOut << ("01"[mBits.access(i)]);
        }
        pOut << std::endl;
    }

    void save(const std::string& pBaseName, FileFactory& pFactory) const
    {
        mToc.save(pBaseName + ".toc", pFactory);
        mBits.save(pBaseName + ".bits", pFactory);
    }

    BitVecSet()
    {
        mToc.insert(0, 1);
    }

private:
    CompactDynamicBitVector mToc;
    CompactDynamicBitVector mBits;
};

#endif // BITVECSET_HH
