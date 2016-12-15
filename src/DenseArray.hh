// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef DENSEARRAY_HH
#define DENSEARRAY_HH

#ifndef STD_LIMITS
#include <limits>
#define STD_LIMITS
#endif

#ifndef STD_BITSET
#include <bitset>
#define STD_BITSET
#endif

#ifndef BOOST_STATIC_ASSERT_HPP
#include <boost/static_assert.hpp>
#define BOOST_STATIC_ASSERT_HPP
#endif

#ifndef BOOST_DYNAMIC_BITSET_HPP
#include <boost/dynamic_bitset.hpp>
#define BOOST_DYNAMIC_BITSET_HPP
#endif

#ifndef WORDYBITVECTOR_HH
#include "WordyBitVector.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif


class DenseIndexBuilderBase
{
protected:
    DenseIndexBuilderBase(const std::string& pBaseName, FileFactory& pFactory);

    void alignFilePos(uint64_t pMask);

    const std::string mBaseName;

    FileFactory::OutHolderPtr mFileHolder;
    std::ostream& mFile;
};


class DenseSelect
{
public:
    static constexpr uint64_t version = 2012092701ULL;
    // 2012092701L   - two-level indexes

    typedef uint16_t internal_pointer_t;

    // This is mostly to ensure that radix == 2, but it doesn't
    // hurt to sanity check everything else.
    BOOST_STATIC_ASSERT(
        (std::numeric_limits<internal_pointer_t>::radix == 2)
        && (std::numeric_limits<internal_pointer_t>::digits >= 16)
        && std::numeric_limits<internal_pointer_t>::is_specialized
        && std::numeric_limits<internal_pointer_t>::is_integer
        && !std::numeric_limits<internal_pointer_t>::is_signed);

    // A block is dense if the range between the first 1 and the last 1
    // is less than DENSEBLOCK.
    static const uint64_t sLogSmallBlock = 16;
    static const uint64_t sSmallBlock = (1ull << sLogSmallBlock);

    // A block is dense if the range between the first 1 and the last 1
    // is less than DENSEBLOCK.
    static const uint64_t sLogIntermediateBlock = 24;
    static const uint64_t sIntermediateBlock = (1ull << sLogIntermediateBlock);

    // The number of 1's per block.
    static const uint64_t sLogDefBlockSize = 13;
    static const uint64_t sDefBlockSize = (1ull << sLogDefBlockSize);

    // The gap between samples.
    static const uint64_t sLogDefSampleRate = 6;
    static const uint64_t sDefSampleRate = (1ull << sLogDefSampleRate);

    struct Header
    {
        uint64_t version;

        // Flags from kFirstUnusedFlag to kLastFlag-1 are reserved and
        // MUST be zero.  If any are 1, the index will not be loaded.
        enum {
            kInvertSense = 0,
            kFirstUnusedFlag = 1,
            kLastFlag = 64
        };

        std::bitset<kLastFlag> flags;

        uint64_t indexArrayOffset;
        uint64_t rankArrayOffset;

        // The tuning constants
        uint64_t logBlockSize; // Log of number of 1's per block
        uint64_t blockSize;
        uint64_t logSampleRate; // Log of gap between samples.
        uint64_t sampleRate;

        // Statistics
        uint64_t numBlocks; // Number of entries in the master index,
                            // which should be close to count() /  blockSize.
                            // It should also be equal to
                            // smallBlocks + intermediateBlocks + largeBlocks.
        uint64_t indexSize; // Size of master index, in bytes.
        uint64_t smallBlocks; // Number of small blocks.
        uint64_t smallBlocksSize; // Size of small blocks, in bytes.
        uint64_t intermediateBlocks; // Number of intermediate blocks.
        uint64_t intermediateBlocksSize; // Size of intermediate blocks, in bytes.
        uint64_t largeBlocks; // Number of large blocks.
        uint64_t largeBlocksSize; // Size of large blocks, in bytes.

        Header();
        Header(const bool sInvert);
    };


    class Builder : public DenseIndexBuilderBase
    {
    public:
        // pBitPos referes to an indexed position, not necessarily a 1.
        //
        void push_back(uint64_t pBitPos)
        {
            mCurrBlock.push_back(pBitPos);
            if (mCurrBlock.size() == mHeader.blockSize)
            {
                flush();
            }
        }

        void end();

        Builder(const std::string& pBaseName, FileFactory& pFactory,
                bool pInvertSense);

    private:
        void flush();

        Header mHeader;

        std::vector<uint64_t> mCurrBlock;
        std::vector<uint64_t> mIndex;
        std::vector<uint64_t> mRank;
        std::vector<uint64_t> mSubRankStart;
        std::vector<uint64_t> mSubBlockRange;
        std::vector<uint16_t> mInternalPtr;
    };

