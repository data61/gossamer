// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing SimpleHashSet.
 *
 */

#include "SimpleHashSet.hh"
#include <vector>
#include <set>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestSimpleHashSet
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(testEmpty)
{
    SimpleHashSet<uint64_t> x;
    BOOST_CHECK_EQUAL(x.count(1), false);
    BOOST_CHECK_EQUAL(x.count(12), false);
    BOOST_CHECK_EQUAL(x.count(123), false);
    BOOST_CHECK_EQUAL(x.count(1234), false);
}

BOOST_AUTO_TEST_CASE(testEasy)
{
    SimpleHashSet<uint64_t> x;
    x.insert(1);
    x.insert(12);
    x.insert(123);
    x.insert(1234);
    BOOST_CHECK_EQUAL(x.count(1), true);
    BOOST_CHECK_EQUAL(x.count(12), true);
    BOOST_CHECK_EQUAL(x.count(123), true);
    BOOST_CHECK_EQUAL(x.count(1234), true);
}

BOOST_AUTO_TEST_CASE(testEasyWithEnsure)
{
    SimpleHashSet<uint64_t> x;
    x.ensure(1);
    x.ensure(12);
    x.ensure(123);
    x.ensure(1234);
    BOOST_CHECK_EQUAL(x.count(1), true);
    BOOST_CHECK_EQUAL(x.count(12), true);
    BOOST_CHECK_EQUAL(x.count(123), true);
    BOOST_CHECK_EQUAL(x.count(1234), true);
    x.ensure(1);
    x.ensure(12);
    x.ensure(123);
    x.ensure(1234);
    BOOST_CHECK_EQUAL(x.count(1), true);
    BOOST_CHECK_EQUAL(x.count(12), true);
    BOOST_CHECK_EQUAL(x.count(123), true);
    BOOST_CHECK_EQUAL(x.count(1234), true);
}

BOOST_AUTO_TEST_CASE(testBigger)
{
    mt19937 rng(19);
    uniform_real_distribution<> dist;

    set<uint64_t> xs;
    SimpleHashSet<uint64_t> x;
    uint64_t z = 0;
    for (uint64_t i = 0; i < 1000; ++i)
    {
        uint64_t y = 1024ULL * 1024ULL * 1024ULL * dist(rng);
        z += y + 1;
        while (xs.count(z - 1) || xs.count(z + 1))
        {
            ++z;
        }
        xs.insert(z);
        x.insert(z);
    }
    for (set<uint64_t>::const_iterator i = xs.begin(); i != xs.end(); ++i)
    {
        BOOST_CHECK_EQUAL(x.count(*i - 1), false);
        BOOST_CHECK_EQUAL(x.count(*i), true);
        BOOST_CHECK_EQUAL(x.count(*i + 1), false);
    }
}

// Test for Bug ID #172.
BOOST_AUTO_TEST_CASE(testExactlyFullTable)
{
    SimpleHashSet<uint8_t> x(64);
    for (uint8_t i = 0; i < 64; ++i)
    {
        x.insert(i);
    }
    BOOST_CHECK(!x.count((uint8_t)100));
}

#include "testEnd.hh"
