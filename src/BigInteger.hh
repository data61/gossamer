// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/** \file
 * A restricted implementation of unsigned big integers.
 *
 * \note if the maximum BigInteger is MAX_BIG_INTEGER then by definition
 *  -# MAX_BIG_INTEGER + 1 == 0
 *  -# MAX_BIG_INTEGER == 0 - 1
 *
 */

#ifndef BIGINTEGER_HH
#define BIGINTEGER_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif // STDINT_H

#ifndef STD_CSTRING
#include <cstring>
#define STD_CSTRING
#endif // STD_CSTRING

#ifndef STD_LIMITS
#include <limits>
#define STD_LIMITS
#endif // STD_LIMITS

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif // STD_IOSTREAM

#ifndef STD_UNORDERED_SET
#include <unordered_set>
#define STD_UNORDERED_SET
#endif

#ifndef BOOST_STATIC_ASSERT_HPP
#include <boost/static_assert.hpp>
#define BOOST_STATIC_ASSERT_HPP
#endif // BOOST_STATIC_ASSERT_HPP

#ifndef UTILS_HH
#include "Utils.hh"
#endif // UTILS_HH

#ifndef NUMERIC_CONVERSION_UTIL_HH
#include "NumericConversionUtil.hh"
#endif // NUMERIC_CONVERSION_UTIL_HH

#define DO_NOT_USE_GCC_EXTENSIONS_IN_BIGINTEGER

#if !defined(__GNUC__) || defined(DO_NOT_USE_GCC_EXTENSIONS_IN_BIGINTEGER)
#undef USE_UINT_128_T
#else
#define USE_UINT_128_T
#endif


/**
 * A brief description of the class goes here.
 *
 * The number of words for the representation is either the minimal number
 * of machine words required to represent Bits, or the number required to
 * store 64 bits, whichever is larger.  This ensures that we can always
 * place a uint64_t in the storage space without overflowing the bounds.
 * It is okay if Bits < 64, because extra bits will be masked off.
 */

class BigIntegerBase
{
protected:
    typedef uint64_t word_type;
#ifdef USE_UINT_128_T
    typedef __uint128_t double_word_type;
#else
    typedef uint32_t half_word_type;
#endif

    typedef std::numeric_limits<word_type> word_type_limits;

    BOOST_STATIC_ASSERT(word_type_limits::is_specialized
        && !word_type_limits::is_signed
        && word_type_limits::radix == 2);

    static const uint64_t sBitsPerWord = word_type_limits::digits;

    static void writeDecimal(std::ostream& pIO, word_type* pWords,
                             uint64_t pNumWords);

    static void readDecimal(std::istream& pIO, word_type* pWords,
                            uint64_t pNumWords);

#ifndef USE_UINT_128_T
    /**
     *  Full adder
     *  \return the carry
    */
    static inline uint64_t add128(uint64_t pA, uint64_t pB, uint64_t pCarryIn, uint64_t& pResult)
    {
        pResult = pA + pB + pCarryIn;
        // Want to return the carry. But probably easier to think
        // about the condition when there's no carry:
        //   pResult > pA, then hasn't overflowed and thus no carry
        //   (pB | pCarryIn) == 0, adding zero can't overflow
        // then we should reutrn:  !( (pResult > pA) || ((pB | pCarryIn) == 0) ) 
        // simplify we get the following:
        return (pResult <= pA) && (pB | pCarryIn);
    }
#endif
};


/**
 * Unsigned big integer support.
 *
 * Only the operations essential for supporting positions in large bitmaps are supported.
 * 
 *  \param Words The number of 64-bit words.
 */
