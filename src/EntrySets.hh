// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ENTRYSETS_HH
#define ENTRYSETS_HH

#ifndef BITVECSET_HH
#include "BitVecSet.hh"
#endif

class EntrySets
{
public:
    bool exists(uint64_t pSegRank) const
    {
        return mSetIds.access(pSegRank);
    }

    uint64_t size(uint64_t pSegRank) const
    {
        BOOST_ASSERT(mSetIds.access(pSegRank));
        uint64_t r = mSetIds.rank(pSegRank);
        BitVecSet::VecNum v(r);
        return mSetData.size(v);
    }

    uint64_t count(uint64_t pSegRank) const
    {
        BOOST_ASSERT(mSetIds.access(pSegRank));
        uint64_t r = mSetIds.rank(pSegRank);
        BitVecSet::VecNum v(r);
        return mSetData.count(v);
    }

    bool access(uint64_t pSegRank, uint64_t pPos) const
    {
        BOOST_ASSERT(mSetIds.access(pSegRank));
        uint64_t r = mSetIds.rank(pSegRank);
        BitVecSet::VecNum v(r);
        return mSetData.access(v, BitVecSet::VecPos(pPos));
    }

    void insert(uint64_t pSegRank)
    {
        BOOST_ASSERT(mSetIds.access(pSegRank) == false);
        mSetIds.update(pSegRank, true);
        uint64_t r = mSetIds.rank(pSegRank);
        mSetData.insert(BitVecSet::VecNum(r));
    }

    void insert(uint64_t pSegRank, uint64_t pReadNum)
    {
        mSetIds.update(pSegRank, true);
        uint64_t r = mSetIds.rank(pSegRank);
        BitVecSet::VecNum v(r);
        uint64_t z = mSetData.size(v);
        for (uint64_t i = z; i < pReadNum; ++i)
        {
            mSetData.insert(v, BitVecSet::VecPos(i), false);
        }
        mSetData.insert(v, BitVecSet::VecPos(pReadNum), true);
    }

    void erase(uint64_t pSegRank)
    {
        BOOST_ASSERT(mSetIds.access(pSegRank));
        uint64_t r = mSetIds.rank(pSegRank);
        BOOST_ASSERT(mSetData.size(BitVecSet::VecNum(r)) == 0);
        mSetData.erase(BitVecSet::VecNum(r));
        mSetIds.update(pSegRank, false);
    }

    void erase(uint64_t pSegRank, uint64_t pReadNum)
    {
        mSetIds.update(pSegRank, true);
        uint64_t r = mSetIds.rank(pSegRank);
        BitVecSet::VecNum v(r);
        BitVecSet::VecPos p(pReadNum);
        mSetData.erase(v, p);
        // XXX remove trailing 0s.
    }

    void save(const std::string& pBaseName, FileFactory& pFactory) const
    {
        mSetIds.save(pBaseName + "-set-ids", pFactory);
        mSetData.save(pBaseName + "-set-data", pFactory);
    }

private:
    CompactDynamicBitVector mSetIds;
    BitVecSet mSetData;
};

#endif // ENTRYSETS_HH
