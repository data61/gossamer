// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SORTBASE_HH
#define SORTBASE_HH

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_CALL_TRAITS_HPP
#include <boost/call_traits.hpp>
#define BOOST_CALL_TRAITS_HPP
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif


class ExternalSortBase
{
public:
    static void genFileName(const std::string& pBase,
                            uint64_t pNum,
                            std::string& pName);

    static void genFileName(const std::string& pDir,
                            const std::string& pBase,
                            uint64_t pNum,
                            std::string& pName);

    template<typename Source, typename Dest>
    static void copy(Source& pSrc, Dest& pDest)
    {
        while (pSrc.valid())
        {
            pDest.push_back(*pSrc);
            ++pSrc;
        }
    }

    template<typename Lhs, typename Rhs, typename Dest>
    static void merge(Lhs& pLhs, Rhs& pRhs, Dest& pDest);
};

template <typename T>
struct ExternalSortCodec
{
    static void write(std::ostream& pOut, const T& pItem)
    {
        pOut.write(reinterpret_cast<const char*>(&pItem), sizeof(pItem));
    }

    static bool read(std::istream& pIn, T& pItem)
    {
        pIn.read(reinterpret_cast<char*>(&pItem), sizeof(pItem));
        return pIn.good();
    }
};


template<typename T>
class IndirectComparator
{
public:
    bool operator()(const uint64_t pLhs, const uint64_t pRhs) const
    {
        return mItems[pLhs] < mItems[pRhs];
    }

    explicit IndirectComparator(const std::vector<T>& pItems)
        : mItems(pItems)
    {
    }

private:
    const std::vector<T>& mItems;
};


template<typename T, typename V>
class FileOutWrapper
{
public:
    void push_back(const T& pItem)
    {
        V::write(mOut, pItem);
    }

    explicit FileOutWrapper(const std::string& pName, FileFactory& pFactory)
        : mOutHolder(pFactory.out(pName)), mOut(**mOutHolder)
    {
    }

private:
    FileFactory::OutHolderPtr mOutHolder;
    std::ostream& mOut;
};


template<typename T, typename V>
class FileInWrapper
{
public:
    bool valid() const
    {
        return mValid;
    }

    const T& operator*() const
    {
        return mItem;
    }

    void operator++()
    {
        mValid = V::read(mFile, mItem);
    }

    explicit FileInWrapper(const std::string& pName,
                           FileFactory& pFactory)
        : mName(pName), mFactory(pFactory),
          mFileHolder(pFactory.in(pName)), mFile(**mFileHolder)
    {
        ++(*this);
    }

    ~FileInWrapper()
    {
    }
private:
    const std::string mName;
    FileFactory& mFactory;
    FileFactory::InHolderPtr mFileHolder;
    std::istream& mFile;
    bool mValid;
    T mItem;
};

template <typename Src, typename T>
class Merger
{
public:
    bool valid() const
    {
        return mCurr != mEnd;
    }

    const T& operator*() const
    {
        return *mCurr;
    }

    void operator++()
    {
        ++mCurr;
        if (mCurr != mEnd)
        {
            return;
        }
        refill();
    }

    Merger(uint64_t pN, Src& pLhs, Src& pRhs)
        : mN(pN), mLhs(pLhs), mRhs(pRhs)
    {
        mBuffer.reserve(pN);
        refill();
    }

private:
    void refill()
    {
        mBuffer.clear();
        while (mLhs.valid() && mRhs.valid() && mBuffer.size() < mN)
        {
            if (*mLhs < *mRhs)
            {
                mBuffer.push_back(*mLhs);
                ++mLhs;
                continue;
            }
            mBuffer.push_back(*mRhs);
            ++mRhs;
        }
        if (mBuffer.size() == mN)
        {
            mCurr = mBuffer.begin();
            mEnd = mBuffer.end();
            return;
        }
        while (mLhs.valid() && mBuffer.size() < mN)
        {
            mBuffer.push_back(*mLhs);
            ++mLhs;
        }
        while (mRhs.valid() && mBuffer.size() < mN)
        {
            mBuffer.push_back(*mRhs);
            ++mRhs;
        }
        mCurr = mBuffer.begin();
        mEnd = mBuffer.end();
    }

    const uint64_t mN;
    Src& mLhs;
    Src& mRhs;
    std::vector<T> mBuffer;
    typename std::vector<T>::const_iterator mCurr;
    typename std::vector<T>::const_iterator mEnd;
};

#include "ExternalSortUtils.tcc"

#endif
