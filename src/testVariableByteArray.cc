// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "VariableByteArray.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestVariableByteArray
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    {
        VariableByteArray::Builder b("x", fac, 100, 0.1);

        b.push_back(0);
        b.push_back(1);
        b.push_back(2);
        b.push_back(3);
        b.push_back(4);
        b.push_back(254);
        b.push_back(255);
        b.push_back(256);
        b.push_back(257);
        b.push_back(1);
        b.push_back(2);
        b.push_back(3);
        b.push_back(65535);
        b.push_back(65536);
        b.push_back(3);
        b.push_back(65535);

        b.end();
    }

    VariableByteArray a("x", fac);

    BOOST_CHECK_EQUAL(a[0], 0);
    BOOST_CHECK_EQUAL(a[1], 1);
    BOOST_CHECK_EQUAL(a[2], 2);
    BOOST_CHECK_EQUAL(a[3], 3);
    BOOST_CHECK_EQUAL(a[4], 4);
    BOOST_CHECK_EQUAL(a[5], 254);
    BOOST_CHECK_EQUAL(a[6], 255);
    BOOST_CHECK_EQUAL(a[7], 256);
    BOOST_CHECK_EQUAL(a[8], 257);
    BOOST_CHECK_EQUAL(a[9], 1);
    BOOST_CHECK_EQUAL(a[10], 2);
    BOOST_CHECK_EQUAL(a[11], 3);
    BOOST_CHECK_EQUAL(a[12], 65535);
    BOOST_CHECK_EQUAL(a[13], 65536);
    BOOST_CHECK_EQUAL(a[14], 3);
    BOOST_CHECK_EQUAL(a[15], 65535);
}

BOOST_AUTO_TEST_CASE(test2)
{
    const uint64_t N = 10000ull;
    StringFileFactory fac;
    std::vector<VariableByteArray::value_type> values;
    values.reserve(N);
    {
        VariableByteArray::Builder b("x", fac, N, 0.01);

        std::mt19937 rng(209);
        std::uniform_int_distribution<> dist(0,70000);

        for (uint64_t i = 0; i < N; ++i)
        {
            VariableByteArray::value_type v = dist(rng);
            values.push_back(v);
            b.push_back(v);
        }
        b.end();
    }

    VariableByteArray a("x", fac);

    for (uint64_t i = 0; i < N; ++i)
    {
        //cerr << i << endl;
        BOOST_CHECK_EQUAL(a[i], values[i]);
    }
}

BOOST_AUTO_TEST_CASE(test3)
{
    const uint64_t N = 1000ull;
    StringFileFactory fac;
    std::vector<VariableByteArray::value_type> values;
    values.reserve(N);
    {
        VariableByteArray::Builder b("x", fac, N, 0.01);

        std::mt19937 rng(209);
        std::uniform_int_distribution<> dist(0,70000);

        for (uint64_t i = 0; i < N; ++i)
        {
            VariableByteArray::value_type v = dist(rng);
            values.push_back(v);
            b.push_back(v);
        }
        b.end();
    }

    VariableByteArray a("x", fac);

    VariableByteArray::Iterator itr(a.iterator());
    for (uint64_t i = 0; i < N; ++i, ++itr)
    {
        //cerr << i << '\t' << values[i] << '\t' << a[i] << '\t' << (*itr) << endl;
        BOOST_CHECK(itr.valid());
        BOOST_CHECK_EQUAL(values[i], *itr);
    }
    BOOST_CHECK(!itr.valid());
}

BOOST_AUTO_TEST_CASE(test4)
{
    const uint64_t N = 100000ull;
    StringFileFactory fac;
    std::vector<VariableByteArray::value_type> values;
    values.reserve(N);
    {
        VariableByteArray::Builder b("x", fac, N, 0.01);

        std::mt19937 rng(209);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            double x = dist(rng);
            uint64_t y = x * x * x * 1024 * 1024 * 16;
            VariableByteArray::value_type v = y;
            values.push_back(v);
            b.push_back(v);
        }
        b.end();
    }

    VariableByteArray a("x", fac);

    VariableByteArray::Iterator itr(a.iterator());
    for (uint64_t i = 0; i < N; ++i, ++itr)
    {
        //cerr << i << '\t' << values[i] << '\t' << a[i] << '\t' << (*itr) << endl;
        BOOST_CHECK(itr.valid());
        BOOST_CHECK_EQUAL(values[i], *itr);
        BOOST_CHECK_EQUAL(values[i], a[i]);
    }
    BOOST_CHECK(!itr.valid());
}

#include "testEnd.hh"
