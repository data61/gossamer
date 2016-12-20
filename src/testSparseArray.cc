// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "SparseArray.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <random>


using namespace boost;
using namespace std;
using namespace Gossamer;

#define GOSS_TEST_MODULE TestSparseArray
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test_termination)
{
    const uint64_t N = 257;
    StringFileFactory fac;
    {
        SparseArray::Builder b("x", fac, 8);
        b.end(position_type(N));
    }
    SparseArray a("x", fac);
    BOOST_CHECK_EQUAL(a.access(position_type(256)), false);
}

#if 1
BOOST_AUTO_TEST_CASE(test1)
{
    // const uint64_t N = 10000;
    const uint64_t N = 30;
    StringFileFactory fac;
    dynamic_bitset<> bits(N);
    {
        SparseArray::Builder b("x", fac, position_type(N), N * 0.1);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.1;
            if (bit)
            {
                bits[i] = true;
                b.push_back(position_type(i));
            }
        }
        b.end(position_type(N));
    }

    SparseArray a("x", fac);

    // for (uint64_t i = 0; i < N; ++i)
    // {
    //     cerr << (bits[i] ? '1' : '0');
    // }
    // cerr << endl;

    // cerr << "bits.count() == " << bits.count() << endl;

    rank_type ones(0);
    SparseArray::Iterator it = a.iterator();

    for (uint64_t i = 0; i < N; ++i)
    {
        bool bitsi = bits[i];
        position_type pos(i);

        // cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(pos), bitsi);

        BOOST_CHECK_EQUAL(a.rank(pos), ones);
        rank_type r;
        BOOST_CHECK_EQUAL(a.accessAndRank(pos, r), bitsi);
        BOOST_CHECK_EQUAL(r, ones);

        if (bitsi)
        {
            BOOST_CHECK(it.valid());
            BOOST_CHECK_EQUAL(*it, pos);
            BOOST_CHECK_EQUAL(a.select(ones), pos);
            ++ones;
            ++it;
        }
    }
    BOOST_CHECK(!it.valid());

    static const uint64_t closeRank = 10ull;
    for (uint64_t i = 0; i < N - closeRank; ++i)
    {
        position_type pos1(i);
        for (uint64_t j = 1; j < closeRank; ++j)
        {
            position_type pos2(i + j);
            rank_type rank1a = a.rank(pos1);
            rank_type rank1b = a.rank(pos2);
            std::pair<rank_type,rank_type> rank2 = a.rank(pos1, pos2);
            BOOST_CHECK_EQUAL(rank1a, rank2.first);
            BOOST_CHECK_EQUAL(rank1b, rank2.second);
        }
    }
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test2)
{
    const uint64_t N = 1000;
    StringFileFactory fac;
    rank_type m(0);
    {
        SparseArray::Builder b("x", fac, position_type(N), N * 0.01);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        for (uint64_t i = 0; i < N; ++i)
        {
            bool bit = dist(rng) < 0.01;
            if (bit)
            {
                b.push_back(position_type(i));
                ++m;
            }
        }
        b.end(position_type(N));
    }

    SparseArray a("x", fac);

    BOOST_ASSERT(a.rank(position_type(N)) == m);

    // Try and trigger a segv if the check isn't there.
    BOOST_ASSERT(a.rank(position_type(2*N)) == m);
    BOOST_ASSERT(a.rank(position_type(4*N)) == m);
    BOOST_ASSERT(a.rank(position_type(8*N)) == m);
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test3)
{
    uint64_t M = 120;
    SparseArray::position_type N(1);
    N <<= 64 + 8;

    StringFileFactory fac;
    std::vector<SparseArray::position_type> v;
    {
        SparseArray::Builder b("x", fac, position_type(N), rank_type(M));

        mt19937 rng(17);

        for (uint64_t i = 0; i < M; ++i)
        {
            SparseArray::position_type bitpos(i);
            bitpos <<= 64;
            bitpos |= static_cast<uint64_t>(rng()) << 32;
            bitpos |= static_cast<uint64_t>(rng());
            v.push_back(bitpos);
            b.push_back(position_type(bitpos));
        }
        b.end(position_type(N));
    }

    SparseArray a("x", fac);
    SparseArray::Iterator it = a.iterator();

    for (uint64_t i = 0; i < M; ++i)
    {
        position_type pos(v[i]);

        // cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(pos), true);

        BOOST_CHECK_EQUAL(a.rank(pos), rank_type(i));
        rank_type r;
        BOOST_CHECK_EQUAL(a.accessAndRank(pos, r), true);
        BOOST_CHECK_EQUAL(r, rank_type(i));

        BOOST_CHECK(it.valid());
        BOOST_CHECK_EQUAL(*it, pos);
        BOOST_CHECK_EQUAL(a.select(rank_type(i)), pos);
        ++it;
    }
    BOOST_CHECK(!it.valid());

}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test4)
{
    uint64_t M = 120;
    SparseArray::position_type N(1);
    N <<= 100;

    StringFileFactory fac;
    std::vector<SparseArray::position_type> v;
    {
        SparseArray::Builder b("x", fac, position_type(N), rank_type(M));

        mt19937 rng(17);

        for (uint64_t i = 0; i < M; ++i)
        {
            SparseArray::position_type bitpos(i);
            bitpos <<= 64;
            bitpos |= static_cast<uint64_t>(rng()) << 32;
            bitpos |= static_cast<uint64_t>(rng());
            bitpos <<= 28;
            bitpos |= static_cast<uint64_t>(rng()) & ((1ULL << 28) - 1);
            v.push_back(bitpos);
            b.push_back(position_type(bitpos));
        }
        b.end(position_type(N));
    }

    SparseArray a("x", fac);
    SparseArray::Iterator it = a.iterator();

    for (uint64_t i = 0; i < M; ++i)
    {
        position_type pos(v[i]);

        // cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(pos), true);

        BOOST_CHECK_EQUAL(a.rank(pos), rank_type(i));
        rank_type r;
        BOOST_CHECK_EQUAL(a.accessAndRank(pos, r), true);
        BOOST_CHECK_EQUAL(r, rank_type(i));
        BOOST_CHECK_EQUAL(a.select(rank_type(i)), pos);

        BOOST_CHECK(it.valid());
        BOOST_CHECK_EQUAL(*it, pos);
        ++it;
    }
    BOOST_CHECK(!it.valid());
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test5)
{
    uint64_t M = 120;
    SparseArray::position_type N(1);
    N <<= 100;

    StringFileFactory fac;
    std::vector<SparseArray::position_type> v;
    {
        SparseArray::Builder b("x", fac, position_type(N), rank_type(M));

        mt19937 rng(17);

        for (uint64_t i = 0; i < M; ++i)
        {
            SparseArray::position_type bitpos(i);
            bitpos <<= 64;
            bitpos |= static_cast<uint64_t>(rng()) << 32;
            bitpos |= static_cast<uint64_t>(rng());
            bitpos <<= 28;
            bitpos |= static_cast<uint64_t>(rng()) & ((1ULL << 28) - 1);
            v.push_back(bitpos);
            b.push_back(position_type(bitpos));
        }
        b.end(position_type(N));
    }

    SparseArray a("x", fac);
    SparseArray::LazyIterator lit("x", fac);

    for (uint64_t i = 0; i < M; ++i)
    {
        position_type pos(v[i]);

        // cerr << i << '\t' << ones << '\t' << bitsi << endl;
        BOOST_CHECK_EQUAL(a.access(pos), true);

        BOOST_CHECK_EQUAL(a.rank(pos), rank_type(i));
        rank_type r;
        BOOST_CHECK_EQUAL(a.accessAndRank(pos, r), true);
        BOOST_CHECK_EQUAL(r, rank_type(i));
        BOOST_CHECK_EQUAL(a.select(rank_type(i)), pos);

        BOOST_CHECK(lit.valid());
        BOOST_CHECK_EQUAL(*lit, pos);
        ++lit;

    }
    BOOST_CHECK(!lit.valid());
}
#endif

#include "testEnd.hh"
