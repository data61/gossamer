// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SIMPLEHASHSET_HH
#define SIMPLEHASHSET_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_UNORDERED_SET
#include <unordered_set>
#define STD_UNORDERED_SET
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

template <typename T>
class SimpleHashSet
{
private:
    static const uint64_t maxTries = 10;
    static const uint64_t wordBits = 8 * sizeof(uint64_t);

    class IntersectVisitor
    {
    public:
        void operator()(const T& pItem)
        {
            if (mRhs.count(pItem))
            {
                mRes.insert(pItem);
            }
        }

        IntersectVisitor(const SimpleHashSet& pRhs, SimpleHashSet& pRes)
            : mRhs(pRhs), mRes(pRes)
        {
        }

    private:
        const SimpleHashSet& mRhs;
        SimpleHashSet& mRes;
    };

    class UnionVisitor
    {
    public:
        void operator()(const T& pItem)
        {
            if (!mRes.count(pItem))
            {
                mRes.insert(pItem);
            }
        }

        UnionVisitor(SimpleHashSet& pRes)
            : mRes(pRes)
        {
        }

    private:
        SimpleHashSet& mRes;
    };

    class DifferenceVisitor
    {
    public:
        void operator()(const T& pItem)
        {
            if (!mRhs.count(pItem))
            {
                mRes.insert(pItem);
            }
        }

        DifferenceVisitor(const SimpleHashSet& pRhs, SimpleHashSet& pRes)
            : mRhs(pRhs), mRes(pRes)
        {
        }

    private:
        const SimpleHashSet& mRhs;
        SimpleHashSet& mRes;
    };

public:
    uint64_t width() const
    {
        return mMask + 1;
    }

    uint64_t size() const;

    bool count(const T& pItem) const;

    void insert(const T& pItem);

    void ensure(const T& pItem);

    void clear();

    SimpleHashSet& operator&=(const SimpleHashSet& pRhs)
    {
        SimpleHashSet t;
        IntersectVisitor vis(pRhs, t);
        visit(vis);
        swap(t);
        return *this;
    }

    SimpleHashSet& operator|=(const SimpleHashSet& pRhs)
    {
        UnionVisitor vis(*this);
        pRhs.visit(vis);
        return *this;
    }

    SimpleHashSet& operator-=(const SimpleHashSet& pRhs)
    {
        SimpleHashSet t;
        DifferenceVisitor vis(pRhs, t);
        visit(vis);
        swap(t);
        return *this;
    }

    SimpleHashSet& operator^=(const SimpleHashSet& pRhs)
    {
        SimpleHashSet t;
        DifferenceVisitor vis1(pRhs, t);
        visit(vis1);
        DifferenceVisitor vis2(*this, t);
        pRhs.vis(vis2);
        swap(t);
        return *this;
    }

    void swap(SimpleHashSet& pRhs)
    {
        std::swap(mMask, pRhs.mMask);
        std::swap(mInUse, pRhs.mInUse);
        std::swap(mItems, pRhs.mItems);
    }

    template<typename Visitor>
    void visit(Visitor& pVisitor)
    {
        uint64_t z = width();
        for (uint64_t i = 0; i < z; ++i)
        {
            uint64_t w = i / wordBits;
            uint64_t b = i % wordBits;
            if ((mInUse[w] >> b) & 1)
            {
                pVisitor(mItems[i]);
            }
        }
    }

    template<typename Visitor>
    void visit(Visitor& pVisitor) const
    {
        uint64_t z = width();
        for (uint64_t i = 0; i < z; ++i)
        {
            uint64_t w = i / wordBits;
            uint64_t b = i % wordBits;
            if ((mInUse[w] >> b) & 1)
            {
                pVisitor(mItems[i]);
            }
        }
    }

    SimpleHashSet(uint64_t pWidth = 64);

    SimpleHashSet(const SimpleHashSet& pRhs);

    SimpleHashSet& operator=(const SimpleHashSet& pRhs);

    ~SimpleHashSet()
    {
        delete [] mInUse;
        delete [] mItems;
    }

private:
    void expand();

    uint64_t probe(uint64_t pHash) const
    {
        const uint64_t a = (1ULL << 54) - 131;
        const uint64_t b = (1ULL << 45) - 55;
        return ((pHash * a + b) & (mMask >> 1)) + 1;
    }

    uint64_t mMask;
    uint64_t* mInUse;
    T* mItems;
};


