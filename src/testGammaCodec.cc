// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "GammaCodec.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGammaCodec
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(test1a)
{
    uint64_t x = 1;
    uint64_t w = 0;
    uint64_t l = GammaCodec::encode(x, w);
    BOOST_CHECK_EQUAL(w, 1);
    BOOST_CHECK_EQUAL(l, 1);
    uint64_t y = GammaCodec::decode(w);
    BOOST_CHECK_EQUAL(x, y);
    BOOST_CHECK_EQUAL(w, 0);
}

BOOST_AUTO_TEST_CASE(test1b)
{
    uint64_t x = 2;
    uint64_t w = 0;
    uint64_t l = GammaCodec::encode(x, w);
    BOOST_CHECK_EQUAL(w, 2);
    BOOST_CHECK_EQUAL(l, 3);
    uint64_t y = GammaCodec::decode(w);
    BOOST_CHECK_EQUAL(x, y);
    BOOST_CHECK_EQUAL(w, 0);
}

BOOST_AUTO_TEST_CASE(test1c)
{
    uint64_t x = 3;
    uint64_t w = 0;
    uint64_t l = GammaCodec::encode(x, w);
    BOOST_CHECK_EQUAL(w, 6);
    BOOST_CHECK_EQUAL(l, 3);
    uint64_t y = GammaCodec::decode(w);
    BOOST_CHECK_EQUAL(x, y);
    BOOST_CHECK_EQUAL(w, 0);
}

BOOST_AUTO_TEST_CASE(test1d)
{
    uint64_t x = 11693;
    uint64_t w = 0;
    uint64_t l = GammaCodec::encode(x, w);
    BOOST_CHECK_EQUAL(w, 57368576);
    BOOST_CHECK_EQUAL(l, 27);
    uint64_t y = GammaCodec::decode(w);
    BOOST_CHECK_EQUAL(x, y);
    BOOST_CHECK_EQUAL(w, 0);
}

BOOST_AUTO_TEST_CASE(test2)
{
    for (uint64_t i = 1; i < 1024ULL * 1024ULL; ++i)
    {
        uint64_t x = i;
        uint64_t w = 0;
        GammaCodec::encode(x, w);
        uint64_t y = GammaCodec::decode(w);
        BOOST_CHECK_EQUAL(x, y);
        BOOST_CHECK_EQUAL(w, 0);
    }
}

#include "testEnd.hh"