    PropertyTree stat() const;

    bool access(uint64_t pPos) const
    {
        bool bit = mBitVector.get(pPos);
        return mHeader.flags[Header::kInvertSense] ? !bit : bit;
    }

    uint64_t select(uint64_t i) const;

    std::pair<uint64_t,uint64_t> select(uint64_t i, uint64_t j) const;

    DenseSelect(const WordyBitVector& pBitVector,
                const std::string& pBaseName, FileFactory& pFactory,
                bool pInvertSense);

protected:
    enum block_type_t {
        tSmall = 0,
        tFullSpill64 = 1,
        tFullSpill32 = 2,
        tFullSpill16 = 3,
        tFullSpill8 = 4,
        tIntermediate = 5
    };
    static const uint64_t sBlockTypeMask = 0x7;

    uint64_t lookupSubBlock(const uint8_t* pBlockStart, uint64_t pStartRank,
                            internal_pointer_t pSubBlock, uint64_t pI) const;

    const WordyBitVector& mBitVector;
    FileFactory::MappedHolderPtr mFileHolder;
    Header mHeader;
    const uint8_t* mData;
    const uint64_t* mIndex;
    const uint64_t* mRank;
};


class DenseRank
{
public:
    static const uint64_t version = 2011071201ULL;
    // Version history
    // 2011071201L   - first version

    typedef uint16_t small_offset_t;

    // This is mostly to ensure that radix == 2, but it doesn't
    // hurt to sanity check everything else.
    BOOST_STATIC_ASSERT(
        (std::numeric_limits<small_offset_t>::radix == 2)
        && (std::numeric_limits<small_offset_t>::digits >= 16)
        && std::numeric_limits<small_offset_t>::is_specialized
        && std::numeric_limits<small_offset_t>::is_integer
        && !std::numeric_limits<small_offset_t>::is_signed);

    static const uint64_t sLogSmallBlockSize = 8;
    static const uint64_t sSmallBlockSize = (1ull << sLogSmallBlockSize);

    static const uint64_t sLogLargeBlockSize = 15;
    static const uint64_t sLargeBlockSize = (1ull << sLogLargeBlockSize);

    struct Header
    {
        uint64_t version;
        uint64_t size;
        uint64_t count;
        uint64_t largeBlockArrayOffset;
        uint64_t smallBlockArrayOffset;

        Header();

        Header(const std::string& pFileName, FileFactory& pFactory);
    };

    class Builder : public DenseIndexBuilderBase
    {
    public:
        void push_back(uint64_t pBitPos)
        {
            while ((pBitPos >> sLogLargeBlockSize) != mCurrLargeBlock)
            {
                flush();
            }
            uint64_t smallBlockOffset
                = (pBitPos & (sLargeBlockSize - 1)) >> sLogSmallBlockSize;
            ++mSmallBlockArray[smallBlockOffset];
        }

        void end(uint64_t pEndPos);

        Builder(const std::string& pBaseName, FileFactory& pFactory);

    private:
        void flush();

        Header mHeader;

        uint64_t mLastLargeBlockRank;
        uint64_t mCurrLargeBlock;
        std::vector<uint16_t> mSmallBlockArray;
        std::vector<uint64_t> mLargeBlockArray;
    };

    uint64_t count() const
    {
        const uint64_t init = (mBitVector.words() - 1) * mBitVector.wordBits;
        const uint64_t end = init + mBitVector.wordBits;
        return rank(init) + mBitVector.popcountRange(init, end);
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        uint64_t s = sizeof(Header);
        s += mFileHolder->size();
        t.putProp("storage", s);
        return t;
    }

    uint64_t rank(uint64_t p) const
    {
        uint64_t smallBlockNum = p / sSmallBlockSize;
        uint64_t largeBlockNum = p / sLargeBlockSize;
        return mLargeBlockArray[largeBlockNum]
             + mSmallBlockArray[smallBlockNum]
             + mBitVector.popcountRange(smallBlockNum * sSmallBlockSize, p);
    }

    uint64_t rank1(uint64_t p) const
    {
        return rank(p);
    }

    uint64_t rank0(uint64_t p) const
    {
        return p - rank(p);
    }

    std::pair<uint64_t,uint64_t> rank(uint64_t pPos1, uint64_t pPos2) const
    {
        BOOST_ASSERT(pPos1 <= pPos2);

        uint64_t r1 = rank(pPos1);
        uint64_t r2;
        if (pPos2 - pPos1 > sSmallBlockSize*2)
        {
            r2 = rank(pPos2);
        }
        else
        {
            r2 = r1 + mBitVector.popcountRange(pPos1, pPos2);
        }
        return std::make_pair(r1, r2);
    }

