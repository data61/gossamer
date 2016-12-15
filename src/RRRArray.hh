// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef RRRARRAY_HH
#define RRRARRAY_HH


#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef ENUMERATIVECODE_HH
#include "EnumerativeCode.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef FIXEDWIDTHBITARRAY_HH
#include "FixedWidthBitArray.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef VARIABLEWIDTHBITARRAY_HH
#include "VariableWidthBitArray.hh"
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class RRRBase;
class RRRRank;
class RRRArray;

class RRRBase
{
public:
    static const uint64_t one = 1;
    static const uint64_t logN = 20;
    static const uint64_t N = one << logN;
    static const uint64_t U = 15;
    static const uint64_t K = one << (logN / 2);
    static const uint64_t C = Gossamer::Log2<U + 1>::value;

    static EnumerativeCode<U> sEnumCode;
};


/// RRRArray with rank only.
class RRRRank : public RRRBase
{
    friend class Iterator;

public:
    typedef uint64_t position_type;
    typedef uint64_t rank_type;

private:
    static const uint64_t version = 2011032901ULL;
    // Version history
    // 2011032901   - introduce version tracking.

    struct Header
    {
        uint64_t version;
        RRRRank::position_type size;
        RRRRank::rank_type count;

        Header()
            : version(RRRRank::version), size(0), count(0)
        {
        }

        Header(const std::string& pFileName, FileFactory& pFactory);
    };

public:
    class Builder
    {
    public:
        
        void push_back(uint64_t pPos)
        {
            uint64_t blkNum = pPos / U;
            uint64_t bit = pPos % U;
            if (blkNum != mCurrBlkNum)
            {
                flush();
                mCurrBlkNum = blkNum;
                mCurrBlk = 0;
            }
            mCurrBlk |= one << bit;
            mHeader.count++;
            mHeader.size = pPos + 1;
        }

        void end(uint64_t pN);

        Builder(const std::string& pBaseName, FileFactory& pFactory);
        Builder(const std::string& pBaseName, FileFactory& pFactory, uint64_t pUnused);

    private:
        void flush();

        const std::string mBaseName;
        FileFactory& mFactory;
        uint64_t mCurrFileBlkNum;
        uint64_t mCurrBlk;
        uint64_t mCurrBlkNum;
        uint64_t mClassSum;
        uint64_t mOffsetSum;
        Header mHeader;
        MappedArray<uint64_t>::Builder mClassSumBuilder;
        MappedArray<uint64_t>::Builder mOffsetSumBuilder;
        FixedWidthBitArray<C>::Builder mClassListBuilder;
        VariableWidthBitArray::Builder mOffsetListBuilder;
    };

    template <typename FItr, typename VItr>
    class GeneralIterator
    {
    public:

        bool valid() const
        {
            return mCurr < mSize;
        }

        uint64_t operator*() const
        {
            return mCurr;
        }

        void operator++()
        {
            mCurrBlockMask <<= 1;
            ++mCurr;
            next();
        }

        GeneralIterator(const FItr& pClassItr, 
                        const VItr& pOffsetItr, 
                        position_type pSize)
            : mCurr(0), mSize(pSize),
              mClassItr(pClassItr), 
              mOffsetItr(pOffsetItr)
        {
            loadBlock();
            next();
        }

    private:

        /**
         * Advance until mCurr indicates a set bit.
         */
        void next()
        {
            while (valid())
            {
                if (mCurrBlockMask > (1ULL << (RRRBase::U - 1)))
                {
                    // Used up the current block - get the next.
                    ++mClassItr;
                    loadBlock();
                }
                else if (mCurrBlockMask & mCurrBlock)
                {
                    // Current block bit set - return.
                    return;
                }
                else
                {
                    // Current block bit not set - advance.
                    mCurrBlockMask <<= 1;
                    ++mCurr;
                }
            }
        }

        /**
         * Read the next block into mCurrBlock, and reset mCurrBlockIx.
         */
        void loadBlock()
        {
            if (valid())
            {
                uint64_t cls = *mClassItr;
                uint64_t blkSz = RRRBase::sEnumCode.numCodeBits(cls);
                uint64_t blkOrd = mOffsetItr.get(blkSz);
                mCurrBlock = RRRBase::sEnumCode.decode(cls, blkOrd);
                mCurrBlockMask = 1ULL;
            }
        }

