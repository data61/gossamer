// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "EnumerativeCode.hh"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <boost/static_assert.hpp>

#include "Utils.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestEnumerativeCode
#include "testBegin.hh"

EnumerativeCode<64> s_ec;

void
testBitPattern(uint64_t bitPattern)
{
    uint32_t bits = Gossamer::popcnt(bitPattern);
    uint64_t numCodes = s_ec.numCodes(bits);
    uint64_t code = s_ec.encode(bits, bitPattern);
    BOOST_CHECK(code < numCodes);
    uint64_t decode = s_ec.decode(bits, code);
    BOOST_CHECK_EQUAL(bitPattern, decode);
}

BOOST_AUTO_TEST_CASE(combinations)
{
    for (uint64_t n = 0; n <= 3; ++n)
    {
        BOOST_CHECK_EQUAL(s_ec.choose(n, 0), 1);
        BOOST_CHECK_EQUAL(s_ec.choose(n, n), 1);
        for (uint64_t k = 1; k < n; ++k)
        {
            BOOST_CHECK_EQUAL(s_ec.choose(n, k), s_ec.choose(n-1,k-1) + s_ec.choose(n-1,k));
        }
    }
}

BOOST_AUTO_TEST_CASE(numcodebits)
{
    for (uint64_t n = 0; n <= 64; ++n)
    {
        BOOST_CHECK_EQUAL(Gossamer::log2(s_ec.numCodes(n)), s_ec.numCodeBits(n));
    }
}

BOOST_AUTO_TEST_CASE(bitpatterns)
{
    for (uint64_t i = 0; i < 0xfff; ++i)
    {
        testBitPattern(i);
        testBitPattern(~i);
        for (uint64_t j = 12; j < 64; ++j)
        {
            testBitPattern(i | (1ull << j));
            testBitPattern(~(i | (1ull << j)));
        }
    }
}

#include "testEnd.hh"