    uint64_t countRange(uint64_t pPos1, uint64_t pPos2) const
    {
        BOOST_ASSERT(pPos1 <= pPos2);

        uint64_t r1 = rank(pPos1);
        if (pPos2 - pPos1 > sSmallBlockSize*2)
        {
            return rank(pPos2) - r1;
        }
        else
        {
            return mBitVector.popcountRange(pPos1, pPos2);
        }
    }

    bool access(uint64_t pPos) const
    {
        return mBitVector.get(pPos);
    }

    bool accessAndRank(uint64_t pPos, uint64_t& pRank) const
    {
        pRank = rank(pPos);
        return access(pPos);
    }

    DenseRank(const WordyBitVector& pBitVector,
              const std::string& pBaseName, FileFactory& pFactory);

protected:
    const WordyBitVector& mBitVector;
    FileFactory::MappedHolderPtr mFileHolder;
    Header mHeader;
    const uint8_t* mData;
    const uint16_t* mSmallBlockArray;
    const uint64_t* mLargeBlockArray;
};


class DenseArray
{
public:
    class Builder;
    friend class Builder;
    class Iterator;
    friend class Iterator;

    typedef uint64_t position_type;
    typedef uint64_t rank_type;

private:
    static const uint64_t version = 2011101401ULL;
    // Version history
    // 2011101401ULL   - first version

    struct Header
    {   
        uint64_t version;
        DenseArray::position_type size;
        DenseArray::rank_type count;

        Header();
        Header(const std::string& pFileName, FileFactory& pFactory);
    };

public:
    class Builder
    {
    public:
        typedef DenseArray::rank_type position_type;
        typedef DenseArray::rank_type rank_type;
        typedef bool value_type;

        void push_back(uint64_t pPos)
        {
            mBitsFile.push(pPos);
            mSelectIndexFile.push_back(pPos);
            mRankIndexFile.push_back(pPos);
            ++mHeader.count;
        }

        void end(position_type pN);

        Builder(const std::string& pBaseName, FileFactory& pFactory);

    private:
        Header mHeader;
        WordyBitVector::Builder mBitsFile;
        DenseSelect::Builder mSelectIndexFile;
        DenseRank::Builder mRankIndexFile;
        FileFactory::OutHolderPtr mHeaderFileHolder;
        std::ostream& mHeaderFile;
    };

    class Iterator : private WordyBitVector::Iterator1
    {
        friend class DenseArray;

    public:
        typedef DenseArray::position_type position_type;
        typedef DenseArray::rank_type rank_type;
 
        using WordyBitVector::Iterator1::valid;
        using WordyBitVector::Iterator1::operator*;
        using WordyBitVector::Iterator1::operator++;

        Iterator(const DenseArray& pArray)
            : WordyBitVector::Iterator1(pArray.mBits.iterator1())
        {
        }
    };

    class LazyIterator : private WordyBitVector::LazyIterator1
    {
        friend class DenseArray;

    public:
        typedef DenseArray::position_type position_type;
        typedef DenseArray::rank_type rank_type;
 
        using WordyBitVector::LazyIterator1::valid;
        using WordyBitVector::LazyIterator1::operator*;
        using WordyBitVector::LazyIterator1::operator++;

        LazyIterator(const std::string& pBaseName, FileFactory& pFactory);
    };

    Iterator iterator() const
    {
        return Iterator(*this);
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        return LazyIterator(pBaseName, pFactory);
    }

    const position_type& size() const
    {
        return mHeader.size;
    }

    const rank_type& count() const
    {
        return mHeader.count;
    }

    bool access(position_type pPos) const
    {
        return mBits.get(pPos);
    }

    bool accessAndRank(position_type pPos, rank_type& pRank) const
    {
        return mRankIndex.accessAndRank(pPos, pRank);
    }

    std::pair<rank_type,rank_type> rank(position_type pLhs, position_type pRhs) const
    {
        return mRankIndex.rank(pLhs, pRhs);
    }

    rank_type rank(position_type pPos) const
    {
        return mRankIndex.rank(pPos);
    }

    rank_type countRange(position_type pPos1, position_type pPos2) const
    {
        return mRankIndex.countRange(pPos1, pPos2);
    }

    position_type select(rank_type pRnk) const
    {
        return mSelectIndex.select(pRnk);
    }

    std::pair<position_type,position_type> select(rank_type pLhs, rank_type pRhs) const
    {
        return mSelectIndex.select(pLhs, pRhs);
    }

    PropertyTree stat() const;

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    DenseArray(const std::string& pBaseName, FileFactory& pFactory);

    ~DenseArray();

protected:
    Header mHeader;
    const WordyBitVector mBits;
    const DenseSelect mSelectIndex;
    const DenseRank mRankIndex;
};

#endif // DENSEARRAY_HH
