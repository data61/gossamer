// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "DenseArray.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestDenseArray
#include "testBegin.hh"

#if 1
BOOST_AUTO_TEST_CASE(bug_over_256)
{
    uint64_t N = 516ull;
    StringFileFactory fac;
    {
        DenseArray::Builder b("v", fac);

        for (uint64_t i = 0; i < N; ++i)
        {
            b.push_back(i);
        }
        b.end(N);
    }

    DenseArray da("v", fac);

    uint64_t ones = 0;
    const uint64_t z = da.size();
    //for (uint64_t i = 0; i < z; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < z; ++i)
    {
        BOOST_CHECK_EQUAL(da.rank(i), ones);
        if (da.access(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(da.select(ones), i);
            ++ones;
        }
    }
    BOOST_CHECK_EQUAL(ones, da.count());

    for (uint64_t i = 0; i < ones - 197; i += 113)
    {
        for (uint64_t j = 1; j < 197; ++j)
        {
            std::pair<uint64_t,uint64_t> s2 = da.select(i, i+j);
            BOOST_CHECK_EQUAL(da.select(i), s2.first);
            BOOST_CHECK_EQUAL(da.select(i+j), s2.second);

            std::pair<uint64_t,uint64_t> r2 = da.rank(i, i+j);
            BOOST_CHECK_EQUAL(da.rank(i), r2.first);
            BOOST_CHECK_EQUAL(da.rank(i+j), r2.second);
        }
    }

}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test1)
{
    uint64_t N = 100000ull;
    StringFileFactory fac;
    {
        DenseArray::Builder b("v", fac);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            if (dist(rng) > 0.5)
            {
                b.push_back(i);
            }
        }
        b.end(N);
    }

    DenseArray da("v", fac);

    uint64_t ones = 0;
    const uint64_t z = da.size();
    //for (uint64_t i = 0; i < z; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < z; ++i)
    {
        BOOST_CHECK_EQUAL(da.rank(i), ones);
        if (da.access(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(da.select(ones), i);
            ++ones;
        }
    }
    BOOST_CHECK_EQUAL(ones, da.count());

    for (uint64_t i = 0; i < ones - 197; i += 113)
    {
        for (uint64_t j = 1; j < 197; ++j)
        {
            std::pair<uint64_t,uint64_t> s2 = da.select(i, i+j);
            BOOST_CHECK_EQUAL(da.select(i), s2.first);
            BOOST_CHECK_EQUAL(da.select(i+j), s2.second);

            std::pair<uint64_t,uint64_t> r2 = da.rank(i, i+j);
            BOOST_CHECK_EQUAL(da.rank(i), r2.first);
            BOOST_CHECK_EQUAL(da.rank(i+j), r2.second);
        }
    }

}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test2)
{
    static const uint64_t N = 1000000;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 1.0/70000.0;
            bits[i] = bit;
            vb.push_backX(bit);
            if (bit)
            {
                //cerr << "setting bit: " << i << endl;
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    for (uint64_t i = 0; i < N; ++i)
    {
        //cerr << i << endl;
        BOOST_CHECK_EQUAL(v.get(i), bits[i]);
        //if (v.get(i))
        //{
        //    cout << " " << i;
        //}
    }
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test3)
{
    static const uint64_t N = 100000;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.999;
            vb.push_backX(bit);
            if (bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test4)
{
    static const uint64_t N = 20;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, true);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) > 0.5;
            vb.push_backX(bit);
            if (!bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, true);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (!v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test5)
{
    static const uint64_t N = 100000;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, true);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) > 0.5;
            vb.push_backX(bit);
            if (!bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, true);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    //for (uint64_t i = 0; i < N && i < 75; ++i)
    //{
    //    cerr << (v.get(i) ? '1' : '0');
    //}
    //cerr << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (!v.get(i))
        {
            //cerr << "I = " << i << endl;
            //cerr << "ones = " << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test6)
{
    static const uint64_t N = 20;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        vb.push(0); b.push_back(0);
        vb.push(1); b.push_back(1);
        vb.push(4); b.push_back(4);
        vb.push(6); b.push_back(6);
        vb.push(8); b.push_back(8);
        vb.push(10); b.push_back(10);
        vb.push(12); b.push_back(12);
        vb.push(15); b.push_back(15);
        vb.push(17); b.push_back(17);
        vb.push(18); b.push_back(18);

        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    //for (uint64_t i = 0; i < N && i < 75; ++i)
    //{
    //    cerr << (v.get(i) ? '1' : '0');
    //}
    //cerr << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << "I = " << i << endl;
            //cerr << "ones = " << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test_one_in_10)
{
    static const uint64_t N = 1000000;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.1;
            vb.push_backX(bit);
            if (bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }

    for (uint64_t i = 0; i < ones - 197; i += 113)
    {
        for (uint64_t j = 1; j < 197; ++j)
        {
            std::pair<uint64_t,uint64_t> s2 = a.select(i, i+j);
            BOOST_CHECK_EQUAL(a.select(i), s2.first);
            BOOST_CHECK_EQUAL(a.select(i+j), s2.second);
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test_one_in_100)
{
    static const uint64_t N = 1000000;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.01;
            vb.push_backX(bit);
            if (bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }

    for (uint64_t i = 0; i < ones - 197; i += 113)
    {
        for (uint64_t j = 1; j < 197; ++j)
        {
            std::pair<uint64_t,uint64_t> s2 = a.select(i, i+j);
            BOOST_CHECK_EQUAL(a.select(i), s2.first);
            BOOST_CHECK_EQUAL(a.select(i+j), s2.second);
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test_one_in_1000)
{
    static const uint64_t N = 10000000;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.001;
            vb.push_backX(bit);
            if (bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }

    for (uint64_t i = 0; i < ones - 197; i += 113)
    {
        for (uint64_t j = 1; j < 197; ++j)
        {
            std::pair<uint64_t,uint64_t> s2 = a.select(i, i+j);
            BOOST_CHECK_EQUAL(a.select(i), s2.first);
            BOOST_CHECK_EQUAL(a.select(i+j), s2.second);
        }
    }
}
#endif


#if 1
BOOST_AUTO_TEST_CASE(test_one_in_10000)
{
    static const uint64_t N = 10000000;
    StringFileFactory fac;
    {
        WordyBitVector::Builder vb("v", fac);
        DenseSelect::Builder b("x", fac, false);

        std::mt19937 rng(17);
        std::uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.0001;
            vb.push_backX(bit);
            if (bit)
            {
                b.push_back(i);
            }
        }
        vb.end();
        b.end();
    }

    WordyBitVector v("v", fac);
    DenseSelect a(v, "x", fac, false);

    uint64_t ones = 0;
    //for (uint64_t i = 0; i < N; ++i)
    //{
    //    cout << (v.get(i) ? '1' : '0');
    //}
    //cout << endl;
    for (uint64_t i = 0; i < N; ++i)
    {
        if (v.get(i))
        {
            //cerr << ones << endl;
            BOOST_CHECK_EQUAL(a.select(ones), i);
            ++ones;
        }
    }

    for (uint64_t i = 0; i < ones - 197; i += 113)
    {
        for (uint64_t j = 1; j < 197; ++j)
        {
            std::pair<uint64_t,uint64_t> s2 = a.select(i, i+j);
            BOOST_CHECK_EQUAL(a.select(i), s2.first);
            BOOST_CHECK_EQUAL(a.select(i+j), s2.second);
        }
    }

}
#endif

#include "testEnd.hh"