        uint64_t mCurr;         // The current rank
        uint64_t mSize;
        FItr mClassItr;
        VItr mOffsetItr;
        uint64_t mCurrBlock;        // The current unpacked block - word must be >= RRRBase::U bits
        uint64_t mCurrBlockMask;    // The mask into the current unpacked block.
    };

    typedef GeneralIterator<FixedWidthBitArray<RRRBase::C>::Iterator,
                            VariableWidthBitArray::Iterator>            Iterator;
    typedef GeneralIterator<FixedWidthBitArray<RRRBase::C>::LazyIterator,
                            VariableWidthBitArray::LazyIterator>        LazyIterator;

    position_type size() const
    {
        return mHeader.size;
    }

    rank_type count() const
    {
        return mHeader.count;
    }

    Iterator iterator() const
    {
        return Iterator(mClassList.iterator(),
                        mOffsetList.iterator(),
                        size());
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        Header header(pBaseName + ".header", pFactory);
        return LazyIterator(FixedWidthBitArray<RRRBase::C>::lazyIterator(pBaseName + ".classes", pFactory),
                            VariableWidthBitArray::lazyIterator(pBaseName + ".offsets", pFactory),
                            header.size);
    }

    bool access(const uint64_t& pPos) const
    {
        uint64_t blkNum = pPos / U;
        uint64_t blkOff = pPos % U;
        uint64_t prevRank;
        uint64_t blk = getBlock(blkNum, prevRank);
        return (blk & (one << blkOff)) != 0;
    }

    bool accessAndRank(const uint64_t& pPos, uint64_t& pRank) const
    {
        uint64_t blkNum = pPos / U;
        uint64_t blkOff = pPos % U;
        uint64_t prevRank;
        uint64_t blk = getBlock(blkNum, prevRank);
        pRank = prevRank + Gossamer::popcnt(blk & ((one << blkOff) - 1));
        return (blk & (one << blkOff)) != 0;
    }

    std::pair<uint64_t,rank_type> rank(const uint64_t& pLhs, const position_type& pRhs) const
    {
        std::pair<uint64_t,rank_type> retval;

        uint64_t blkNumL = pLhs / U;
        uint64_t blkNumR = pRhs / U;
        if (blkNumL != blkNumR)
        {
            retval.first = rank(pLhs);
            retval.second = rank(pRhs);
            return retval;
        }

        uint64_t blkOffL = pLhs % U;
        uint64_t blkOffR = pRhs % U;
        uint64_t prevRank;
        uint64_t blk = getBlock(blkNumL, prevRank);
        retval.first = prevRank + Gossamer::popcnt(blk & ((one << blkOffL) - 1));
        retval.second = prevRank + Gossamer::popcnt(blk & ((one << blkOffR) - 1));
        return retval;
    }

    uint64_t rank(uint64_t pPos) const
    {
        uint64_t blkNum = pPos / U;
        uint64_t blkOff = pPos % U;
        uint64_t prevRank;
        uint64_t blk = getBlock(blkNum, prevRank);
        return prevRank + Gossamer::popcnt(blk & ((one << blkOff) - 1));
    }

    PropertyTree stat() const;

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    RRRRank(const std::string& pBaseName, FileFactory& pFactory);

    void debug(uint64_t pN) const;

private:
    friend class RRRArray;

    uint64_t getBlock(const uint64_t& pBlkNum, uint64_t& pPrevRank) const;

    const Header mHeader;
    MappedArray<uint64_t> mClassSum;
    MappedArray<uint64_t> mOffsetSum;
    FixedWidthBitArray<RRRBase::C> mClassList;
    VariableWidthBitArray mOffsetList;
};


/// RRRArray with rank and select.
class RRRArray : public RRRBase
{
public:
    typedef uint64_t position_type;
    typedef uint64_t rank_type;

private:
    static const uint64_t version = 2011032902ULL;
    // Version history
    // 2011032901   - introduce version tracking.

    struct Header
    {
        uint64_t version;
        RRRArray::position_type size;
        RRRArray::rank_type count;

        Header()
            : version(RRRArray::version), size(0), count(0)
        {
        }

