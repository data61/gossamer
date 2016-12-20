// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "RRRArray.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestRRRArray
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(testSecondSuperBlock)
{
    const uint64_t N = 491540;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        RRRRank::Builder b("x", fac);

        mt19937 rng(19);
        uniform_real_distribution<> dist;

        for (uint64_t i = 491520; i < N; ++i)
        {
            bool bit = dist(rng) < 0.5;
            if (bit)
            {
                bits[i] = true;
                b.push_back(i);
            }
        }
        b.end(N);
    }

    RRRRank a("x", fac);

    for (uint64_t i = 491520; i < N; ++i)
    {
        if (a.access(i) != bits[i])
        {
            cerr << i << endl;
        }
        BOOST_CHECK_EQUAL(a.access(i), bits[i]);
        bits[i] = 0;
    }
    BOOST_CHECK_EQUAL(bits.count(), 0);
}

#if 1
BOOST_AUTO_TEST_CASE(test1)
{
    const uint64_t N = 1000;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        RRRRank::Builder b("x", fac);

        mt19937 rng(19);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < static_cast<double>(i)/static_cast<double>(N);
            if (bit)
            {
                bits[i] = true;
                b.push_back(i);
            }
        }
        b.end(N);
    }

    RRRRank a("x", fac);

    // for (uint64_t i = 0; i < N; ++i)
    // {
    //     cerr << (bits[i] ? '1' : '0');
    // }
    // cerr << endl;

    uint64_t ones = 0;
    for (uint64_t i = 0; i < N; ++i)
    {
        //cerr << i << endl;
        BOOST_CHECK_EQUAL(a.rank(i), ones);
        if (bits[i])
        {
            ++ones;
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test2)
{
    const uint64_t N = 2000;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        RRRArray::Builder b("x", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.1;
            if (bit)
            {
                // std::cerr << "Setting " << i << '\n';
                bits[i] = true;
                b.push_back(i);
            }
        }
        b.end(N);
    }

    RRRArray a("x", fac);

    // for (uint64_t i = 0; i < N; ++i)
    // {
    //     cerr << (bits[i] ? '1' : '0');
    // }
    // cerr << endl;

    //cerr << "bits.count() == " << bits.count() << endl;

    // a.debug(N);

    uint64_t ones = 0;

    for (uint64_t i = 0; i < N; ++i)
    {
        bool bitsi = bits[i];
        //cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(i), bitsi);

        BOOST_CHECK_EQUAL(a.rank(i), ones);
        uint64_t r;
        BOOST_CHECK_EQUAL(a.accessAndRank(i, r), bitsi);
        BOOST_CHECK_EQUAL(r, ones);

        if (bitsi)
        {
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif




#if 1
BOOST_AUTO_TEST_CASE(test3)
{
    const uint64_t N = 2000;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        RRRArray::Builder b("x", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.05;
            if (bit)
            {
                // std::cerr << "Setting " << i << '\n';
                bits[i] = true;
                b.push_back(i);
            }
        }
        b.end(N);
    }

    RRRArray a("x", fac);

    // for (uint64_t i = 0; i < N; ++i)
    // {
    //     cerr << (bits[i] ? '1' : '0');
    // }
    // cerr << endl;

    //cerr << "bits.count() == " << bits.count() << endl;

    // a.debug(N);

    uint64_t ones = 0;

    for (uint64_t i = 0; i < N; ++i)
    {
        bool bitsi = bits[i];
        //cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(i), bitsi);

        BOOST_CHECK_EQUAL(a.rank(i), ones);
        uint64_t r;
        BOOST_CHECK_EQUAL(a.accessAndRank(i, r), bitsi);
        BOOST_CHECK_EQUAL(r, ones);

        if (bitsi)
        {
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test4)
{
    const uint64_t N = 20000;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        RRRArray::Builder b("x", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.001;
            if (bit)
            {
                // std::cerr << "Setting " << i << '\n';
                bits[i] = true;
                b.push_back(i);
            }
        }
        b.end(N);
    }

    RRRArray a("x", fac);

    // for (uint64_t i = 0; i < N; ++i)
    // {
    //     cerr << (bits[i] ? '1' : '0');
    // }
    // cerr << endl;

    //cerr << "bits.count() == " << bits.count() << endl;

    // a.debug(N);

    uint64_t ones = 0;

    for (uint64_t i = 0; i < N; ++i)
    {
        bool bitsi = bits[i];
        //cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(i), bitsi);

        BOOST_CHECK_EQUAL(a.rank(i), ones);
        uint64_t r;
        BOOST_CHECK_EQUAL(a.accessAndRank(i, r), bitsi);
        BOOST_CHECK_EQUAL(r, ones);

        if (bitsi)
        {
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif

BOOST_AUTO_TEST_CASE(test5)
{
    const uint64_t N = 250;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        RRRRank::Builder b("x", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.05;
            if (bit)
            {
                // std::cerr << "Setting " << i << '\n';
                bits[i] = true;
                b.push_back(i);
            }
        }
        b.end(N);
    }

    RRRRank a("x", fac);

    //for (uint64_t i = 0; i < bits.size(); ++ i)
    //{
    //    cerr << "01"[bits[i]];
    //}
    //cerr << endl;

    RRRRank::Iterator itr(a.iterator());
    while (itr.valid())
    {
        //cerr << *itr << endl;
        BOOST_CHECK(bits[*itr]);
        bits[*itr] = false;
        ++itr;
    }
    BOOST_CHECK_EQUAL(bits.count(), 0);
}

#include "testEnd.hh"