template<typename T>
void
SimpleHashSet<T>::ensure(const T& pItem)
{
    uint64_t h = std::hash<T>()(pItem);
    uint64_t p = 0;
    uint64_t i = h & mMask;
    uint64_t w = i / wordBits;
    uint64_t b = i % wordBits;
    uint64_t j = 0;
    while ((mInUse[w] >> b) & 1)
    {
        if (mItems[i] == pItem)
        {
            return;
        }
        if (!p)
        {
            p = probe(h);
        }
        i = (i + p) & mMask;
        w = i / wordBits;
        b = i % wordBits;
        if (++j > maxTries)
        {
            expand();
            // We know pItem is not a member, so call
            // insert() rather than ensure().
            insert(pItem);
            return;
        }
    }
    mInUse[w] |= 1ULL << b;
    mItems[i] = pItem;
}


template<typename T>
void
SimpleHashSet<T>::insert(const T& pItem)
{
    BOOST_ASSERT(count(pItem) == false);
    uint64_t h = std::hash<T>()(pItem);
    uint64_t p = 0;
    uint64_t i = h & mMask;
    uint64_t w = i / wordBits;
    uint64_t b = i % wordBits;
    uint64_t j = 0;
    while ((mInUse[w] >> b) & 1)
    {
        if (!p)
        {
            p = probe(h);
        }
        i = (i + p) & mMask;
        w = i / wordBits;
        b = i % wordBits;
        if (++j > maxTries)
        {
            expand();
            insert(pItem);
            return;
        }
    }
    mInUse[w] |= 1ULL << b;
    mItems[i] = pItem;
}


template<typename T>
void
SimpleHashSet<T>::clear()
{
    uint64_t z = (width() + (wordBits - 1)) / wordBits;
    for (uint64_t i = 0; i < z; ++i)
    {
        mInUse[i] = 0;
    }
}


template<typename T>
SimpleHashSet<T>::SimpleHashSet(uint64_t pWidth)
    : mMask(0), mInUse(NULL), mItems(NULL)
{
    pWidth = std::max<uint64_t>(Gossamer::roundUpToNextPowerOf2(pWidth), 16);
    mMask = pWidth - 1;
    uint64_t z = (width() + (wordBits - 1)) / wordBits;
    mInUse = new uint64_t[z];
    for (uint64_t i = 0; i < z; ++i)
    {
        mInUse[i] = 0;
    }
    mItems = new T[width()];
}


template<typename T>
SimpleHashSet<T>::SimpleHashSet(const SimpleHashSet& pRhs)
{
    clear();
    uint64_t z = pRhs.width();
    for (uint64_t i = 0; i < z; ++i)
    {
        uint64_t w = i / wordBits;
        uint64_t b = i % wordBits;
        if ((pRhs.mInUse[w] >> b) & 1)
        {
            insert(pRhs.mItems[i]);
        }
    }
}


template<typename T>
uint64_t
SimpleHashSet<T>::size() const
{
    uint64_t elements = 0;
    uint64_t z = (width() + (wordBits - 1)) / wordBits;
    for (uint64_t i = 0; i < z; ++i)
    {
        elements += Gossamer::popcnt(mInUse[i]);
    }
    return elements;
}


template<typename T>
bool
SimpleHashSet<T>::count(const T& pItem) const
{
    uint64_t h = std::hash<T>()(pItem);
    uint64_t p = 0;
    uint64_t i = h & mMask;
    uint64_t w = i / wordBits;
    uint64_t b = i % wordBits;
    uint64_t j = 0;
    while ((mInUse[w] >> b) & 1)
    {
        if (mItems[i] == pItem)
        {
            return true;
        }
        if (!p)
        {
            p = probe(h);
        }
        i = (i + p) & mMask;
        w = i / wordBits;
        b = i % wordBits;
        if (++j > maxTries)
        {
            return false;
        }
    }
    return false;
}


template<typename T>
SimpleHashSet<T>&
SimpleHashSet<T>::operator=(const SimpleHashSet& pRhs)
{
    clear();
    uint64_t z = pRhs.width();
    for (uint64_t i = 0; i < z; ++i)
    {
        uint64_t w = i / wordBits;
        uint64_t b = i % wordBits;
        if ((pRhs.mInUse[w] >> b) & 1)
        {
            insert(pRhs.mItems[i]);
        }
    }
    return *this;
}


template<typename T>
void
SimpleHashSet<T>::expand()
{
    uint64_t z = width();
    SimpleHashSet<T> t(z * 2);
    for (uint64_t i = 0; i < z; ++i)
    {
        uint64_t w = i / wordBits;
        uint64_t b = i % wordBits;
        if ((mInUse[w] >> b) & 1)
        {
            t.insert(mItems[i]);
        }
    }
    swap(t);
}


#endif // SIMPLEHASHSET_HH
