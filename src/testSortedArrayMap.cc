// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing SortedArrayMap.
 *
 */

#include "SortedArrayMap.hh"
#include <vector>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestSortedArrayMap
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(testEmpty)
{
    SortedArrayMap<uint64_t,uint64_t> x;
    BOOST_CHECK(x.find(1) == x.end());
    BOOST_CHECK(x.find(12) == x.end());
    BOOST_CHECK(x.find(123) == x.end());
    BOOST_CHECK(x.find(1234) == x.end());
}

BOOST_AUTO_TEST_CASE(testEasy)
{
    SortedArrayMap<uint64_t,uint64_t> x;
    typedef SortedArrayMap<uint64_t,uint64_t>::value_type value_type;
    x.insert(value_type(1,10));
    x.insert(value_type(12,120));
    x.insert(value_type(123,1230));
    x.insert(value_type(1234,12340));

    BOOST_CHECK_EQUAL(x.at(1), 10);
    BOOST_CHECK_EQUAL(x.at(12), 120);
    BOOST_CHECK_EQUAL(x.at(123), 1230);
    BOOST_CHECK_EQUAL(x.at(1234), 12340);

    BOOST_CHECK(x.find(1) != x.end());
    BOOST_CHECK(x.find(12) != x.end());
    BOOST_CHECK(x.find(123) != x.end());
    BOOST_CHECK(x.find(1234) != x.end());

    BOOST_CHECK_EQUAL(x.find(1)->first, 1);
    BOOST_CHECK_EQUAL(x.find(12)->first, 12);
    BOOST_CHECK_EQUAL(x.find(123)->first, 123);
    BOOST_CHECK_EQUAL(x.find(1234)->first, 1234);

    BOOST_CHECK_EQUAL(x.find(1)->second, 10);
    BOOST_CHECK_EQUAL(x.find(12)->second, 120);
    BOOST_CHECK_EQUAL(x.find(123)->second, 1230);
    BOOST_CHECK_EQUAL(x.find(1234)->second, 12340);
}

BOOST_AUTO_TEST_CASE(testBigger)
{
    mt19937 rng(19);
    uniform_real_distribution<> dist;

    map<uint64_t,uint64_t> xs;
    SortedArrayMap<uint64_t,uint64_t> x;
    typedef SortedArrayMap<uint64_t,uint64_t>::value_type value_type;

    uint64_t z = 0;
    for (uint64_t i = 0; i < 1000; ++i)
    {
        uint64_t y = 1024ULL * 1024ULL * 1024ULL * dist(rng);
        uint64_t v = 1024ULL * 1024ULL * 1024ULL * dist(rng);
        z += y + 1;
        xs[z] = v;
        x.insert(value_type(z,v));
    }

    for (map<uint64_t,uint64_t>::const_iterator i = xs.begin();
         i != xs.end();++i)
    {
        BOOST_CHECK_EQUAL(x.at((*i).first), (*i).second);
        BOOST_CHECK_EQUAL(x[(*i).first], (*i).second);
    }

    for (SortedArrayMap<uint64_t,uint64_t>::iterator i = x.begin();
         i != x.end(); ++i)
    {
        BOOST_CHECK_EQUAL(xs.at((*i).first), (*i).second);
    }
}

#include "testEnd.hh"
