// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SPARSEARRAY_HH
#define SPARSEARRAY_HH

#ifndef RANKSELECT_HH
#include "RankSelect.hh"
#endif

#ifndef DENSEARRAY_HH
#include "DenseArray.hh"
#endif

#ifndef INTEGERARRAY_HH
#include "IntegerArray.hh"
#endif

#ifndef STD_MATH_H
#include <math.h>
#define STD_MATH_H
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef BOOST_NUMERIC_CONVERSION_CAST_HPP
#include <boost/numeric/conversion/cast.hpp>
#define BOOST_NUMERIC_CONVERSION_CAST_HPP
#endif

class SparseArray
{
public:
    class Builder;
    friend class Builder;

    class Iterator;
    friend class Iterator;

    typedef Gossamer::position_type  position_type; // The type of a bit position in the virtual bitmap
    typedef Gossamer::rank_type  rank_type; // The type of a bit count.

private:
    static const uint64_t version = 2012030501ULL;
    // Version history
    // 2010090801   - introduce version tracking.
    // 2012030501   - don't quantize d.

    struct Header
    {
        uint64_t version;
        uint64_t D;
        uint64_t quantizedD;
        SparseArray::position_type DMask;
        SparseArray::position_type size;
        SparseArray::rank_type count;

        Header(uint64_t pD);

        Header(const std::string& pFileName, FileFactory& pFactory);
    };

public:
    class Builder
    {
    public:
        typedef SparseArray::position_type position_type;
        typedef SparseArray::rank_type rank_type;
        typedef position_type value_type;

        /**
         * \todo Add operator >> to position_type and throw exception if h is not uint64_t.
         * Note that for large values of k we might need to restrict the values of D so that h satisfies this criterion.
         * \todo Add operator ++ to rank_type.
         */
        void push_back(const position_type& pBitPos)
        {
            using namespace Gossamer;

            position_type nd = pBitPos >> mHeader.D;
            if (!nd.fitsIn64Bits())
            {
                throw "SparseArray::end()";
            }

            rank_type h(nd.asUInt64());
            h += mBitNum;
            ++mBitNum;

            mHighBitsFile.push(h);

            while (mLastHighBit < h)
            {
                mD0File.push_back(mLastHighBit);
                ++mLastHighBit;
            }
            mD1File.push_back(h);

            mLastHighBit = ++h;

            IntegerArray::value_type l = (pBitPos & mHeader.DMask).value();
            mLowBitsFile.push_back(l);

            BOOST_ASSERT(pBitPos >= mHeader.size);
            mHeader.size = pBitPos + 1;
            ++mHeader.count;
        }

        void end(const position_type& pN);

        Builder(const std::string& pBaseName, FileFactory& pFactory, const position_type& pN, rank_type pM);

        Builder(const std::string& pBaseName, FileFactory& pFactory, uint64_t pD);

    private:
        static uint64_t d(const position_type& pN, rank_type pM);

        Header mHeader;
        rank_type mBitNum;
        rank_type mLastHighBit;
        WordyBitVector::Builder mHighBitsFile;
        DenseSelect::Builder mD0File;
        DenseSelect::Builder mD1File;
        IntegerArray::BuilderPtr mLowBitsFileHolder;
        IntegerArray::Builder& mLowBitsFile;
        FileFactory::OutHolderPtr mHeaderFileHolder;
        std::ostream& mHeaderFile;
    };

    class Iterator
    {
        friend class SparseArray;

    public:
        typedef SparseArray::position_type position_type;
        typedef SparseArray::rank_type rank_type;

        bool valid() const
        {
            return mValid;
        }

        position_type operator*() const
        {
            position_type pos(*mHiItr - mI);
            pos <<= mArray->mHeader.D;
            pos += position_type(mArray->mLowBits[mI]);
            return pos;
        }

        void operator++()
        {
            BOOST_ASSERT(mHiItr.valid());
            BOOST_ASSERT(valid());
            ++mHiItr;
            if (++mI == mArray->count())
            {
                BOOST_ASSERT(!mHiItr.valid());
                mValid = false;
                return;
            }
        }

    private:
        const SparseArray* mArray;
        WordyBitVector::Iterator1 mHiItr;
        rank_type mI;
        bool mValid;

        Iterator(const SparseArray& pArray);
    };

    // TODO: Consolidate with Iterator
    class LazyIterator
    {
    public:
        bool valid() const
        {
            BOOST_ASSERT(mHiItr.valid() == mLowBitsIterator.valid());
            return mHiItr.valid();
        }

        position_type operator*() const
        {
            position_type pos(*mHiItr - mI);
            pos <<= mHeader.D;
            pos += position_type(*mLowBitsIterator);
            return pos;
        }

