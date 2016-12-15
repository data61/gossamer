// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BACKYARDHASH_HH
#define BACKYARDHASH_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_UNORDERED_MAP
#include <unordered_map>
#define STD_UNORDERED_MAP
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef BOOST_ATOMIC_HPP
#include <boost/atomic.hpp>
#define BOOST_ATOMIC_HPP
#endif

#ifndef GOSSAMER_HH
#include "Gossamer.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef SPINLOCK_HH
#include "Spinlock.hh"
#endif

/**
 * The BackyardHash is a hash table which uses ideas from succinct data structures
 * to store things compactly. It supports concurrent inserts, subject to the condition
 * that where two threads attempt to insert the same key, not already present in the
 * hash table, a race condition occurs which means that the key may end up in the table
 * twice. The preconditions for this race condition are such that in practice, the
 * probability of this occurring is vanishingly small, but for correctness, when using
 * the visit() method, the visitor object (or downstream processing) should allow for the
 * posibility of duplicates.
 *
 * To balance the chance of lock contention with space usage, locks are shared between slots.
 * The number of slots is a runtime choice, but the number of locks is fixed with a compile-time
 * constant L.
 *
 * Each slot has the following layout:
 *
 *      vvv...vvvccc...cccjj
 *
 * vvv...vvv    the bits for the stored part of the key.
 * ccc...ccc    the bits for the count.
 * jj           the two bit quantity for the hash function number.
 *
 * The number of v bits and c bits are as follows:
 *  #v bits = #item bits - #slot bits
 *  #c bits = 8 * sizeof(value_type) - #v bits - #j bits
 */
class BackyardHash
{
public:
    /// J is the number of hash functions to use during cuckoo hashing.
    static const uint64_t J = 4;

    /// S is number of attempts to do a cuckoo exchange before giving
    /// up and using the spill table.
    static const uint64_t S = 16 * J;

    /// L governs the number of locks to use: the number of locks is 2^L
    /// irrespective of the number of hash slots.
    static const uint64_t L = 16;

    typedef Gossamer::edge_type value_type;

    /**
     * A Hash object contains the result of the invertable hash:
     * a slot number and the value to store in the slot.
     */
    class Hash
    {
    public:
        uint64_t slot() const
        {
            return mSlot;
        }

        const value_type& value() const
        {
            return mValue;
        }

        Hash(uint64_t pSlot, const value_type& pValue)
            : mSlot(pSlot), mValue(pValue)
        {
        }

    private:
        uint64_t mSlot;
        value_type mValue;
    };

    /**
     * A PartialHash object contains the intermediate results for
     * invertable hashing. The full hash is computed by partitioning
     * the key in to two parts - high bits and low bits. The high
     * bits are hashed with a thorough, and relatively expensive
     * hash function. The J different hash functions are computed by
     * applying cheap universal hash functions (a * x + b) to the
     * intermediate hash. This approach amortizes the cost of the
     * thorough hash function across the J different functions.
     */
    class PartialHash
    {
    public:
        uint64_t slot() const
        {
            return mSlot;
        }

        uint64_t hash() const
        {
            return mHash;
        }

        const value_type& value() const
        {
            return mValue;
        }


        PartialHash(uint64_t pSlot, uint64_t pHash, const value_type& pValue)
            : mSlot(pSlot), mHash(pHash), mValue(pValue)
        {
        }

    private:
        uint64_t mSlot;
        uint64_t mHash;
        value_type mValue;
    };

    /**
     * Content is a wrapper object for the contents of a slot.
     */
    class Content
    {
    public:
        /**
         * The hash function number.
         */
        const uint64_t& hash() const
        {
            return mHash;
        }

        /**
         * The frequency of the item.
         */
        const uint64_t& count() const
        {
            return mCount;
        }

        /**
         * The high bits of the item.
         */
        const value_type& value() const
        {
            return mValue;
        }

        Content(uint64_t pHash, uint64_t pCount, const value_type& pValue)
            : mHash(pHash), mCount(pCount), mValue(pValue)
        {
        }

    private:
        const uint64_t mHash;
        const uint64_t mCount;
        const value_type mValue;
    };

    /**
     * The number of entries in the hash table.
     */
    uint64_t size() const
    {
        return mSize;
    }

