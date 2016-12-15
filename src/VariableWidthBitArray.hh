// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef VARIABLEWIDTHBITARRAY_HH
#define VARIABLEWIDTHBITARRAY_HH

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


class VariableWidthBitArray
{
public:
    static const uint64_t one = 1;
    static const uint64_t W = 64;

    class Builder
    {
    public:
        void push_back(uint64_t pItem, uint64_t pWidth)
        {
            BOOST_ASSERT(pWidth <= 64);
            BOOST_ASSERT((pItem >> pWidth) == 0);
            uint64_t b = mPos % W;
            mCurrWord |= pItem << b;
            if (b + pWidth >= W)
            {
                flush();
                pItem >>= W - b;
                mCurrWord = pItem;
            }
            mPos += pWidth;
        }

        void end();

        Builder(const std::string& pBaseName, FileFactory& pFactory);

    private:
        void flush();

        MappedArray<uint64_t>::Builder mFile;
        uint64_t mPos;
        uint64_t mCurrWord;
    };

    template <typename Itr>
    class GeneralIterator
    {
    public:

        bool valid() const
        {
            return mPos < mSize;
        }
    
        /**
         * Get and advance past the next pWidth bits.
         */ 
        uint64_t get(uint64_t pWidth)
        {
            uint64_t x(0);
            
            if ((mPos % 64) + pWidth > 63)
            {
                // Crossed word boundary.
                uint64_t bot_sz = 64 - (mPos % 64);
                uint64_t top_sz = pWidth - bot_sz;
                uint64_t bot_m = (1ULL << bot_sz) - 1;
                uint64_t top_m = (1ULL << top_sz) - 1;
                uint64_t bot = bot_m & (*mArrayItr >> (64 - bot_sz));
                ++mArrayItr;
                uint64_t top = top_m & *mArrayItr;
                x = top << bot_sz | bot;
            }
            else
            {
                uint64_t m = (1ULL << pWidth) - 1;
                x = m & (*mArrayItr >> (mPos % 64));
            }

            mPos += pWidth;
            return x;
        }

        GeneralIterator(const Itr& pItr)
            : mArrayItr(pItr),
              mPos(0), mSize(64 * pItr.size())
        {
        }

    private:
        
        Itr mArrayItr;
        uint64_t mPos;              // In bits
        uint64_t mSize;             // In bits
    };

    typedef GeneralIterator<MappedArray<uint64_t>::Iterator> Iterator;
    typedef GeneralIterator<MappedArray<uint64_t>::LazyIterator> LazyIterator;

    Iterator iterator() const
    {
        return Iterator(mFile.iterator());
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        return LazyIterator(MappedArray<uint64_t>::lazyIterator(pBaseName, pFactory));
    }

    uint64_t get(uint64_t pOffset, uint64_t pWidth) const
    {
        uint64_t w = pOffset / W;
        uint64_t b = pOffset % W;
        if (b + pWidth <= W)
        {
            // One word case.
            //
            return (mFile[w] >> b) & ((one << pWidth) - 1);
        }

        uint64_t j = W - b;
        return ((mFile[w] >> b) | (mFile[w + 1] << j)) & ((one << pWidth) - 1);
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("words", mFile.stat());

        uint64_t s = 0;
        s += t("words").as<uint64_t>("storage");
        t.putProp("storage", s);

        return t;
    }

    VariableWidthBitArray(const std::string& pBaseName, FileFactory& pFactory);

private:
    MappedArray<uint64_t> mFile;
};

#endif // VARIABLEWIDTHBITARRAY_HH
