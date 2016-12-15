// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SIMPLEHASHMAP_HH
#define SIMPLEHASHMAP_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_UNORDERED_MAP
#include <unordered_map>
#define STD_UNORDERED_MAP
#endif

template <typename T, typename V>
class SimpleHashMap : public boost::noncopyable
{
private:
    static const uint64_t sMaxTries = 10;
    static const uint64_t sWordBits = 8 * sizeof(uint64_t);

    friend class Iterator;

public:

    class Iterator
    {
    public:
        
        bool valid() const
        {
            return mPos < mMap.width();
        }
    
        void operator++()
        {
            ++mPos;
            next();
        }

        const T& key() const
        {
            return mMap.mItems[mPos];
        }

        const V& value() const
        {
            return mMap.mValues[mPos];
        }

        std::pair<T,V> operator*() const
        {
            return std::make_pair(mMap.mItems[mPos], mMap.mValues[mPos]);
        }

        Iterator(const SimpleHashMap<T,V>& pMap)
            : mPos(0), mMap(pMap)
        {
            next();
        }

        
    private:
        
        void next()
        {
            for (; valid(); ++mPos)
            {
                uint64_t w = mPos / sWordBits;
                uint64_t b = mPos % sWordBits;
                if (    ((mMap.mInUse[w] >> b) & 1)
                    && !((mMap.mGone[w] >> b) & 1))
                {
                    break;
                }
            }
        }

        uint64_t mPos;
        const SimpleHashMap<T,V>& mMap;
    };

    uint64_t size() const
    {
        return mSize;
    }

    V* find(const T& pItem) const;

    V& insert(const T& pItem, const V& pValue);

    const V& operator[](const T& pItem) const
    {
        V* p = find(pItem);
        if (p)
        {
            return *p;
        }
        throw "non-const";
    }

    V& operator[](const T& pItem)
    {
        V* p = find(pItem);
        if (p)
        {
            return *p;
        }
        return insert(pItem, V());
    }

    void clear();

    void swap(SimpleHashMap& pRhs)
    {
        std::swap(mSize, pRhs.mSize);
        std::swap(mMask, pRhs.mMask);
        std::swap(mInUse, pRhs.mInUse);
        std::swap(mGone, pRhs.mGone);
        std::swap(mItems, pRhs.mItems);
        std::swap(mValues, pRhs.mValues);
    }

    uint64_t width() const
    {
        return mMask + 1;
    }

    void write(const std::string& pBaseName, FileFactory& pFactory) const;

    static SimpleHashMap<T,V> read(const std::string& pBaseName, FileFactory& pFactory);

    SimpleHashMap(uint64_t pWidth = 64);

    ~SimpleHashMap()
    {
        delete [] mInUse;
        delete [] mGone;
        delete [] mItems;
        delete [] mValues;
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
    uint64_t* mGone;
    T* mItems;
    V* mValues;
    uint64_t mSize;
};


template <typename T, typename V>
V*
SimpleHashMap<T,V>::find(const T& pItem) const
{
    uint64_t h = std::hash<T>()(pItem);
    uint64_t p = 0;
    uint64_t i = h & mMask;
    uint64_t w = i / sWordBits;
    uint64_t b = i % sWordBits;
    uint64_t j = 0;
    while ((mInUse[w] >> b) & 1)
    {
        if (mItems[i] == pItem)
        {
            if ((mGone[w] >> b) & 1)
            {
                return NULL;
            }
            return &mValues[i];
        }
        if (!p)
        {
            p = probe(h);
        }
        i = (i + p) & mMask;
        w = i / sWordBits;
        b = i % sWordBits;
        if (++j > sMaxTries)
        {
            return NULL;
        }
    }
    return NULL;
}


template <typename T, typename V>
V&
SimpleHashMap<T,V>::insert(const T& pItem, const V& pValue)
{
    BOOST_ASSERT(find(pItem) == NULL);
    //BOOST_ASSERT(!gone(pItem));
    uint64_t h = std::hash<T>()(pItem);
    uint64_t p = 0;
    uint64_t i = h & mMask;
    uint64_t w = i / sWordBits;
    uint64_t b = i % sWordBits;
    uint64_t j = 0;
    while ((mInUse[w] >> b) & 1)
    {
        if (!p)
        {
            p = probe(h);
        }
        i = (i + p) & mMask;
        w = i / sWordBits;
        b = i % sWordBits;
        if (++j > sMaxTries)
        {
            // std::cerr << "expanding from " << width() << std::endl;
            expand();
            return insert(pItem, pValue);
        }
    }
    ++mSize;
    mInUse[w] |= 1ULL << b;
    mItems[i] = pItem;
    mValues[i] = pValue;
    return mValues[i];
}


template <typename T, typename V>
void
SimpleHashMap<T,V>::clear()
{
    const uint64_t z = (width() + (sWordBits - 1)) / sWordBits;
    for (uint64_t i = 0; i < z; ++i)
    {
        mInUse[i] = 0;
        mGone[i] = 0;
    }
}


template <typename T, typename V>
SimpleHashMap<T,V>::SimpleHashMap(uint64_t pWidth)
    : mMask(0), mInUse(NULL), mItems(NULL), mSize(0)
{
    pWidth = std::max<uint64_t>(Gossamer::roundUpToNextPowerOf2(pWidth), 16);
    mMask = pWidth - 1;
    const uint64_t z = (width() + (sWordBits - 1)) / sWordBits;
    mInUse = new uint64_t[z];
    mGone = new uint64_t[z];
    for (uint64_t i = 0; i < z; ++i)
    {
        mInUse[i] = 0;
        mGone[i] = 0;
    }
    mItems = new T[width()];
    mValues = new V[width()];
}


template <typename T, typename V>
void
SimpleHashMap<T,V>::expand()
{
    const uint64_t z = width();
    SimpleHashMap<T,V> t(z * 2);
    for (uint64_t i = 0; i < z; ++i)
    {
        uint64_t w = i / sWordBits;
        uint64_t b = i % sWordBits;
        if (    ((mInUse[w] >> b) & 1)
            && !((mGone[w] >> b) & 1))
        {
            t.insert(mItems[i], mValues[i]);
        }
    }
    swap(t);
}


#endif // SIMPLEHASHMAP_HH
