// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef VARIABLEBYTEARRAY_HH
#define VARIABLEBYTEARRAY_HH


#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef SPARSEARRAY_HH
#include "SparseArray.hh"
#endif

template <typename T>
struct VariableByteArrayTraits
{
    static typename T::value_type init(uint64_t pVal);

    static uint64_t asUInt64(const typename T::value_type& pVal);
};

#if 0
template <>
struct VariableByteArrayTraits<RRRRank>
{
    static RRRRank::position_type init(uint64_t pVal)
    {
        return pVal;
    }

    static uint64_t asUInt64(uint64_t pVal)
    {
        return pVal;
    }
};
#endif

template <>
struct VariableByteArrayTraits<SparseArray>
{
    static SparseArray::position_type init(uint64_t pVal)
    {
        return SparseArray::position_type(pVal);
    }

    static uint64_t asUInt64(const SparseArray::position_type& pVal)
    {
        return pVal.asUInt64();
    }
};

class VariableByteArray
{
public:
    typedef SparseArray bitmap_type;
    typedef VariableByteArrayTraits<bitmap_type> bitmap_traits;
    typedef uint32_t value_type;

    static const uint64_t one = 1;

    class Builder
    {
    public:
        typedef VariableByteArray::value_type value_type;
        
        void push_back(value_type pNumber)
        {
            uint64_t pos = mPosition0++;
            mOrder0.push_back(static_cast<uint8_t>(pNumber & 0xff));

            if (!(pNumber >>= 8))
            {
                return;
            }

            mOrder1Present.push_back(bitmap_type::position_type(pos));

            pos = mPosition1++;
            mOrder1.push_back(static_cast<uint8_t>(pNumber & 0xff));

            if (!(pNumber >>= 8))
            {
                return;
            }

            mOrder2Present.push_back(bitmap_traits::init(pos));
            mOrder2.push_back(static_cast<uint16_t>(pNumber & 0xffff));
        }

        void end();

        Builder(const std::string& pBaseName, FileFactory& pFactory, uint64_t pNumItems, double pFrac);

    private:
        uint64_t mPosition0;
        uint64_t mPosition1;

        MappedArray<uint8_t>::Builder mOrder0;
        bitmap_type::Builder mOrder1Present;
        MappedArray<uint8_t>::Builder mOrder1;
        bitmap_type::Builder mOrder2Present;
        MappedArray<uint16_t>::Builder mOrder2;
    };

    template <typename MItr8, typename MItr16, typename BItr>
    class GeneralIterator
    {
    public:
        bool valid() const
        {
            return mValid;
        }

        value_type operator*() const
        {
            return mCurr;
        }

        void operator++()
        {
            ++mOrd0Itr;
            get();
        }

        GeneralIterator(const MItr8& pOrd0Itr, 
                        const BItr& pOrd1PresItr, const MItr8& pOrd1Itr, 
                        const BItr& pOrd2PresItr, const MItr16& pOrd2Itr)
            : mOrd0Itr(pOrd0Itr), 
              mOrd1PresItr(pOrd1PresItr), mOrd1Itr(pOrd1Itr), 
              mOrd2PresItr(pOrd2PresItr), mOrd2Itr(pOrd2Itr),
              mCurr(0), mValid(true)
        {
            get();
        }

    private:

        uint64_t size() const
        {
            return mOrd0Itr.size();
        }

        void get()
        {
            if (!mOrd0Itr.valid())
            {
                mValid = false;
                return;
            }
            mCurr = *mOrd0Itr;
            while (mOrd1PresItr.valid() && bitmap_traits::asUInt64(*mOrd1PresItr) < mOrd0Itr.position())
            {
                ++mOrd1PresItr;
                ++mOrd1Itr;
            }
            if (!mOrd1PresItr.valid() || bitmap_traits::asUInt64(*mOrd1PresItr) > mOrd0Itr.position())
            {
                return;
            }
            mCurr |= static_cast<value_type>(*mOrd1Itr) << 8;
            while (mOrd2PresItr.valid() && bitmap_traits::asUInt64(*mOrd2PresItr) < mOrd1Itr.position())
            {
                ++mOrd2PresItr;
                ++mOrd2Itr;
            }
            if (!mOrd2PresItr.valid() || bitmap_traits::asUInt64(*mOrd2PresItr) > mOrd1Itr.position())
            {
                return;
            }
            mCurr |= static_cast<value_type>(*mOrd2Itr) << 16;
        }

        MItr8 mOrd0Itr;
        BItr mOrd1PresItr;
        MItr8 mOrd1Itr;
        BItr mOrd2PresItr;
        MItr16 mOrd2Itr;
        value_type mCurr;
        bool mValid;
    };

    typedef GeneralIterator<MappedArray<uint8_t>::Iterator,
                            MappedArray<uint16_t>::Iterator,
                            bitmap_type::Iterator>
                                                                Iterator;
    typedef GeneralIterator<MappedArray<uint8_t>::LazyIterator,
                            MappedArray<uint16_t>::LazyIterator,
                            bitmap_type::LazyIterator>
                                                                LazyIterator;

    Iterator iterator() const
    {
        return Iterator(mOrder0.iterator(),
                        mOrder1Present.iterator(), mOrder1.iterator(),
                        mOrder2Present.iterator(), mOrder2.iterator());
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        return LazyIterator(MappedArray<uint8_t>::lazyIterator(pBaseName + ".ord0", pFactory),
                            bitmap_type::lazyIterator(pBaseName + ".ord1p", pFactory),
                            MappedArray<uint8_t>::lazyIterator(pBaseName + ".ord1", pFactory),
                            bitmap_type::lazyIterator(pBaseName + ".ord2p", pFactory),
                            MappedArray<uint16_t>::lazyIterator(pBaseName + ".ord2", pFactory));
    }

    uint64_t size() const
    {
        return mOrder0.size();
    }

    value_type operator[](uint64_t pIndex) const
    {
        using namespace std;

        value_type result = static_cast<value_type>(mOrder0[pIndex]);
        uint64_t r1;
        if (!mOrder1Present.accessAndRank(bitmap_traits::init(pIndex), r1))
        {
            return result;
        }
        result |= static_cast<value_type>(mOrder1[r1]) << 8;

        uint64_t r2;
        if (!mOrder2Present.accessAndRank(bitmap_traits::init(r1), r2))
        {
            return result;
        }

        result |= static_cast<value_type>(mOrder2[r2]) << 16;
        return result;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("ord1-pred", mOrder1Present.stat());
        t.putSub("ord2-pred", mOrder2Present.stat());

        t.putProp("size", size());

        uint64_t s = 0;
        s += 1 * mOrder0.size();
        s += t("ord1-pred").as<uint64_t>("storage");
        s += 1 * mOrder1.size();
        s += t("ord2-pred").as<uint64_t>("storage");
        s += 2 * mOrder2.size();
        t.putProp("storage", s);

        return t;
    }

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    VariableByteArray(const std::string& pBaseName, FileFactory& pFactory);

private:

    friend class VariableByteArray::Builder;
    // friend class VariableByteArray::Iterator;

    MappedArray<uint8_t> mOrder0;
    bitmap_type mOrder1Present;
    MappedArray<uint8_t> mOrder1;
    bitmap_type mOrder2Present;
    MappedArray<uint16_t> mOrder2;
};

#endif // VARIABLEBYTEARRAY_HH
