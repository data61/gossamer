// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**
 * Notes:
 * position_type is used for the position in the bit map so must be a BigInteger.
 */
#ifndef RANKSELECT_HH
#define RANKSELECT_HH

#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef BOOST_OPERATORS_HPP
#include <boost/operators.hpp>
#define BOOST_OPERATORS_HPP
#endif

#ifndef BOOST_CALL_TRAITS_HPP
#include <boost/call_traits.hpp>
#define BOOST_CALL_TRAITS_HPP
#endif

#ifndef STD_UNORDERED_SET
#include <unordered_set>
#define STD_UNORDERED_SET
#endif

#ifndef NUMERIC_CONVERSION_UTIL_HH
#include "NumericConversionUtil.hh"
#endif

#ifndef BIGINTEGER_HH
#include "BigInteger.hh"
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif

namespace Gossamer
{
    typedef uint64_t rank_type;

    class position_type
        : boost::equality_comparable<position_type
        , boost::less_than_comparable<position_type
        , boost::addable<position_type, uint64_t
        , boost::subtractable<position_type, uint64_t
        , boost::incrementable<position_type
        , boost::decrementable<position_type
        , boost::andable<position_type
        , boost::orable<position_type
        , boost::orable<position_type, uint64_t
        , boost::xorable<position_type
        , boost::left_shiftable<position_type, uint64_t
        , boost::right_shiftable<position_type, uint64_t
        > > > > > > > > > > > >
    {
    public:
        typedef BigInteger<2> value_type;

        typedef boost::call_traits<value_type>::param_type value_param_type;
        typedef boost::call_traits<value_type>::value_type value_return_type;
        typedef boost::call_traits<value_type>::reference value_reference;
        typedef boost::call_traits<value_type>::const_reference value_const_reference;

        typedef const position_type& param_type;
        typedef position_type return_type;
        typedef position_type& reference;
        typedef const position_type& const_reference;

        struct Hash
        {
            size_t operator() (param_type pPos) const
            {
                return std::hash<value_type>()(pPos.value());
            }
        };

        position_type()
            : mValue(0)
        {
        }

        explicit position_type(uint64_t pValue)
            : mValue(pValue)
        {
        }

        explicit position_type(value_param_type pValue)
            : mValue(pValue)
        {
        }

        void reverse()
        {
            mValue.reverse();
        }

        void reverseComplement(const uint64_t& pK)
        {
            mValue.reverseComplement(pK);
        }

        bool isNormal(const uint64_t& pK)
        {
            position_type rc(*this);
            rc.reverseComplement(pK);
            uint64_t h0 = Hash()(*this);
            uint64_t h1 = Hash()(rc);
            return (h0 < h1) || ((h0 == h1) && (rc >= *this));
        }

        void normalize(const uint64_t& pK)
        {
            position_type rc(*this);
            rc.reverseComplement(pK);
            uint64_t h0 = Hash()(*this);
            uint64_t h1 = Hash()(rc);
            if (h0 > h1)
            {
                *this = rc;
            }
            else if (h0 == h1 && rc < *this)
            {
                *this = rc;
            }
        }

        position_type normalized(const uint64_t& pK) const
        {
            position_type p(*this);
            p.normalize(pK);
            return p;
        }

        bool fitsIn64Bits() const
        {
            return mValue.fitsIn64Bits();
        }

        uint64_t asUInt64() const
        {
            return mValue.asUInt64();
        }

        uint64_t mostSigWord() const
        {
            return mValue.mostSigWord();
        }

        double asDouble() const
        {
            return mValue.asDouble();
        }

        bool operator==(param_type pRhs) const
        {
            return mValue == pRhs.mValue;
        }

        bool operator<(param_type pRhs) const
        {
            return mValue < pRhs.mValue;
        }

        reference operator+=(position_type::param_type pRhs)
        {
            mValue += pRhs.mValue;
            return *this;
        }

        reference operator+=(uint64_t pRhs)
        {
            mValue += pRhs;
            return *this;
        }

        reference operator-=(uint64_t pRhs)
        {
            mValue -= pRhs;
            return *this;
        }

        reference operator|=(param_type pRhs)
        {
            mValue |= pRhs.mValue;
            return *this;
        }

        reference operator|=(uint64_t pRhs)
        {
            mValue |= pRhs;
            return *this;
        }

        reference operator&=(param_type pRhs)
        {
            mValue &= pRhs.mValue;
            return *this;
        }

        uint64_t operator&(uint64_t pRhs) const
        {
            return mValue & pRhs;
        }

        reference operator&=(uint64_t pRhs)
        {
            mValue &= pRhs;
            return *this;
        }

        reference operator^=(param_type pRhs)
        {
            mValue ^= pRhs.mValue;
            return *this;
        }

        reference operator<<=(uint64_t pBits)
        {
            mValue <<= pBits;
            return *this;
        }

        reference operator>>=(uint64_t pBits)
        {
            mValue >>= pBits;
            return *this;
        }

        reference operator++()
        {
            ++mValue;
            return *this;
        }

        reference operator--()
        {
            --mValue;
            return *this;
        }

        bool boolean_test() const
        {
            return mValue.boolean_test();
        }

        std::pair<const uint64_t*,const uint64_t*> words() const
        {
            return mValue.words();
        }

        std::pair<uint64_t*,uint64_t*> words()
        {
            return mValue.words();
        }

        friend std::ostream&
        operator<<(std::ostream& pStream, param_type pValue)
        {
            return pStream << pValue.mValue;
        }

#if 0
        friend std::istream&
        operator>>(std::istream& pStream, reference pValue)
        {
            return pStream >> pValue.mValue;
        }
#endif

        value_return_type value() const
        {
            return mValue;
        }

        friend return_type operator~(param_type pRhs)
        {
            return position_type(~pRhs.value());
        }

    private:
        value_type mValue;
    };

    inline void kmerToString(const uint64_t k, const Gossamer::position_type& x, std::string& s)
    {
        static const char bases[4] = {'A', 'C', 'G', 'T'};
        s.reserve(s.size() + 2 * k);
        for (uint64_t i = 0; i < k; ++i)
        {
            uint64_t j = k - i - 1;
            s += bases[(x >> (2 * j)).asUInt64() & 3];
        }
    }

    inline std::string kmerToString(const uint64_t k, const Gossamer::position_type& x)
    {
        std::string s;
        kmerToString(k, x, s);
        return s;
    }

}


// std::numeric_limits
namespace std {
    template<>
    struct numeric_limits<Gossamer::position_type>
        : public WrappedNumericLimits<Gossamer::position_type,
                                      Gossamer::position_type::value_type>
    {
    };
}

// std::hash
namespace std {
    template<>
    struct hash<Gossamer::position_type>
        : public Gossamer::position_type::Hash
    {
    };
}


#endif // RANKSELECT_HH