    /**
     * The number of slots in the hash table.
     */
    uint64_t capacity() const
    {
        return mItems.size();
    }

    /**
     * The number of entries that have spilled from the main hash table.
     */
    uint64_t spills()
    {
        SpinlockHolder lk(mOtherMutex);
        return mOther.size();
    }

    /**
     * Retrieve an item from the hash table. pIdx is not a *rank*;
     * it is merely a key.
     */
    std::pair<value_type,uint64_t> operator[](uint32_t pIdx) const
    {
        if (pIdx < mItems.size())
        {
            Content x = unpack(mItems[pIdx]);
            value_type k = unhash(pIdx & mSlotMask, x.hash(), x.value());
            uint64_t c = x.count();
            return std::pair<value_type,uint64_t>(k,c);
        }

        pIdx -= mItems.size();
        BOOST_ASSERT(mOtherIndex.size() == mOther.size());
        return mOtherIndex[pIdx];
    }

    /**
     * Return a radix key for the indexed item.
     * (radix(a) != radix(b)) => a != b.
     * (radix(a) == radix(b)) !=> a == b.
     * It returns the most significant bits of the "value"
     * part of the slot contents, selecting at most 64 bits.
     */
    uint64_t radix64(uint32_t pIdx) const
    {
        if (pIdx < mItems.size())
        {
            return mItems[pIdx].mostSigWord();
        }

        pIdx -= mItems.size();
        BOOST_ASSERT(mOtherIndex.size() == mOther.size());
        value_type v = valueBits(mOtherIndex[pIdx].first);
        v <<= mCountBits + mHashNumBits;
        return v.mostSigWord();
    }

    /**
     * Return a radix key for the indexed item.
     * (radix(a) != radix(b)) => a != b.
     * (radix(a) == radix(b)) !=> a == b.
     * It returns the most significant bits of the "value"
     * part of the slot contents, selecting at most 64 bits.
     */
    template <int Shift>
    uint64_t radix(uint32_t pIdx) const
    {
        return radix64(pIdx) >> Shift;
    }

    uint64_t count(const value_type& pItem) const
    {
        PartialHash p = partialHash(pItem);
        uint64_t c = 0;
        for (uint64_t j = 0; j < J; ++j)
        {
            Hash h = hash(p, j);
            uint64_t s0 = h.slot();
            uint64_t s = s0;
            while (s < mItems.size())
            {
                Content x = unpack(mItems[s]);
                if (x.hash() == j && unhash(s0, j, x.value()) == pItem && x.count() > 0)
                {
                    c += x.count();
                }
                s += (1ULL << mSlotBits);
            }
        }
        std::unordered_map<value_type,uint64_t>::const_iterator i = mOther.find(pItem);
        if (i != mOther.end())
        {
            c += i->second;
        }
        return c;
    }

    void insert(const value_type& pItem);

    void sort(std::vector<uint32_t>& pPerm, uint64_t pNumThreads) const;

    template <typename Vis>
    void visit(Vis& pVisitor) const
    {
        uint32_t i = 0;
        for (; i < mItems.size(); ++i)
        {
            uint64_t c = getCount(i);
            if (c == 0)
            {
                continue;
            }
            Content x = unpack(mItems[i]);
            value_type k = unhash(i & mSlotMask, x.hash(), x.value());
            pVisitor(i, k, c);
        }
        BOOST_ASSERT(mOtherIndex.size() == mOther.size());
        for (uint64_t j = 0; j < mOtherIndex.size(); ++j, ++i)
        {
            pVisitor(i, mOtherIndex[j].first, mOtherIndex[j].second);
        }
    }

    template <typename Vis>
    void visit0(Vis& pVisitor) const
    {
        uint32_t i = 0;
        for (; i < mItems.size(); ++i)
        {
            uint64_t c = getCount(i);
            if (c == 0)
            {
                continue;
            }
            pVisitor(i);
        }
        BOOST_ASSERT(mOtherIndex.size() == mOther.size());
        for (uint64_t j = 0; j < mOtherIndex.size(); ++j, ++i)
        {
            pVisitor(i);
        }
    }