        void operator++()
        {
            BOOST_ASSERT(mHiItr.valid());
            BOOST_ASSERT(mLowBitsIterator.valid());
            BOOST_ASSERT(valid());
            ++mHiItr;
            ++mLowBitsIterator;
            ++mI;
        }

        LazyIterator(const std::string& pBaseName, FileFactory& pFactory);

    private:

        LazyIterator& operator=(const LazyIterator&);

        Header mHeader;

        WordyBitVector::LazyIterator1 mHiItr;
        IntegerArray::LazyIteratorPtr mLowBitsIteratorHolder;
        IntegerArray::LazyIterator& mLowBitsIterator;
        rank_type mI;
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

    rank_type count() const
    {
        return mHeader.count;
    }

    bool access(const position_type& pPos) const
    {
        uint64_t posD = (pPos >> mHeader.D).asUInt64();

        std::pair<uint64_t,uint64_t> xrange = findLowOrderGroup(posD);
        BOOST_ASSERT(xrange.second <= mLowBits.size());

        position_type j = pPos & mHeader.DMask;
        uint64_t rank = searchLowBits(xrange.first, xrange.second, j);
        if (rank >= xrange.second)
        {
            return false;
        }
        return position_type(mLowBits[rank]) == j;
    }

    bool accessAndRank(const position_type& pPos, rank_type& pRank) const
    {
        uint64_t posD = (pPos >> mHeader.D).asUInt64();

        std::pair<uint64_t,uint64_t> xrange = findLowOrderGroup(posD);
        BOOST_ASSERT(xrange.second <= mLowBits.size());

        position_type j = pPos & mHeader.DMask;
        pRank = searchLowBits(xrange.first, xrange.second, j);
        if (pRank >= xrange.second)
        {
            return false;
        }
        return position_type(mLowBits[pRank]) == j;
    }

    std::pair<rank_type,rank_type> rank(const position_type& pLhs, const position_type& pRhs) const
    {
        std::pair<rank_type,rank_type> retval;

        BOOST_ASSERT(pLhs <= pRhs);

        uint64_t posDlhs = (pLhs >> mHeader.D).asUInt64();
        uint64_t posDrhs = (pRhs >> mHeader.D).asUInt64();

        if (posDlhs != posDrhs)
        {
            retval.first = rank(pLhs);
            retval.second = rank(pRhs);
            return retval;
        }

        std::pair<uint64_t,uint64_t> xrange = findLowOrderGroup(posDlhs);
        BOOST_ASSERT(xrange.second <= mLowBits.size());

        uint64_t firstRank = searchLowBits(xrange.first, xrange.second, pLhs & mHeader.DMask);
        retval.first = firstRank;
        uint64_t secondRank = searchLowBits(firstRank, xrange.second, pRhs & mHeader.DMask);
        retval.second = secondRank;

        return retval;
    }

    rank_type rank(const position_type& pPos) const
    {
        if (pPos >= mHeader.size)
        {
            return mHeader.count;
        }

        uint64_t posD = (pPos >> mHeader.D).asUInt64();

        std::pair<uint64_t,uint64_t> xrange = findLowOrderGroup(posD);
        BOOST_ASSERT(xrange.second <= mLowBits.size());

        position_type j = pPos & mHeader.DMask;
        return searchLowBits(xrange.first, xrange.second, j);
    }

    position_type select(rank_type pRnk) const
    {
        BOOST_ASSERT(pRnk < mHeader.count);

        position_type pos(0);
        if (mHeader.D < position_type::value_type::sBits)
        {
            pos |= mD1.select(pRnk);
            pos -= pRnk;
            pos <<= mHeader.D;
        }
        pos |= position_type(mLowBits[pRnk]);
        return pos;
    }

    PropertyTree stat() const;

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    SparseArray(const std::string& pBaseName, FileFactory& pFactory);

    ~SparseArray();

private:
    std::pair<uint64_t,uint64_t> findLowOrderGroup(uint64_t posD) const
    {
        if (mHeader.D >= position_type::value_type::sBits)
        {
            return std::make_pair(0, mLowBits.size());
        }
        else if (!posD)
        {
            return std::make_pair(0, mD0.select(0));
        }
        else
        {
            std::pair<uint64_t,uint64_t> ranks = mD0.select(posD - 1, posD);
            ranks.first += 1;
            return std::make_pair(
                ranks.first >= posD ? ranks.first - posD : 0,
                ranks.second >= posD ? ranks.second - posD : 0
            );
        }
    }

    uint64_t searchLowBits(uint64_t pBegin, uint64_t pEnd, const position_type& pValue) const
    {
        return mLowBitsHolder->lower_bound(pBegin, pEnd, pValue.value());
    }

    Header mHeader;
    const WordyBitVector mHighBits;
    const DenseSelect mD0;
    const DenseSelect mD1;
    IntegerArrayPtr mLowBitsHolder;
    const IntegerArray& mLowBits;
};

#endif // SPARSEARRAY_HH