        Header(const std::string& pFileName, FileFactory& pFactory);
    };

public:
    class Builder
    {
    public:
        void push_back(uint64_t pPos)
        {
            mHeader.count++;
            mHeader.size = pPos + 1;
            mRankBuilder.push_back(pPos);
            uint64_t blkNum = pPos / U;
            if (mStart)
            {
                mClumpArrayBuilder.push_back(blkNum);
                mPrevBlkNum = blkNum;
                mEmptyBlks = blkNum;
                mBitNum = 1;
                // mRankPBuilder.push_back(blkNum);
                mRankQBuilder.push_back(0);
                if (blkNum != 0)
                {
                    // mRankRBuilder.push_back(0);
                }
                mStart = false;
                return;
            }

            if (blkNum != mPrevBlkNum)
            {
                // This is the first bit in a new block.
                mRankQBuilder.push_back(mBitNum);
                // mRankPBuilder.push_back(blkNum);

                // Count how many empty blocks have occurred.
                mEmptyBlks += blkNum - mPrevBlkNum - 1;

                if (mPrevBlkNum + 1 != blkNum)
                {
                    // This is the start of a new clump.  Mark it as such,
                    // and record the number of empty blocks before this.
                    mRankRBuilder.push_back(blkNum - mEmptyBlks);
                    mClumpArrayBuilder.push_back(mEmptyBlks);
                }
                mPrevBlkNum = blkNum;
            }
            ++mBitNum;
        }

        void end(uint64_t pN);

        Builder(const std::string& pBaseName, FileFactory& pFactory);
        Builder(const std::string& pBaseName, FileFactory& pFactory, uint64_t pUnused);

    private:
        const std::string mBaseName;
        FileFactory& mFactory;
        bool mStart;
        uint64_t mBitNum;
        uint64_t mPrevBlkNum;
        uint64_t mEmptyBlks;
        Header mHeader;
        RRRRank::Builder mRankBuilder;
        // RRRRank::Builder mRankPBuilder;
        RRRRank::Builder mRankQBuilder;
        RRRRank::Builder mRankRBuilder;
        MappedArray<uint64_t>::Builder mClumpArrayBuilder;
    };

    class Iterator
    {
    public:
        bool valid() const
        {
            return mRnk < mArray->count();
        }

        uint64_t operator*() const
        {
            return mArray->select(mRnk);
        }

        void operator++()
        {
            BOOST_ASSERT(valid());
            ++mRnk;
        }

        Iterator(const RRRArray& pArray)
            : mArray(&pArray), mRnk(0)
        {
        }

    private:
        const RRRArray* mArray;
        uint64_t mRnk;
    };

    position_type size() const
    {
        return mHeader.size;
    }

    rank_type count() const
    {
        return mHeader.count;
    }

    Iterator iterate() const
    {
        return Iterator(*this);
    }

    bool access(uint64_t pPos) const
    {
        return mRank.access(pPos);
    }

    bool accessAndRank(uint64_t pPos, uint64_t& pRank) const
    {
        return mRank.accessAndRank(pPos, pRank);
    }

    std::pair<uint64_t,uint64_t> rank(uint64_t pLhs, uint64_t pRhs) const
    {
        return mRank.rank(pLhs, pRhs);
    }

    uint64_t rank(uint64_t pPos) const
    {
        return mRank.rank(pPos);
    }

    uint64_t select(uint64_t pRnk) const
    {
        // std::cerr << "select(" << pRnk << ")\n";
        uint64_t blockRank = mRankQ.rank(pRnk + 1);
        // std::cerr << "  blockRank = " << blockRank << "\n";
        uint64_t clump = mRankR.rank(blockRank);
        // std::cerr << "  clump = " << clump << "\n";
        uint64_t selectP = mClumpArray[clump] + blockRank - 1;
        // std::cerr << "  selectP = " << selectP << "\n";
        uint64_t prevRank;
        uint64_t blk = mRank.getBlock(selectP, prevRank);

        pRnk -= prevRank;
        while (pRnk > 0)
        {
            blk &= ~(blk & (-blk));
            // BOOST_ASSERT(blk != 0);
            --pRnk;
        }

        return selectP * U + Gossamer::find_first_set(blk) - 1;
    }

    PropertyTree stat() const;

    RRRArray(const std::string& pBaseName, FileFactory& pFactory);

    void debug(uint64_t pN) const;

private:
    const Header mHeader;
    RRRRank mRank;
    // RRRRank mRankP;
    RRRRank mRankQ;
    RRRRank mRankR;
    MappedArray<uint64_t> mClumpArray;
};


#endif // RRRARRAY_HH
