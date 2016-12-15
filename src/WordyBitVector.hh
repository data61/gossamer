// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef WORDYBITVECTOR_HH
#define WORDYBITVECTOR_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

class WordyBitVector
{
public:
    struct Inverted
    {
        static const bool sInvert = true;
    };
    
    struct Uninverted
    {
        static const bool sInvert = false;
    };

    static const uint64_t one = 1;
    static const uint64_t wordBits = 64;

    class Builder
    {
    public:
        // Pad to the position pPos, but do not add a bit at pPos.
        void padTo(uint64_t pPos)
        {
            BOOST_ASSERT(mCurrPos <= pPos);

            uint64_t destWordNum = pPos / wordBits;
            if (mCurrWordNum < destWordNum)
            {
                flush();

                // The next call to flush() will automatically fill in any
                // intervening zero words.
                mCurrWordNum = destWordNum;
                mCurrWord = 0;
            }
            mCurrPos = pPos;
        }

        // Add a 1 bit at pPos.
        // It is a requirement that sequential calls to
        // push give strictly increasing values of pPos.
        //
        void push(uint64_t pPos)
        {
            padTo(pPos);
            push_backX(true);
        }

        void pad(uint64_t pPos)
        {
            padTo(pPos+1);
        }

        void push_backX(bool pBit)
        {
            uint64_t w = mCurrPos / wordBits;
            uint64_t b = mCurrPos % wordBits;

            if (w != mCurrWordNum)
            {
                flush();
                mCurrWordNum = w;
                mCurrWord = 0;
            }

            if (pBit)
            {
                mCurrWord |= one << b;
            }

            ++mCurrPos;
        }


        // Signal that no more bits will be given.
        //
        void end()
        {
            flush();
        }


        // Constructor
        //
        Builder(const std::string& pName, FileFactory& pFactory);

    private:
        // Write out the current word,
        // inserting any 0s into the file as necessary.
        //
        void flush();

        MappedArray<uint64_t>::Builder mFile;
        uint64_t mCurrPos;
        uint64_t mFileWordNum;
        uint64_t mCurrWordNum;
        uint64_t mCurrWord;
    };

    template <typename Itr>
    class GeneralIterator
    {
    public:
        bool valid() const
        {
            return mValid;
        }

        uint64_t operator*() const
        {
            return mCurrWordNum * wordBits + mCurrBitPos;
        }

        void operator++()
        {
            next();
            seek1();
        }

        GeneralIterator(const Itr& pItr)
            : mWordItr(pItr), mValid(true),
              mCurrWordNum(0), mCurrBitPos(0), mCurrWord(0)
        {
            if (mWordItr.size() > 0)
            {
                mCurrWord = *mWordItr;
            }
            else
            {
                mValid = false;
            }
            seek1();
        }

    private:
        void next()
        {
            uint64_t bit = mCurrWord & -mCurrWord;  // Extract lowest-order set bit...
            mCurrWord &= ~bit;                      // ..and clear it.
        }

        void seek1()
        {
            BOOST_ASSERT(valid());
            while (mCurrWord == 0)
            {
                mCurrWordNum++;
                ++mWordItr;
                if (!mWordItr.valid())
                {
                    mValid = false;
                    return;
                }
                mCurrWord = *mWordItr;
            }
            mCurrBitPos = Gossamer::find_first_set(mCurrWord) - 1;
        }

        Itr mWordItr;
        bool mValid;
        uint64_t mCurrWordNum;
        uint64_t mCurrBitPos;
        uint64_t mCurrWord;
    };

    typedef GeneralIterator<MappedArray<uint64_t>::Iterator> Iterator1;
    typedef GeneralIterator<MappedArray<uint64_t>::LazyIterator> LazyIterator1;

    // Return the number of WORDS in the bitmap
    //
    uint64_t words() const
    {
        return mWords.size();
    }

    
    // Get the value at a given bit position.
    //
    bool get(uint64_t pBitPos) const
    {
        uint64_t w = pBitPos / wordBits;
        uint64_t b = pBitPos % wordBits;
        if (w >= words())
        {
            return false;
        }
        return mWords[w] & (one << b);
    }

    // Return an object for iterating over
    // the positions of the 1s.
    //
    Iterator1 iterator1() const
    {
        return Iterator1(mWords.iterator());
    }

    static LazyIterator1 lazyIterator1(const std::string& pName, FileFactory& pFactory)
    {
        return LazyIterator1(MappedArray<uint64_t>::lazyIterator(pName, pFactory));
    }

    // Find the position of the 'pCount'th 0 bit
    // relative to the 'pFrom'th bit.
    //
    uint64_t select0(uint64_t pFrom, uint64_t pCount) const
    {
        return select<Inverted>(pFrom, pCount);
    }

    // Find the position of the 'pCount'th 1 bit
    // relative to the 'pFrom'th bit.
    //
    uint64_t select1(uint64_t pFrom, uint64_t pCount) const
    {
        return select<Uninverted>(pFrom, pCount);
    }

    // Count the number of 1 bits in the range [pBegin,pEnd).
    //
    uint64_t popcountRange(uint64_t pBegin, uint64_t pEnd) const;

    // Find the position of the 'pCount'th bit
    // relative to the 'pFrom'th bit.
    //
    template <typename Sense>
    uint64_t select(uint64_t pFrom, uint64_t pCount) const;

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putProp("storage", words() * sizeof(uint64_t));
        return t;
    }

    // Constructor
    //
    WordyBitVector(const std::string& pName, FileFactory& pFactory);

private:
    MappedArray<uint64_t> mWords;
};

#include "WordyBitVector.tcc"

#endif // WORDYBITVECTOR_HH