    bool less(uint32_t pLhs, uint32_t pRhs) const
    {
        if (pLhs < mItems.size() && pRhs < mItems.size())
        {
            Content l = unpack(mItems[pLhs]);
            Content r = unpack(mItems[pRhs]);
            if (l.value() < r.value())
            {
                return true;
            }
            if (l.value() > r.value())
            {
                return false;
            }
            value_type x = unhash(pLhs & mSlotMask, l.hash(), l.value());
            value_type y = unhash(pRhs & mSlotMask, r.hash(), r.value());
            return x < y;
        }
        return (*this)[pLhs] < (*this)[pRhs];
    }

    void clear()
    {
        for (uint64_t i = 0; i < mItems.size(); ++i)
        {
            mItems[i] = value_type(0);
        }
        mOther.clear();
        mOtherIndex.clear();
        mSize = 0;
        mSpills = 0;
        mPanics = 0;
    }

    void index() const
    {
        mOtherIndex.clear();
        for (auto& j : mOther) {
            mOtherIndex.push_back(j);
        }
    }

    PropertyTree stat() const
    {
        double d = static_cast<double>(mSize) / mItems.size();
        PropertyTree t;
        t.putProp("cuckoo-size", mSize);
        t.putProp("cuckoo-load", d);
        t.putProp("spill-items", mOther.size());
        t.putProp("count-spills", mSpills);
        t.putProp("panic-spills", mPanics);
        return t;
    }

    /**
     * The approximate number of bytes required to hold a table with the given
     * number of slot bits, plus two permutation vectors.
     */
    static uint64_t estimatedSize(uint64_t pSlotBits)
    {
        uint64_t slots = 1ULL << pSlotBits;
        uint64_t b = slots * (1.5 * sizeof(uint32_t) + sizeof(value_type));
        return b;
    }

    /**
     * The maximum number of slot bits available in a table (plus space for
     * two permutation vectors) not exceeding the given number of bytes.
     */
    static uint64_t maxSlotBits(uint64_t pBufferSize)
    {
        uint64_t slots = pBufferSize / (1.5 * sizeof(uint32_t) + sizeof(value_type));
        return log2((double)slots);
    }

    BackyardHash(uint64_t pSlotBits, uint64_t pItemBits, uint64_t pNumSlots);

    ~BackyardHash()
    {
    }

private:
    PartialHash partialHash(const value_type& pKey) const
    {
        uint64_t s0 = slotBits(pKey);
        value_type v = valueBits(pKey);
        uint64_t h = hash0(v);
        return PartialHash(s0, h, v);
    }

    Hash hash(const PartialHash& pHash, uint64_t pJ) const
    {
        uint64_t s0 = pHash.slot();
        value_type v = pHash.value();
        uint64_t h = univ(pJ, pHash.hash()) & mSlotMask;
        uint64_t s = s0 ^ h;
        return Hash(s, v);
    }

    value_type unhash(uint64_t pSlot, uint64_t pHash, value_type pValue) const
    {
        uint64_t h = univ(pHash, hash0(pValue)) & mSlotMask;
        uint64_t s0 = pSlot ^ h;
        return unSlotBits(s0) | unValueBits(pValue);
    }

    uint64_t slotBits(const value_type& pKey) const
    {
        return pKey.asUInt64() & mSlotMask;
    }

    value_type valueBits(const value_type& pKey) const
    {
        return pKey >> mSlotBits;
    }

    value_type unSlotBits(uint64_t pSlot) const
    {
        return value_type(pSlot);
    }

    value_type unValueBits(const value_type& pValue) const
    {
        return pValue << mSlotBits;
    }

    value_type pack(uint64_t pHashNum, uint64_t pCount, const value_type& pValue) const
    {
        value_type x = pValue;
        x <<= mCountBits;
        x |= value_type(pCount);
        x <<= mHashNumBits;
        x |= value_type(pHashNum);
        return x;
    }

    Content unpack(const value_type& pContent) const
    {
        value_type x = pContent;
        uint64_t j = x.asUInt64() & mHashNumMask;
        x >>= mHashNumBits;
        uint64_t c = x.asUInt64() & mCountMask;
        x >>= mCountBits;
        value_type v = x;
        return Content(j, c, v);
    }

