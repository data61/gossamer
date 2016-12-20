// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "WordyBitVector.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestWordyBitVector
#include "testBegin.hh"

#if 1
BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    fac.out("x");
    WordyBitVector v("x", fac);
    BOOST_CHECK_EQUAL(v.words(), 0);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test2)
{
    StringFileFactory fac;
    {
        WordyBitVector::Builder b("x", fac);
        b.push(7);
        b.push(47);
        b.push(63);
        b.push(64);
        b.push(65);
        b.push(97);
        b.push(108);
        b.push(235);
        b.end();
    }
    
    WordyBitVector v("x", fac);

    //for (uint64_t i = 0; i < v.words() * WordyBitVector::wordBits; ++i)
    //{
    //    cerr << (v.get(i) ? '1' : '0');
    //}
    //cerr << endl;

    BOOST_CHECK_EQUAL(v.get(6), false);
    BOOST_CHECK_EQUAL(v.get(7), true);
    BOOST_CHECK_EQUAL(v.get(8), false);
    BOOST_CHECK_EQUAL(v.get(46), false);
    BOOST_CHECK_EQUAL(v.get(47), true);
    BOOST_CHECK_EQUAL(v.get(48), false);
    BOOST_CHECK_EQUAL(v.get(62), false);
    BOOST_CHECK_EQUAL(v.get(63), true);
    BOOST_CHECK_EQUAL(v.get(64), true);
    BOOST_CHECK_EQUAL(v.get(65), true);
    BOOST_CHECK_EQUAL(v.get(66), false);
    BOOST_CHECK_EQUAL(v.get(96), false);
    BOOST_CHECK_EQUAL(v.get(97), true);
    BOOST_CHECK_EQUAL(v.get(98), false);
    BOOST_CHECK_EQUAL(v.get(107), false);
    BOOST_CHECK_EQUAL(v.get(108), true);
    BOOST_CHECK_EQUAL(v.get(109), false);
    BOOST_CHECK_EQUAL(v.get(234), false);
    BOOST_CHECK_EQUAL(v.get(235), true);

    BOOST_CHECK_EQUAL(v.select1(25, 0), 47);
    BOOST_CHECK_EQUAL(v.select1(25, 1), 63);
    BOOST_CHECK_EQUAL(v.select1(0, 5), 97);
    BOOST_CHECK_EQUAL(v.select1(25, 5), 108);

    BOOST_CHECK_EQUAL(v.select0(0, 0), 0);
    BOOST_CHECK_EQUAL(v.select0(0, 1), 1);
    BOOST_CHECK_EQUAL(v.select0(45, 3), 49);
    BOOST_CHECK_EQUAL(v.select0(0, 11), 12);
    BOOST_CHECK_EQUAL(v.select0(62, 0), 62);
    BOOST_CHECK_EQUAL(v.select0(62, 1), 66);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test3)
{
    StringFileFactory fac;
    {
        WordyBitVector::Builder b("x", fac);
        b.push(2);
        b.push(3);
        b.push(5);
        b.push(7);
        b.push(8);
        b.push(10);
        b.push(12);
        b.push(14);
        b.push(15);
        b.push(17);
        b.end();
    }
    
    // Sane?
    WordyBitVector v("x", fac);

    BOOST_CHECK_EQUAL(v.select1(2, 2), 5);
    BOOST_CHECK_EQUAL(v.select1(2, 9), 17);
    //BOOST_CHECK_EQUAL(v.select1(2, 10), 18);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test4)
{
    static const uint64_t N = 1000000;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        WordyBitVector::Builder vb("v", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 1.0/70000.0;
            vb.push_backX(bit);
            bits[i] = bit;
        }
        vb.end();
    }

    WordyBitVector v("v", fac);

    for (uint64_t i = 0; i < N; ++i)
    {
        //cerr << "i = " << i << endl;
        BOOST_CHECK_EQUAL(v.get(i), bits[i]);
    }

    for (uint64_t i = 0; i < N - 201; i += 123)
    {
        uint64_t count = 0;
        for (uint64_t j = 0; j < 201; ++j)
        {
            BOOST_CHECK_EQUAL(count, v.popcountRange(i, i+j));
            count += v.get(i+j);
        }
    }

}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test5)
{
    static const uint64_t N = 1000000;
    StringFileFactory fac;
    vector<uint64_t> ones;
    {
        WordyBitVector::Builder vb("v", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 1.0/70000.0;
            vb.push_backX(bit);
            if (bit)
            {
                ones.push_back(i);
            }
        }
        vb.end();
    }

    WordyBitVector v("v", fac);

    WordyBitVector::Iterator1 itr = v.iterator1();
    for (uint64_t i = 0; i < ones.size(); ++i, ++itr)
    {
        BOOST_CHECK(itr.valid());
        BOOST_CHECK_EQUAL(*itr, ones[i]);
    }
    BOOST_CHECK(!itr.valid());
}
#endif

#include "testEnd.hh"
