// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "CompactDynamicBitVector.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestCompactDynamicBitVector
#include "testBegin.hh"


static void make(const char* s, CompactDynamicBitVector& v)
{
    for (uint64_t i = 0; s[i]; ++i)
    {
        switch (s[i])
        {
            case '0':
            {
                v.insert(i, false);
                break;
            }
            case '1':
            {
                v.insert(i, true);
                break;
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test0)
{
    CompactDynamicBitVector t;
    BOOST_CHECK_EQUAL(t.size(), 0);
    BOOST_CHECK_EQUAL(t.count(), 0);
    t.insert(0, true);
    BOOST_CHECK_EQUAL(t.size(), 1);
    BOOST_CHECK_EQUAL(t.count(), 1);
}

BOOST_AUTO_TEST_CASE(test1)
{
    static const uint64_t N = 65536;

    mt19937 rng(17);
    uniform_real_distribution<> dist;

    CompactDynamicBitVector t;
    uint64_t c = 0;
    for (uint64_t i = 0; i < N; ++i)
    {
        uint64_t pos = dist(rng) * i;
        bool b = dist(rng) > 0.5;
        t.insert(pos, b);
        if (b)
        {
            c++;
        }
        BOOST_CHECK_EQUAL(t.size(), i + 1);
        BOOST_CHECK_EQUAL(t.count(), c);
    }
}

BOOST_AUTO_TEST_CASE(test2a)
{
    static const char* s =
        "11101111000100101100111100101001101100101011011001"
        "10101001100100011010010110011000001011010000110001"
        "11101100111100000110000011000110111100000010001100"
        "11101110101000110101101011110110100000011100001010"
        "11001111001010001010010010010111110010110111110001"
        "11101011110010010011011111001010110111111100010100"
        "00010100011100111010100101000100000011100001010110"
        "101101001111100110010011011";

    CompactDynamicBitVector t;
    make(s, t);
    BOOST_CHECK_EQUAL(t.size(), 377);
    BOOST_CHECK_EQUAL(t.count(), 190);

    t.erase(350);

    BOOST_CHECK_EQUAL(t.size(), 376);
    BOOST_CHECK_EQUAL(t.count(), 189);
}

BOOST_AUTO_TEST_CASE(test2)
{
    static const uint64_t N = 65536;

    mt19937 rng(17);
    uniform_real_distribution<> dist;

    CompactDynamicBitVector t;
    uint64_t c = 0;
    uint64_t z = 0;
    for (uint64_t i = 0; i < N; ++i)
    {
#if 0
        if (i >= 1870)
        {
            BOOST_CHECK_EQUAL(t.size(), z);
            BOOST_CHECK_EQUAL(t.count(), c);
            cerr << "size = " << z << endl;
            cerr << "count = " << c << endl;
            
            for (uint64_t j = 0; j < t.size(); ++j)
            {
                cerr << "01"[t.access(j)];
            }
            cerr << endl;
        }
#endif
        BOOST_CHECK_EQUAL(t.size(), z);
        BOOST_CHECK_EQUAL(t.count(), c);
        uint64_t pos = dist(rng) * z;
        bool op = dist(rng) > 0.4;
        if (t.size() == 0 || op)
        {
            bool b = dist(rng) > 0.5;
            t.insert(pos, b);
#if 0
            if (i >= 1870)
            {
                cerr << "inserting " << "01"[b] << " @ " << pos << endl;
            }
#endif
            if (b)
            {
                c++;
            }
            ++z;
        }
        else
        {
#if 0
            if (i >= 1870)
            {
                cerr << "removing " << "01"[t.access(pos)] << " @ " << pos << endl;
            }
#endif
            if (t.access(pos))
            {
                --c;
            }
            t.erase(pos);
            --z;
        }
        BOOST_CHECK_EQUAL(t.size(), z);
        BOOST_CHECK_EQUAL(t.count(), c);
    }
}

#include "testEnd.hh"