template<int Words>
class BigInteger
    : public BigIntegerBase
{
private:
    BOOST_STATIC_ASSERT(Words > 0);

public:
    static const uint64_t sWords = Words;
    static const uint64_t sBits = sBitsPerWord * sWords;

    BigInteger()
    {
        clear();
    }

    explicit BigInteger(uint64_t pRhs)
    {
        clear();
        mWords[0] = static_cast<word_type>(pRhs);
    }

    bool boolean_test() const
    {
        word_type test(0);
        for (int64_t i = 0; i < Words; ++i)
        {
            test |= mWords[i];
        }
        return test != 0;
    }

    bool fitsIn64Bits() const
    {
        word_type test(0);
        for (int64_t i = 1; i < Words; ++i)
        {
            test |= mWords[i];
        }
        return test == 0;
    }

    uint64_t asUInt64() const
    {
        return mWords[0];
    }

    uint64_t mostSigWord() const
    {
        return mWords[Words - 1];
    }

    double asDouble() const
    {
        double scale = static_cast<double>(std::numeric_limits<word_type>::max()) + 1;
        double value = 0;
        for (int64_t i = Words - 1; i >= 0; --i)
        {
            value = value * scale + mWords[i];
        }
        return value;
    }

    /// Reverse two-bit sequences.
    void reverse()
    {
        for (int64_t i = 0; i < Words/2; ++i)
        {
            word_type tmp = Gossamer::rev(mWords[i]);
            mWords[i] = Gossamer::rev(mWords[Words - i - 1]);
            mWords[Words - i - 1] = tmp;
        }
    }

    /// Reverse complement.
    void reverseComplement(uint64_t pK)
    {
        for (int64_t i = 0; i < Words/2; ++i)
        {
            word_type tmp = Gossamer::rev(~mWords[i]);
            mWords[i] = Gossamer::rev(~mWords[Words - i - 1]);
            mWords[Words - i - 1] = tmp;
        }
        if (Words & 1)
        {
            mWords[Words/2] = Gossamer::rev(~mWords[Words/2]);
        }
        *this >>= sBits - 2*pK;
    }

    BigInteger& operator+=(uint64_t pRhs)
    {
#ifdef USE_UINT_128_T
        double_word_type carry = pRhs;

        for (int64_t i = 0; i < Words; ++i)
        {
            double_word_type sum = double_word_type(mWords[i]) + carry;
            mWords[i] = static_cast<word_type>(sum);
            carry = sum >> sBitsPerWord;
        }
#else
        // NOTE: this only works for 64bit words
        uint64_t carry = pRhs;
        uint64_t sum;
        for (int64_t i = 0; i < Words; ++i)
        {
            carry = add128( uint64_t(mWords[i]), 0, carry, sum);
            mWords[i] = static_cast<word_type>(sum);
        }
#endif
        return *this;
    }

    BigInteger& operator+=(const BigInteger& pRhs)
    {
#ifdef USE_UINT_128_T
        word_type carry = 0;

        for (int64_t i = 0; i < Words; ++i)
        {
            double_word_type sum = double_word_type(mWords[i]) + double_word_type(pRhs.mWords[i]) + carry;
            mWords[i] = static_cast<word_type>(sum);
            carry = static_cast<word_type>(sum >> sBitsPerWord);
        }
#else
        // NOTE: this only works for 64bit words
        uint64_t carry = 0;
        uint64_t sum;
        for (int64_t i = 0; i < Words; ++i)
        {
            carry = add128( uint64_t(mWords[i]), uint64_t(pRhs.mWords[i]), carry, sum);
            mWords[i] = static_cast<word_type>(sum);
        }
#endif
        return *this;
    }

    BigInteger& operator-=(uint64_t pRhs)
    {
#ifdef USE_UINT_128_T
        // a - b == a + ~b + 1
        double_word_type sum = double_word_type(mWords[0]) + double_word_type(~pRhs) + 1;
        mWords[0] = static_cast<word_type>(sum);
        word_type carry = static_cast<word_type>(sum >> sBitsPerWord);

        for (int64_t i = 1; i < Words; ++i)
        {
            double_word_type sum = double_word_type(mWords[i]) + double_word_type(~word_type(0)) + carry;
            mWords[i] = static_cast<word_type>(sum);
            carry = static_cast<word_type>(sum >> sBitsPerWord);
        }
#else
        // NOTE: this only works for 64bit words
        uint64_t carry = 1; // implement two's complement's +1 step
        uint64_t sum;
        carry = add128( uint64_t(mWords[0]), ~uint64_t(pRhs), carry, sum);
        mWords[0] = static_cast<word_type>(sum); // this should just drop the higher bits
        for (int64_t i = 1; i < Words; ++i)
        {
            // sign extend
            carry = add128( uint64_t(mWords[i]), ~uint64_t(0), carry, sum);
            mWords[i] = static_cast<word_type>(sum); // this should just drop the higher bits
        }
#endif

        return *this;
    }

    BigInteger& operator-=(const BigInteger& pRhs)
    {
#ifdef USE_UINT_128_T
        // a - b == a + ~b + 1
        word_type carry = 1;

        for (int64_t i = 0; i < Words; ++i)
        {
            double_word_type sum = double_word_type(mWords[i]) + double_word_type(~pRhs.mWords[i]) + carry;
            mWords[i] = static_cast<word_type>(sum);
            carry = static_cast<word_type>(sum >> sBitsPerWord);
        }
#else
        // NOTE: this only works for 64bit words
        uint64_t carry = 1; // implement two's complement's +1 step
        uint64_t sum;
        for (int64_t i = 0; i < Words; ++i)
        {
            carry = add128( uint64_t(mWords[i]), ~uint64_t(pRhs.mWords[i]), carry, sum);
            mWords[i] = static_cast<word_type>(sum); // this should just drop the higher bits
        }
#endif
        return *this;
    }

    BigInteger& operator<<=(uint64_t pShift)
    {
        if (pShift >= sBits)
        {
            clear();
            return *this;
        }

        int64_t wordShift = pShift / sBitsPerWord;
        if (wordShift > 0)
        {
            for (int64_t i = Words - wordShift - 1; i >= 0; --i)
            {
                mWords[i + wordShift] = mWords[i];
            }
            for (int64_t i = 0; i < wordShift; ++i)
            {
                mWords[i] = word_type(0);
            }
            pShift -= wordShift * sBitsPerWord;
        }

        if (pShift > 0)
        {
            word_type carry = 0;
            for (int64_t i = wordShift; i < Words; ++i)
            {
                word_type nextCarry = mWords[i] >> (sBitsPerWord - pShift);
                mWords[i] = (mWords[i] << pShift) | carry;
                carry = nextCarry;
            }
        }

        return *this;
    }

    BigInteger& operator>>=(uint64_t pShift)
    {
        if (pShift >= sBits)
        {
            clear();
            return *this;
        }

        int64_t wordShift = pShift / sBitsPerWord;
        if (wordShift > 0)
        {
            for (int64_t i = 0; i < Words - wordShift; ++i)
            {
                mWords[i] = mWords[i + wordShift];
            }
            for (int64_t i = Words - wordShift; i < Words; ++i)
            {
                mWords[i] = word_type(0);
            }
            pShift -= wordShift * sBitsPerWord;
        }

        if (pShift > 0)
        {
            word_type carry = 0;
            for (int64_t i = Words - wordShift - 1; i >= 0; --i)
            {
                word_type nextCarry = mWords[i] << (sBitsPerWord - pShift);
                    mWords[i] = (mWords[i] >> pShift) | carry;
                carry = nextCarry;
            }
        }

        return *this;
    }

    BigInteger& operator|=(uint64_t pRhs)
    {
        mWords[0] |= word_type(pRhs);
        return *this;
    }

    BigInteger& operator|=(const BigInteger& pRhs)
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            mWords[i] |= pRhs.mWords[i];
        }
        return *this;
    }

    BigInteger& operator&=(uint64_t pRhs)
    {
        uint64_t w = asUInt64() & pRhs;
        clear();
        mWords[0] = word_type(w);
        return *this;
    }

    uint64_t operator&(uint64_t pRhs) const
    {
        return asUInt64() & pRhs;
    }

    BigInteger& operator&=(const BigInteger& pRhs)
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            mWords[i] &= pRhs.mWords[i];
        }
        return *this;
    }

    BigInteger& operator^=(const BigInteger& pRhs)
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            mWords[i] ^= pRhs.mWords[i];
        }
        return *this;
    }

    BigInteger& operator~()
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            mWords[i] = ~mWords[i];
        }
        return *this;
    }

    BigInteger& operator++()
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            if (++mWords[i])
            {
                break;
            }
        }

        return *this;
    }

    BigInteger& operator--()
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            if (--mWords[i] != ~word_type(0))
            {
                break;
            }
        }

        return *this;
    }

    uint64_t log2() const
    {
        for (int64_t i = Words - 1; i >= 0; --i)
        {
            if (mWords[i])
            {
                return Gossamer::log2(mWords[i]) + i * sBitsPerWord;
            }
        }
        return Gossamer::log2(0);
    }

    bool operator==(const BigInteger& pRhs) const
    {
        for (int64_t i = 0; i < Words; ++i)
        {
            if (mWords[i] != pRhs.mWords[i])
            {
                return false;
            }
        }

        return true;
    }

    bool nonZero() const
    {
        word_type w = mWords[0];
        for (int64_t i = 1; i < Words; ++i)
        {
            w |= mWords[i];
        }
        return w != 0;
    }

    bool operator<(const BigInteger& pRhs) const
    {
        for (int64_t i = Words-1; i >= 0; --i)
        {
            if (mWords[i] < pRhs.mWords[i])
            {
                return true;
            }
            if (mWords[i] > pRhs.mWords[i])
            {
                return false;
            }
        }

        return false;
    }

    std::size_t hash() const
    {
        uint64_t seed = 14695981039346656037ULL;
        for (int64_t i = 0; i < Words; ++i)
        {
            seed = wordHash(mWords[i], seed);
        }
        return seed;
    }

    std::pair<const uint64_t*,const uint64_t*> words() const
    {
        return std::pair<const uint64_t*,const uint64_t*>(mWords, mWords + Words);
    }
    
    std::pair<uint64_t*,uint64_t*> words()
    {
        return std::pair<uint64_t*,uint64_t*>(mWords, mWords + Words);
    }
    
    friend std::ostream&
    operator<<(std::ostream& pStream, const BigInteger& pRhs)
    {
        BigInteger toWrite(pRhs);
        toWrite.writeDecimal(pStream, toWrite.mWords, BigInteger::sWords - 1);
        return pStream;
    }

#if 0
    friend class std::istream&
    operator>>(std::istream& pStream, BigInteger& pValue)
    {
        return pStream;
    }
#endif

private:
    word_type mWords[Words];    ///< Storage.

    void clear()
    {
        std::memset(mWords, 0, sizeof(mWords));
    }

    static uint64_t wordHash(uint64_t pWord, uint64_t pSeed)
    {
        uint64_t r = pSeed;
        for (uint64_t i = 0; i < sizeof(pWord); ++i)
        {
            r ^= pWord & 0xFFULL;
            pWord >>= 8;
            r *= 1099511628211ULL;
        }
        return r;
    }
};

// std::numeric_limits
namespace std {
    template<int Words>
    struct numeric_limits< BigInteger<Words> >
        : public UnsignedIntegralNumericLimits<
                BigInteger<Words>, BigInteger<Words>::sBits
          >
    {
    };
}

// std::hash
namespace std {
    template<int Words>
    struct hash< BigInteger<Words> >
    {
        std::size_t operator()(const BigInteger<Words>& pValue)
        {
            return pValue.hash();
        }
    };
}


#endif // BIGINTEGER_HH
