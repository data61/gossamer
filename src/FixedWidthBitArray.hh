// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FIXEDWIDTHBITARRAY_HH
#define FIXEDWIDTHBITARRAY_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif


template <int N>
class FixedWidthBitArray
{
public:
    static const uint64_t one = 1;
    static const uint64_t W = 64 / N;

    class Builder
    {
    public:
        void push_back(uint64_t pItem)
        {
            uint64_t w = mPos / W;
            uint64_t j = mPos % W;

            if (mCurrWordNum != w)
            {
                flush();
                mCurrWordNum = w;
                mCurrWord = 0;
            }

            mCurrWord |= pItem << (j * N);
            mPos++;
        }

        void end()
        {
            flush();
            mFile.end();
        }

        Builder(const std::string& pBaseName, FileFactory& pFactory)
            : mPos(0), mCurrWordNum(0), mCurrWord(0),
              mFile(pBaseName, pFactory)
        {
        }

    private:
        void flush()
        {
            mFile.push_back(mCurrWord);
        }

        uint64_t mPos;
        uint64_t mCurrWordNum;
        uint64_t mCurrWord;
        MappedArray<uint64_t>::Builder mFile;
    };

    template <typename Itr>
    class GeneralIterator
    {
    public:

        bool valid() const
        {
            return mPos < mSize;
        }

        uint64_t operator*() const
        {
            return mCurr;
        }
    
        void operator++()
        {
            ++mPos;
            mMaskShift += N;
            if (mMaskShift + N > 64)
            {
                mMaskShift = 0;
                ++mArrayItr;
            }
            
            mCurr = mMask & (*mArrayItr >> mMaskShift);
        }

        GeneralIterator(const Itr& pItr)
            : mArrayItr(pItr),
              mCurr(0), mPos(0), mMaskShift(0),
              mSize(64 * pItr.size() / N), mMask((1ULL << N) - 1)
        {
            mCurr = mMask & *mArrayItr;
        }

    private:
        
        Itr mArrayItr;
        uint64_t mCurr;
        uint64_t mPos;
        uint64_t mMaskShift;

        uint64_t mSize;
        uint64_t mMask;
    };

    typedef GeneralIterator<MappedArray<uint64_t>::Iterator> Iterator;
    typedef GeneralIterator<MappedArray<uint64_t>::LazyIterator> LazyIterator;

    Iterator iterator() const
    {
        return Iterator(mWords.iterator());
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        return LazyIterator(MappedArray<uint64_t>::lazyIterator(pBaseName, pFactory));
    }

    uint64_t get(uint64_t pPos) const
    {
        uint64_t w = pPos / W;
        uint64_t j = pPos % W;
        uint64_t x = mWords[w];
        return (x >> (j * N)) & ((one << N) - 1);
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putProp("item-size", N);
        t.putSub("words", mWords.stat());

        uint64_t s = 0;
        s += t("words").as<uint64_t>("storage");
        t.putProp("storage", s);

        return t;
    }

    FixedWidthBitArray(const std::string& pBaseName, FileFactory& pFactory)
        : mWords(pBaseName, pFactory)
    {
    }

private:
    MappedArray<uint64_t> mWords;
};

#endif // FIXEDWIDTHBITARRAY_HH
