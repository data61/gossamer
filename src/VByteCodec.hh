// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef VBYTECODEC_HH
#define VBYTECODEC_HH

#ifndef UTILS_HH
#include "Utils.hh"
#endif

#ifndef BOOST_ASSERT_HPP
#include <boost/assert.hpp>
#define BOOST_ASSERT_HPP
#endif

class VByteCodec
{
public:
    template <typename Dest>
    static void encode(const uint64_t& pItem, Dest& pDest)
    {
        if (pItem < 0x80)
        {
            pDest.push_back(static_cast<uint8_t>(pItem));
            return;
        }
        uint64_t x = pItem;
        uint64_t b = 64 - Gossamer::count_leading_zeroes(x);
        uint64_t v = b / 8; // number of whole bytes of payload
        uint64_t l = b % 8; // number of bits in the most significant partial byte of payload
        if (v + l + 1 <= 8)
        {
            // We can fit the l bits of the msb in the header byte
            uint8_t z = (x >> (8 *v)) | ~(static_cast<uint8_t>(-1) >> v);
            pDest.push_back(z);
        }
        else
        {
            if (l != 0)
            {
                // There is no room for the l bits of the msb
                // in the header, so we need an extra byte.
                ++v;
            }
            uint8_t z = ~(static_cast<uint8_t>(-1) >> v);
            pDest.push_back(z);
        }
        switch (v)
        {
            case 8:
            {
                uint8_t y = x >> 56;
                pDest.push_back(y);
            }
            case 7:
            {
                uint8_t y = x >> 48;
                pDest.push_back(y);
            }
            case 6:
            {
                uint8_t y = x >> 40;
                pDest.push_back(y);
            }
            case 5:
            {
                uint8_t y = x >> 32;
                pDest.push_back(y);
            }
            case 4:
            {
                uint8_t y = x >> 24;
                pDest.push_back(y);
            }
            case 3:
            {
                uint8_t y = x >> 16;
                pDest.push_back(y);
            }
            case 2:
            {
                uint8_t y = x >> 8;
                pDest.push_back(y);
            }
            case 1:
            {
                uint8_t y = x >> 0;
                pDest.push_back(y);
            }
            case 0:
            {
                break;
            }
            default:
            {
                BOOST_ASSERT(false);
            }
        }
    }

    template <typename Itr>
    static uint64_t decode(Itr& pItr, const Itr& pEnd)
    {
        BOOST_ASSERT(pItr != pEnd);

        uint8_t z = *pItr;
        ++pItr;
        if (z < 0x80)
        {
            return z;
        }

        // Count the number of leading 1s by sign-extending,
        // taking bitwise complement and counting leading zeros.
        int64_t x = static_cast<int8_t>(z);
        uint64_t n = Gossamer::count_leading_zeroes(~x);
        uint64_t r = static_cast<uint8_t>(x) & ((~0ULL) >> n);
        n -= 56;
        switch (n)
        {
            case 8:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 7:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 6:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 5:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 4:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 3:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 2:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 1:
            {
                BOOST_ASSERT(pItr != pEnd);
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 0:
            {
                break;
            }
            default:
            {
                BOOST_ASSERT(false);
            }
        }
        return r;
    }

    template <typename Itr>
    static uint64_t decode(Itr& pItr)
    {
        uint8_t z = *pItr;
        ++pItr;
        if (z < 0x80)
        {
            return z;
        }

        // Count the number of leading 1s by sign-extending,
        // taking bitwise complement and counting leading zeros.
        int64_t x = static_cast<int8_t>(z);
        uint64_t n = Gossamer::count_leading_zeroes(~x);
        uint64_t r = static_cast<uint8_t>(x) & ((~0ULL) >> n);
        n -= 56;
        switch (n)
        {
            case 8:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 7:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 6:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 5:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 4:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 3:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 2:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 1:
            {
                r = (r << 8) | *pItr;
                ++pItr;
            }
            case 0:
            {
                break;
            }
            default:
            {
                BOOST_ASSERT(false);
            }
        }
        return r;
    }
};

#endif // VBYTECODEC_HH
