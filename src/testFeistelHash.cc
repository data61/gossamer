// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing FeistelHash.
 *
 */

#include "FeistelHash.hh"
#include <vector>
#include <random>
#include <thread>


#define GOSS_TEST_MODULE TestFeistelHash
#include "testBegin.hh"

using namespace boost;
using namespace std;

BOOST_AUTO_TEST_CASE(testMutant)
{
    typedef FeistelHash::Item Item;
    Item x(0, 0);
    Item y = FeistelHash::hash(x);
    Item z = FeistelHash::unhash(y);
    BOOST_CHECK_EQUAL(x.first, z.first);
    BOOST_CHECK_EQUAL(x.second, z.second);
}

BOOST_AUTO_TEST_CASE(test4)
{
    std::mt19937 rng(19);
    std::uniform_int_distribution<> dist(0, 1 << 24);

    static const uint64_t N = 1024 * 1024;
    for (uint64_t i = 0; i < N; ++i)
    {
        typedef FeistelHash::Item Item;
        Item x(dist(rng), dist(rng));
        Item y = FeistelHash::hash(x);
        Item z = FeistelHash::unhash(y);
        BOOST_CHECK_EQUAL(x.first, z.first);
        BOOST_CHECK_EQUAL(x.second, z.second);
    }
}

#include "testEnd.hh"

