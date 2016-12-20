// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "Utils.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestUtils
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(test1)
{
    BOOST_CHECK_EQUAL(Gossamer::log2(1), 0);
    BOOST_CHECK_EQUAL(Gossamer::log2(2), 1);
    BOOST_CHECK_EQUAL(Gossamer::log2(3), 2);
    BOOST_CHECK_EQUAL(Gossamer::log2(4), 2);
    BOOST_CHECK_EQUAL(Gossamer::log2(5), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(6), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(7), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(8), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(9), 4);
}

BOOST_AUTO_TEST_CASE(test2)
{
    uint64_t w = 0x5;
    BOOST_CHECK_EQUAL(Gossamer::select1(w, 0), 0);
    BOOST_CHECK_EQUAL(Gossamer::select1(w, 1), 2);
    for (uint64_t i = 0; i < 64; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::select1(0xFFFFFFFFFFFFFFFFull, i), i);
    }
    for (uint64_t i = 0; i < 32; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::select1(0x5555555555555555ull, i), 2*i+0);
        BOOST_CHECK_EQUAL(Gossamer::select1(0xAAAAAAAAAAAAAAAAull, i), 2*i+1);
    }
    for (uint64_t i = 0; i < 16; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::select1(0x1111111111111111ull, i), 4*i+0);
        BOOST_CHECK_EQUAL(Gossamer::select1(0x2222222222222222ull, i), 4*i+1);
        BOOST_CHECK_EQUAL(Gossamer::select1(0x4444444444444444ull, i), 4*i+2);
        BOOST_CHECK_EQUAL(Gossamer::select1(0x8888888888888888ull, i), 4*i+3);
    }
}

BOOST_AUTO_TEST_CASE(testClz)
{
    uint64_t x = 1ull << 63;
    for (uint64_t i = 0; i <= 64; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::count_leading_zeroes(x), i);
        x >>= 1;
    }
}


static void
check_bounds(int* b, int* e, int v)
{
    // The simplest way to test the binary search operations is to
    // see if they return the same thing as the platinum-iridium
    // implementation. Hopefully the C++ standard library is bug-free.

    auto lb = Gossamer::lower_bound(b, e, v);
    auto lb16 = Gossamer::tuned_lower_bound<int*,int,std::less<int>,16>(b, e, v, std::less<int>());
    auto lb200 = Gossamer::tuned_lower_bound<int*,int,std::less<int>,200>(b, e, v, std::less<int>());
    auto pilb = std::lower_bound(b, e, v);
    BOOST_CHECK_EQUAL(lb, pilb);
    BOOST_CHECK_EQUAL(lb16, pilb);
    BOOST_CHECK_EQUAL(lb200, pilb);

    auto ub = Gossamer::upper_bound(b, e, v);
    auto ub16 = Gossamer::tuned_upper_bound<int*,int,std::less<int>,16>(b, e, v, std::less<int>());
    auto ub200 = Gossamer::tuned_upper_bound<int*,int,std::less<int>,200>(b, e, v, std::less<int>());
    auto piub = std::upper_bound(b, e, v);
    BOOST_CHECK_EQUAL(ub, piub);
    BOOST_CHECK_EQUAL(ub16, piub);
    BOOST_CHECK_EQUAL(ub200, piub);
}

BOOST_AUTO_TEST_CASE(testUpperLowerBound)
{
    int c[100];
    for (unsigned i = 0; i < 100; ++i)
    {
        c[i] = i;
    }

    check_bounds(&c[0], &c[0], -1);
    check_bounds(&c[0], &c[0], 0);
    check_bounds(&c[0], &c[0], 100);

    check_bounds(&c[0], &c[100], -1);
    check_bounds(&c[0], &c[100], 0);
    check_bounds(&c[0], &c[100], 3);
    check_bounds(&c[0], &c[100], 29);
    check_bounds(&c[0], &c[100], 99);
    check_bounds(&c[0], &c[100], 100);

    for (unsigned i = 0; i < 100; ++i)
    {
        c[i] = (i & ~(int)1) + 1;
    }

    check_bounds(&c[0], &c[100], -1);
    check_bounds(&c[0], &c[100], 0);
    check_bounds(&c[0], &c[100], 1);
    check_bounds(&c[0], &c[100], 2);
    check_bounds(&c[0], &c[100], 3);
    check_bounds(&c[0], &c[100], 29);
    check_bounds(&c[0], &c[100], 49);
    check_bounds(&c[0], &c[100], 50);
    check_bounds(&c[0], &c[100], 51);
    check_bounds(&c[0], &c[100], 52);
}

#include "testEnd.hh"
