// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef STACKEDARRAY_HH
#define STACKEDARRAY_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef RANKSELECT_HH
#include "RankSelect.hh"
#endif

#ifndef STD_LIMITS
#include <limits>
#define STD_LIMITS
#endif

#ifndef BOOST_STATIC_ASSERT_HPP
#include <boost/static_assert.hpp>
#define BOOST_STATIC_ASSERT_HPP
#endif

template <typename T>
struct StackedArrayTraits
{
};

template<>
struct StackedArrayTraits<uint8_t>
{
    typedef uint8_t value_type;
    typedef MappedArray<uint8_t> array_type;
    static const uint64_t value_bits = 8;

    static value_type extract(const Gossamer::position_type::value_type& pValue)
    {
        return pValue.asUInt64();
    }

    static uint64_t upper_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::upper_bound(u + pBegin, u + pEnd, pVal) - u;
    }

    static uint64_t lower_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::lower_bound(u + pBegin, u + pEnd, pVal) - u;
    }
};

template<>
struct StackedArrayTraits<uint16_t>
{
    typedef uint16_t value_type;
    typedef MappedArray<uint16_t> array_type;
    static const uint64_t value_bits = 16;

    static value_type extract(const Gossamer::position_type::value_type& pValue)
    {
        return pValue.asUInt64();
    }

    static uint64_t upper_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::upper_bound(u + pBegin, u + pEnd, pVal) - u;
    }

    static uint64_t lower_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::lower_bound(u + pBegin, u + pEnd, pVal) - u;
    }
};

template<>
struct StackedArrayTraits<uint32_t>
{
    typedef uint32_t value_type;
    typedef MappedArray<uint32_t> array_type;
    static const uint64_t value_bits = 32;

    static value_type extract(const Gossamer::position_type::value_type& pValue)
    {
        return pValue.asUInt64();
    }

    static uint64_t upper_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::upper_bound(u + pBegin, u + pEnd, pVal) - u;
    }

    static uint64_t lower_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::lower_bound(u + pBegin, u + pEnd, pVal) - u;
    }
};

template<>
struct StackedArrayTraits<uint64_t>
{
    typedef uint64_t value_type;
    typedef MappedArray<uint64_t> array_type;
    static const uint64_t value_bits = 64;

    static value_type extract(const Gossamer::position_type::value_type& pValue)
    {
        return pValue.asUInt64();
    }

    static uint64_t upper_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::upper_bound(u + pBegin, u + pEnd, pVal) - u;
    }

    static uint64_t lower_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        const value_type* u = pArray.begin();
        return Gossamer::lower_bound(u + pBegin, u + pEnd, pVal) - u;
    }
};

template <typename Upr, typename Lwr,
          typename UprTraits = StackedArrayTraits<Upr>,
          typename LwrTraits = StackedArrayTraits<Lwr> >
class StackedArray
{
public:
    typedef Gossamer::position_type::value_type value_type;
    typedef Gossamer::position_type::value_type integer_type;

    static const uint64_t value_bits = UprTraits::value_bits + LwrTraits::value_bits;

    static const uint64_t lwrBits = LwrTraits::value_bits;

    class Builder
    {
    public:
        void push_back(const integer_type& pItem)
        {
            integer_type upr(pItem);
            upr >>= lwrBits;
            mUprBldr.push_back(UprTraits::extract(upr));
            mLwrBldr.push_back(LwrTraits::extract(pItem));
        }

        void end()
        {
            mUprBldr.end();
            mLwrBldr.end();
        }

        Builder(const std::string& pBaseName, FileFactory& pFactory)
            : mUprBldr(pBaseName + ".upr", pFactory),
              mLwrBldr(pBaseName + ".lwr", pFactory)
        {
        }

    private:
        typename UprTraits::array_type::Builder mUprBldr;
        typename LwrTraits::array_type::Builder mLwrBldr;
    };

    class LazyIterator
    {
    public:
        bool valid() const
        {
            return mLwr.valid();
        }

        value_type operator*() const
        {
            value_type v(*mUpr);
            v <<= lwrBits;
            v |= value_type(*mLwr);
            return v;
        }

        void operator++()
        {
            ++mUpr;
            ++mLwr;
        }

        LazyIterator(const std::string& pBaseName, FileFactory& pFactory)
            : mUpr(pBaseName + ".upr", pFactory), mLwr(pBaseName + ".lwr", pFactory)
        {
        }

    private:
        typename UprTraits::array_type::LazyIterator mUpr;
        typename LwrTraits::array_type::LazyIterator mLwr;
    };

    uint64_t size() const
    {
        return mUpr.size();
    }

    integer_type operator[](uint64_t pIdx) const
    {
        integer_type retval(mUpr[pIdx]);
        retval <<= lwrBits;
        retval |= mLwr[pIdx];
        return retval;
    }

    uint64_t lower_bound(uint64_t pBegin, uint64_t pEnd, const integer_type& pVal) const
    {
        integer_type upr(pVal);
        upr >>= lwrBits;
        typename LwrTraits::value_type lwrVal = LwrTraits::extract(pVal);
        typename UprTraits::value_type uprVal = UprTraits::extract(upr);
        pBegin = UprTraits::lower_bound(mUpr, pBegin, pEnd, uprVal);
        pEnd = UprTraits::upper_bound(mUpr, pBegin, pEnd, uprVal);
        return LwrTraits::lower_bound(mLwr, pBegin, pEnd, lwrVal);
    }

    uint64_t upper_bound(uint64_t pBegin, uint64_t pEnd, const integer_type& pVal) const
    {
        integer_type upr(pVal);
        upr >>= lwrBits;
        typename LwrTraits::value_type lwrVal = LwrTraits::extract(pVal);
        typename UprTraits::value_type uprVal = UprTraits::extract(upr);
        pBegin = UprTraits::lower_bound(mUpr, pBegin, pEnd, uprVal);
        pEnd = UprTraits::upper_bound(mUpr, pBegin, pEnd, uprVal);
        return LwrTraits::upper_bound(mLwr, pBegin, pEnd, lwrVal);
    }

    StackedArray(const std::string& pBaseName, FileFactory& pFactory)
        : mUpr(pBaseName + ".upr", pFactory),
          mLwr(pBaseName + ".lwr", pFactory)
    {
        BOOST_ASSERT(mUpr.size() == mLwr.size());
    }

private:
    typename UprTraits::array_type mUpr;
    typename LwrTraits::array_type mLwr;
};


template <typename Upr,typename Lwr>
struct StackedArrayTraits<StackedArray<Upr,Lwr> >
{
    typedef typename StackedArray<Upr,Lwr>::value_type value_type;
    typedef StackedArray<Upr,Lwr> array_type;
    static const uint64_t value_bits = array_type::value_bits;

    static value_type extract(const Gossamer::position_type::value_type& pValue)
    {
        return pValue;
    }

    static uint64_t upper_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        return pArray.upper_bound(pBegin, pEnd, pVal);
    }

    static uint64_t lower_bound(const array_type& pArray, uint64_t pBegin, uint64_t pEnd, const value_type& pVal)
    {
        return pArray.lower_bound(pBegin, pEnd, pVal);
    }
};


#endif // STACKEDARRAY_HH
