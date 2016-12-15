// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef RUNLENGTHCODEDSET_HH
#define RUNLENGTHCODEDSET_HH

#ifndef RUNLENGTHCODEDBITVECTORWORD_HH
#include "RunLengthCodedBitVectorWord.hh"
#endif

#ifndef DELTACODEC_HH
#include "DeltaCodec.hh"
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

class RunLengthCodedSet
{
public:
    typedef RunLengthCodedBitVectorWord<DeltaCodec> Codec;
    static const uint64_t N = 2;

    /**
     * Return the size of the set - i.e. the number of bit positions/
     */
    uint64_t size() const
    {
        uint64_t s = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            s += Codec::size(mWords[i]);
        }
        return s;
    }

    /**
     * Return the number of set bits in the set.
     */
    uint64_t count() const
    {
        uint64_t c = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            c += Codec::count(mWords[i]);
        }
        return c;
    }

    /**
     * Return the rank of the given position within the set.
     */
    uint64_t rank(uint64_t pPos) const
    {
        uint64_t s = 0;
        uint64_t c = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t s0 = Codec::size(mWords[i]);
            if (s + s0 > pPos)
            {
                return c + Codec::rank(mWords[i], pPos - s);
            }
            s += s0;
            c += Codec::count(mWords[i]);
        }
        return c;
    }

    /**
     * Return the position of the pRnk'th 1 bit in the set.
     */
    uint64_t select(uint64_t pRnk) const
    {
        uint64_t s = 0;
        uint64_t c = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t c0 = Codec::count(mWords[i]);
            if (c + c0 > pRnk)
            {
                return s + Codec::select(mWords[i], pRnk - c);
            }
            s += Codec::size(mWords[i]);
            c += c0;
        }
        throw "gak";
    }

    /**
     * Add a 1 to the set. The position pX must be byond the end
     * of the set.
     */
    void append(uint64_t pX)
    {
        BOOST_ASSERT(pX >= size());
        uint64_t w = 0;
        uint64_t s = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t z = Codec::size(mWords[i]);
            if (z == 0)
            {
                w = i;
                break;
            }
            s += z;
        }
        if (w > 0)
        {
            --w;
        }
        BOOST_ASSERT(pX >= s);
        uint64_t x = pX - s;
        // Append 0s from the end of the set up to the 1.
        if (x > 0)
        {
            uint64_t carry = Codec::append(mWords[w], x, false);
            if (carry)
            {
                ++w;
                if (w >= N)
                {
                    throw "gak";
                }
                BOOST_ASSERT(w < N);
                mWords[w] = carry;
            }
        }
        
        // Append the 1.
        uint64_t carry = Codec::append(mWords[w], 1, true);
        if (carry)
        {
            ++w;
            if (w >= N)
            {
                throw "gak";
            }
            BOOST_ASSERT(w < N);
            mWords[w] = carry;
        }
    }

    /**
     * Return the number of runs of 1 bits in the set.
     */
    uint64_t countRanges() const
    {
        uint64_t z = count();
        if (z == 0)
        {
            return 0;
        }
        uint64_t rs = 1;
        uint64_t p = select(0);
        for (uint64_t i = 1; i < z; ++i)
        {
            uint64_t p0 = select(i);
            if (p + 1 < p0)
            {
                rs++;
            }
            p = p0;
        }
        return rs;
    }

    /**
     * Return the number of bits used by the encoding of the set.
     */
    uint64_t bits() const
    {
        uint64_t b = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t z = Codec::bits(mWords[i]);
            if (z > 0)
            {
                b = 64 * i + z;
            }
        }
        return b;
    }

    void dump(std::ostream& pOut) const
    {
        for (uint64_t i = 0; i < N; ++i)
        {
            Codec::dump(mWords[i], pOut);
        }
    }

    RunLengthCodedSet()
    {
        for (uint64_t i = 0; i < N; ++i)
        {
            mWords[i] = 0;
        }
    }

private:
    uint64_t mWords[N];
};

#endif // RUNLENGTHCODEDSET_HH
