// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "BigInteger.hh"
#include "GossamerException.hh"
#include <string>
#include <algorithm>

using namespace std;

namespace
{
    static const char *sLiteralDigitsLc = "0123456789abcdef";
    // static const char *sLiteralDigitsUc = "0123456789ABCDEF";
}

#if 0
    // Multiplication by a small integer.  Required for radix conversion.
    void multiplyBySmallInteger(word_type pRhs)
    {
        word_type carry = 0;

        for (uint64_t i = 0; i < sNumWords; ++i)
        {
            double_word_type product = double_word_type(mWords[i]) * pRhs + carry;
            mWords[i] = static_cast<word_type>(product);
            carry = static_cast<word_type>(product >> sBitsPerWord);
        }

        adjustTopWord();
        return *this;
    }
#endif

void
BigIntegerBase::writeDecimal(ostream& pIO, word_type* pWords,
                             uint64_t pNumWords)
{
    ostream::sentry s(pIO);
    if (!s)
    {
        pIO.setstate(iostream::failbit);
        return;
    }

    const ios_base::fmtflags flags = pIO.flags();
    streamsize width = pIO.width();

    // Long enough to hold hex, dec and octal representations.
    streamsize bufsize = max<streamsize>(width, pNumWords * sizeof(word_type) * 5);
    char* cs = static_cast<char*>(alloca(bufsize)) + bufsize;

    // Convert to literal chars
    streamsize cslen = 0;
    if ((flags & ios_base::basefield) == ios_base::hex
        || (flags & ios_base::basefield) == ios_base::oct)
    {
        // This can be accomplished by bit manipulation alone, but it's NYI.
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("BigInteger currently only supports decimal radix"));
    }
    else
    {
        const word_type radix = 10;
        int highWord = pNumWords;
        while (highWord > 0 || pWords[0])
        {
            while (highWord >= 0 && !pWords[highWord])
            {
                --highWord;
            }

#ifdef USE_UINT_128_T
            word_type remainder = 0;
            for (int64_t i = highWord; i >= 0; --i)
            {
                double_word_type dividend = (double_word_type(remainder) << sBitsPerWord) | pWords[i];
                pWords[i] = static_cast<word_type>(dividend / radix);
                remainder = static_cast<word_type>(dividend % radix);
            }
#else
            // since we can't use 128bit intrinsics, lets use 32 bit integers
            // this is not safe for big endians (or the other way round?)
            half_word_type* pHalfWords = (half_word_type*)( pWords );
            int i = highWord*2 + 1; // index into pWords32
            
            // the first half word maybe blank
            if( !pHalfWords[i] ) {
                --i;
            }

            half_word_type remainder = 0;
            for (; i >= 0; --i)
            {
                word_type dividend = ( word_type(remainder) << (sBitsPerWord/2) ) | pHalfWords[i];
                pHalfWords[i] = static_cast<half_word_type>(dividend / radix);
                remainder = static_cast<half_word_type>(dividend % radix);
            }
#endif


            *--cs = sLiteralDigitsLc[remainder];
            ++cslen;
        }
    }

    // Pad
    if (width > cslen)
    {
        streamsize pslen = width - cslen;
        char* ps = static_cast<char*>(alloca(pslen));
        char_traits<char>::assign(ps, pslen, pIO.fill());
        if ((flags & ios_base::adjustfield) == ios_base::left)
        {
            pIO.write(cs, cslen);
            pIO.write(ps, pslen);
        }
        else
        {
            pIO.write(ps, pslen);
            pIO.write(cs, cslen);
        }
        return;
    }
    pIO.fill(0);
    pIO.write(cs, cslen);
}


