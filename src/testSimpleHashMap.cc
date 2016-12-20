// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing SimpleHashMap.
 *
 */

#include "SimpleHashMap.hh"
#include <vector>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestSimpleHashMap
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(testEmpty)
{
    SimpleHashMap<uint64_t,uint64_t> x;
    BOOST_CHECK_EQUAL(x.find(1), (uint64_t*)0);
    BOOST_CHECK_EQUAL(x.find(12), (uint64_t*)0);
    BOOST_CHECK_EQUAL(x.find(123), (uint64_t*)0);
    BOOST_CHECK_EQUAL(x.find(1234), (uint64_t*)0);
}

BOOST_AUTO_TEST_CASE(testEasy)
{
    SimpleHashMap<uint64_t,uint64_t> x;
    x.insert(1,10);
    x.insert(12,120);
    x.insert(123,1230);
    x.insert(1234,12340);
    BOOST_CHECK_EQUAL(*x.find(1), 10);
    BOOST_CHECK_EQUAL(*x.find(12), 120);
    BOOST_CHECK_EQUAL(*x.find(123), 1230);
    BOOST_CHECK_EQUAL(*x.find(1234), 12340);
}

BOOST_AUTO_TEST_CASE(testBigger)
{
    mt19937 rng(19);
    uniform_real_distribution<> dist;

    map<uint64_t,uint64_t> xs;
    SimpleHashMap<uint64_t,uint64_t> x;
    uint64_t z = 0;
    for (uint64_t i = 0; i < 1000; ++i)
    {
        uint64_t y = 1024ULL * 1024ULL * 1024ULL * dist(rng);
        uint64_t v = 1024ULL * 1024ULL * 1024ULL * dist(rng);
        z += y + 1;
        xs[z] = v;
        x.insert(z,v);
    }

    for (map<uint64_t,uint64_t>::const_iterator i = xs.begin(); i != xs.end(); ++i)
    {
        BOOST_CHECK_EQUAL(*x.find(i->first), i->second);
    }

    for (SimpleHashMap<uint64_t,uint64_t>::Iterator it(x); it.valid(); ++it)
    {
        pair<uint64_t,uint64_t> v = *it;
        BOOST_CHECK_EQUAL(xs[v.first], v.second);
    }
}

#include "testEnd.hh"