    uint64_t getCount(const uint64_t& pSlot) const
    {
        if (mHashNumBits + mCountBits <= 64)
        {
            uint64_t x = mItems[pSlot].asUInt64();
            x >>= mHashNumBits;
            return x & mCountMask;
        }
        value_type x = mItems[pSlot];
        x >>= mHashNumBits;
        return x.asUInt64() & mCountMask;
    }

    static uint64_t hash0(const value_type& pKey)
    {
        std::pair<const uint64_t*,const uint64_t*> v = pKey.words();
        const uint64_t* b = v.first;
        const uint64_t* e = v.second;
        uint64_t n = e - b;

        uint64_t h = 0x6931471805599453ULL;
        uint64_t x = 0x9e3779b97f4a7c13ULL;
        uint64_t y = 0x1415926535897932ULL;
        while (n >= 3)
        {
            h += *b++;
            --n;
            x += *b++;
            --n;
            y += *b++;
            --n;
            mix64(x, y, h);
        }
        switch (n)
        {
            case 2:
            {
                h += *b++;
                --n;
                // fall through!
            }
            case 1:
            {
                x += *b++;
                --n;
                mix64(x, y, h);
            }
        }
        return h;

    }

    static void mix64(uint64_t& a, uint64_t& b, uint64_t& c)
    {
#if 0
        a -= b; a -= c; a ^= (c>>43);
        b -= c; b -= a; b ^= (a<<9);
        c -= a; c -= b; c ^= (b>>8);
        a -= b; a -= c; a ^= (c>>38);
        b -= c; b -= a; b ^= (a<<23);
        c -= a; c -= b; c ^= (b>>5);
        a -= b; a -= c; a ^= (c>>35);
        b -= c; b -= a; b ^= (a<<49);
        c -= a; c -= b; c ^= (b>>11);
        a -= b; a -= c; a ^= (c>>12);
        b -= c; b -= a; b ^= (a<<18);
        c -= a; c -= b; c ^= (b>>22);
#endif
        a=a-b;  a=a-c;  a=a^(c >> 13);
        b=b-c;  b=b-a;  b=b^(a << 8); 
        c=c-a;  c=c-b;  c=c^(b >> 13);
        a=a-b;  a=a-c;  a=a^(c >> 12);
        b=b-c;  b=b-a;  b=b^(a << 16);
        c=c-a;  c=c-b;  c=c^(b >> 5);
        a=a-b;  a=a-c;  a=a^(c >> 3);
        b=b-c;  b=b-a;  b=b^(a << 10);
        c=c-a;  c=c-b;  c=c^(b >> 15);
    }

    static uint64_t univ(uint64_t j, uint64_t x)
    {
        static const uint64_t as[] =
        {
            (1ULL << 54) - 33,
            (1ULL << 54) - 53,
            (1ULL << 54) - 131,
            (1ULL << 54) - 165,
            (1ULL << 54) - 245,
            (1ULL << 54) - 255,
            (1ULL << 54) - 257,
            (1ULL << 54) - 315
        };
        static const uint64_t bs[] =
        {
            (1ULL << 40) - 87,
            (1ULL << 41) - 21,
            (1ULL << 42) - 11,
            (1ULL << 43) - 57,
            (1ULL << 44) - 17,
            (1ULL << 45) - 55,
            (1ULL << 46) - 21,
            (1ULL << 47) - 115
        };

        return as[j] * x + bs[j];
    }

    uint64_t lockNum(uint64_t pSlot) const
    {
        return pSlot % (1ULL << L);
    }

    const uint64_t mSlotBits;
    const uint64_t mSlotMask;
    const uint64_t mItemBits;
    const uint64_t mHashNumBits;
    const uint64_t mHashNumMask;
    const uint64_t mCountBits;
    const uint64_t mCountMask;

    std::vector<value_type> mItems;
    std::vector<Spinlock> mMutexes;
    Spinlock mOtherMutex;
    std::unordered_map<value_type,uint64_t> mOther;
    mutable std::vector<std::pair<value_type,uint64_t> > mOtherIndex;
    boost::atomic<uint64_t> mRandom;
    boost::atomic<uint64_t> mSize;
    boost::atomic<uint64_t> mSpills;
    boost::atomic<uint64_t> mPanics;
};

#endif // BACKYARDHASH_HH
