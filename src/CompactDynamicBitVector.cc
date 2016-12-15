// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "CompactDynamicBitVector.hh"

#include "DeltaCodec.hh"
#include "GossamerException.hh"
#include "MappedArray.hh"
#include "RunLengthCodedBitVectorWord.hh"

using namespace boost;
using namespace std;

typedef RunLengthCodedBitVectorWord<DeltaCodec> BitVecWord;

namespace // anonymous
{

class BuilderVisitor : public CompactDynamicBitVector::Visitor
{
public:
    void enter()
    {
    }

    void exit()
    {
    }

    void leaf(uint64_t pWord)
    {
        mBld.push_back(pWord);
    }

    BuilderVisitor(MappedArray<uint64_t>::Builder& pBld)
        : mBld(pBld)
    {
    }

private:
    MappedArray<uint64_t>::Builder& mBld;
};

} // namespace anonymous

int64_t
CompactDynamicBitVector::Internal::height() const
{
    return mHeight;
}

uint64_t
CompactDynamicBitVector::Internal::size() const
{
    return mSize;
}

uint64_t
CompactDynamicBitVector::Internal::count() const
{
    return mCount;
}

bool
CompactDynamicBitVector::Internal::access(uint64_t pPos) const
{
    uint64_t z = mLhs->size();
    if (pPos < z)
    {
        return mLhs->access(pPos);
    }
    else
    {
        return mRhs->access(pPos - z);
    }
}

uint64_t
CompactDynamicBitVector::Internal::rank(uint64_t pPos) const
{
    uint64_t s = mLhs->size();
    if (pPos < s)
    {
        return mLhs->rank(pPos);
    }
    else
    {
        uint64_t c = mLhs->count();
        return c + mRhs->rank(pPos - s);
    }
}

uint64_t
CompactDynamicBitVector::Internal::select(uint64_t pRnk) const
{
    uint64_t c = mLhs->count();
    if (pRnk < c)
    {
        return mLhs->select(pRnk);
    }
    else
    {
        uint64_t s = mLhs->size();
        return s + mRhs->select(pRnk - c);
    }
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::Internal::update(NodePtr& pSelf, uint64_t pPos, bool pBit)
{
    uint64_t z = mLhs->size();
    if (pPos < z)
    {
        mLhs = mLhs->update(mLhs, pPos, pBit);
    }
    else
    {
        mRhs = mRhs->update(mRhs, pPos - z, pBit);
    }
    return rebalance(pSelf);
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::Internal::insert(NodePtr& pSelf, uint64_t pPos, bool pBit)
{
    uint64_t z = mLhs->size();
    if (pPos < z)
    {
        mLhs = mLhs->insert(mLhs, pPos, pBit);
    }
    else
    {
        mRhs = mRhs->insert(mRhs, pPos - z, pBit);
    }
    return rebalance(pSelf);
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::Internal::erase(NodePtr& pSelf, uint64_t pPos)
{
    uint64_t z = mLhs->size();
    if (pPos < z)
    {
        mLhs = mLhs->erase(mLhs, pPos);
        if (mLhs->size() == 0)
        {
            return mRhs;
        }
    }
    else
    {
        mRhs = mRhs->erase(mRhs, pPos - z);
        if (mRhs->size() == 0)
        {
            return mLhs;
        }
    }
    return rebalance(pSelf);
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::Internal::rebalance(NodePtr& pSelf)
{
    int64_t b = mLhs->height() - mRhs->height();
    if (-1 <= b && b <= 1)
    {
        recompute();
        return pSelf;
    }

    if (b < -1)
    {
        BOOST_ASSERT(dynamic_cast<const Internal*>(mRhs.get()));
        Internal& r(*static_cast<Internal*>(mRhs.get()));
        int64_t br = r.mLhs->height() - r.mRhs->height();
        if (br <= 0)
        {
            return rotateLeft(pSelf);
        }
        else
        {
            mRhs = rotateRight(mRhs);
            return rotateLeft(pSelf);
        }
    }
    else
    {
        BOOST_ASSERT(b > 1);
        BOOST_ASSERT(dynamic_cast<const Internal*>(mLhs.get()));
        Internal& l(*static_cast<Internal*>(mLhs.get()));
        int64_t bl = l.mLhs->height() - l.mRhs->height();
        if (bl >= 0)
        {
            return rotateRight(pSelf);
        }
        else
        {
            mLhs = rotateLeft(mLhs);
            return rotateRight(pSelf);
        }
    }
}

int64_t
CompactDynamicBitVector::External::height() const
{
    return 0;
}

uint64_t
CompactDynamicBitVector::External::size() const
{
    return BitVecWord::size(mWord);
}

uint64_t
CompactDynamicBitVector::External::count() const
{
    return BitVecWord::count(mWord);
}

bool
CompactDynamicBitVector::External::access(uint64_t pPos) const
{
    //std::cerr << "access: " << pPos << '\t' << mWord << '\t';
    //BitVecWord::dump(mWord, std::cerr);
    bool r =  BitVecWord::rank(mWord, pPos + 1) - BitVecWord::rank(mWord, pPos);
    //std::cerr << '\t' << r << std::endl;
    return r;
}

uint64_t
CompactDynamicBitVector::External::rank(uint64_t pPos) const
{
    return BitVecWord::rank(mWord, pPos);
}

uint64_t
CompactDynamicBitVector::External::select(uint64_t pRnk) const
{
    return BitVecWord::select(mWord, pRnk);
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::External::update(NodePtr& pSelf, uint64_t pPos, bool pBit)
{
    NodePtr n = erase(pSelf, pPos);
    return insert(n, pPos, pBit);
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::External::insert(NodePtr& pSelf, uint64_t pPos, bool pBit)
{
    uint64_t w = BitVecWord::insert(mWord, pPos, pBit);
    if (w)
    {
        NodePtr n = NodePtr(new External(w));
        return NodePtr(new Internal(pSelf, n));
    }
    else
    {
        return pSelf;
    }
}

CompactDynamicBitVector::NodePtr
CompactDynamicBitVector::External::erase(NodePtr& pSelf, uint64_t pPos)
{
    uint64_t w = BitVecWord::erase(mWord, pPos);
    if (w)
    {
        NodePtr n = NodePtr(new External(w));
        return NodePtr(new Internal(pSelf, n));
    }
    else
    {
        return pSelf;
    }
}

void
CompactDynamicBitVector::save(const string& pBaseName, FileFactory& pFactory) const
{
    MappedArray<uint64_t>::Builder bld(pBaseName, pFactory);
    BuilderVisitor vis(bld);
    mRoot->visit(vis);
    bld.end();
}

CompactDynamicBitVector::CompactDynamicBitVector(uint64_t pSize)
{
    uint64_t w = 0;
    uint64_t l = BitVecWord::init(w, pSize, false);
    if (l > 64)
    {
        string s("CompactDynamicBitVector: canot encode size "
                    + lexical_cast<string>(pSize)
                    + " which requires " + lexical_cast<string>(l) + " bits.");
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info(s));
    }
    mRoot = NodePtr(new External(w));
}
