// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing SimpleRangeSet.
 *
 */

#include "SimpleRangeSet.hh"
#include <vector>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestSimpleRangeSet
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(testEmpty)
{
    SimpleRangeSet x;
    BOOST_CHECK_EQUAL(x.size(), 0);
    BOOST_CHECK_EQUAL(x.count(), 0);
}

BOOST_AUTO_TEST_CASE(test1)
{
    SimpleRangeSet x1(10, 20);
    BOOST_CHECK_EQUAL(x1.size(), 10);
    BOOST_CHECK_EQUAL(x1.count(), 1);
    SimpleRangeSet x2(15, 25);
    BOOST_CHECK_EQUAL(x2.size(), 10);
    BOOST_CHECK_EQUAL(x2.count(), 1);

    SimpleRangeSet x3 = x2;
    x3 &= x1;
    BOOST_CHECK_EQUAL(x3.size(), 5);
    BOOST_CHECK_EQUAL(x3.count(), 1);
    BOOST_CHECK_EQUAL(x3[0].first, 15);
    BOOST_CHECK_EQUAL(x3[0].second, 20);
}

BOOST_AUTO_TEST_CASE(test2)
{
    SimpleRangeSet lhs;
    lhs |= SimpleRangeSet(69, 72);
    lhs |= SimpleRangeSet(9083, 9084);
    SimpleRangeSet rhs;
    rhs |= SimpleRangeSet(72, 74);
    rhs |= SimpleRangeSet(9082, 9084);
    SimpleRangeSet in = lhs;
    in &= rhs;
    BOOST_CHECK_EQUAL(in.count(), 1);
    BOOST_CHECK_EQUAL(in[0].first, 9083);
    BOOST_CHECK_EQUAL(in[0].second, 9084);
}

BOOST_AUTO_TEST_CASE(test3)
{
    SimpleRangeSet x;
    x |= SimpleRangeSet(1234, 1235);
    x |= SimpleRangeSet(1235, 1236);
    BOOST_CHECK_EQUAL(x.count(), 1);
}

BOOST_AUTO_TEST_CASE(test4)
{
    SimpleRangeSet ex(16, 18);
    SimpleRangeSet en;
    en |= SimpleRangeSet(904, 905);
    en |= SimpleRangeSet(957, 958);
    en |= SimpleRangeSet(1157, 1167);

    ex -= en;

    BOOST_CHECK_EQUAL(ex.size(), 2);
    BOOST_CHECK_EQUAL(ex.count(), 1);
    BOOST_CHECK_EQUAL(ex[0].first, 16);
    BOOST_CHECK_EQUAL(ex[0].second, 18);
}

BOOST_AUTO_TEST_CASE(test5)
{
    SimpleRangeSet lhs(16, 21);
    SimpleRangeSet rhs;
    rhs |= SimpleRangeSet(16, 18);
    rhs |= SimpleRangeSet(19, 21);

    lhs &= rhs;

    BOOST_CHECK_EQUAL(lhs.size(), 4);
    BOOST_CHECK_EQUAL(lhs.count(), 2);
    BOOST_CHECK_EQUAL(lhs[0].first, 16);
    BOOST_CHECK_EQUAL(lhs[0].second, 18);
    BOOST_CHECK_EQUAL(lhs[1].first, 19);
    BOOST_CHECK_EQUAL(lhs[1].second, 21);
}

BOOST_AUTO_TEST_CASE(test6)
{
    SimpleRangeSet lhs(16, 21);
    SimpleRangeSet rhs;
    rhs |= SimpleRangeSet(16, 18);
    rhs |= SimpleRangeSet(19, 21);

    rhs &= lhs;

    BOOST_CHECK_EQUAL(rhs.size(), 4);
    BOOST_CHECK_EQUAL(rhs.count(), 2);
    BOOST_CHECK_EQUAL(rhs[0].first, 16);
    BOOST_CHECK_EQUAL(rhs[0].second, 18);
    BOOST_CHECK_EQUAL(rhs[1].first, 19);
    BOOST_CHECK_EQUAL(rhs[1].second, 21);
}

#include "testEnd.hh"
