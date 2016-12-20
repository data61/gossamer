// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "SparseArrayView.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <random>

using namespace boost;
using namespace std;
using namespace Gossamer;

#define GOSS_TEST_MODULE TestSparseArrayView
#include "testBegin.hh"

class BitmapRemover
{
public:
    bool valid() const
    {
        return mCurr < mBitmap.size();
    }

    uint64_t operator*() const
    {
        BOOST_ASSERT(mBitmap[mCurr]);
        return mCurr;
    }

    void operator++()
    {
        ++mCurr;
        if (valid())
        {
            scan();
        }
    }

    BitmapRemover(const dynamic_bitset<>& pBitmap)
        : mBitmap(pBitmap), mCurr(0)
    {
        scan();
    }

private:
    void scan()
    {
        while (mCurr < mBitmap.size() && !mBitmap[mCurr])
        {
            ++mCurr;
        }
    }

    const dynamic_bitset<>& mBitmap;
    uint64_t mCurr;
};

#if 1
BOOST_AUTO_TEST_CASE(simple)
{
    const uint64_t N = 516;
    const uint64_t M = N;
    StringFileFactory fac;
    {
        SparseArray::Builder b("x", fac, position_type(N), M);

        for (uint64_t i = 0; i < M; ++i)
        {
            b.push_back(position_type(N*i/M));
        }
        b.end(position_type(N));
    }

    SparseArray a("x", fac);

    SparseArrayView v(a);

    BOOST_CHECK_EQUAL(a.count(), v.count());
    for (uint64_t i = 0; i < a.count(); ++i)
    {
        BOOST_CHECK_EQUAL(a.select(i), v.select(i));
    }
    for (position_type i(0); i < a.size(); ++i)
    {
        BOOST_CHECK_EQUAL(a.rank(i), v.rank(i));
    }

    {
        dynamic_bitset<> bs;
        BitmapRemover itr(bs);
        v.remove(itr);

        BOOST_CHECK_EQUAL(a.count(), v.count());
        for (uint64_t i = 0; i < a.count(); ++i)
        {
            BOOST_CHECK_EQUAL(a.select(i), v.select(i));
        }
        for (position_type i(0); i < a.size(); ++i)
        {
            BOOST_CHECK_EQUAL(a.rank(i), v.rank(i));
        }
    }

    {
        dynamic_bitset<> bs(v.count());

        {
            SparseArray::Builder bld("y", fac, position_type(N), a.count());
            for (uint64_t i = 0; i < v.count(); ++i)
            {
                if (i % 2)
                {
                    bs[i] = true;
                }
                else
                {
                    bld.push_back(v.select(i));
                }
            }
            bld.end(position_type(N));
        }

        for (uint64_t i = 0; i < v.count(); ++i)
        {
        }

        BitmapRemover itr(bs);
        v.remove(itr);

        SparseArray b("y", fac);

        BOOST_CHECK_EQUAL(b.count(), v.count());
        for (uint64_t i = 0; i < b.count(); ++i)
        {
            BOOST_CHECK_EQUAL(b.select(i), v.select(i));
        }
        for (position_type i(0); i < b.size(); ++i)
        {
            BOOST_CHECK_EQUAL(b.rank(i), v.rank(i));
        }
    }

    {
        dynamic_bitset<> bs(v.count());

        {
            SparseArray::Builder bld("y", fac, position_type(N), a.count());
            for (uint64_t i = 0; i < v.count(); ++i)
            {
                if (i % 2)
                {
                    bs[i] = true;
                }
                else
                {
                    bld.push_back(v.select(i));
                }
            }
            bld.end(position_type(N));
        }

        BitmapRemover itr(bs);
        v.remove(itr);

        SparseArray b("y", fac);

        BOOST_CHECK_EQUAL(b.count(), v.count());
        for (uint64_t i = 0; i < b.count(); ++i)
        {
            BOOST_CHECK_EQUAL(b.select(i), v.select(i));
        }
        for (position_type i(0); i < b.size(); ++i)
        {
            BOOST_CHECK_EQUAL(b.rank(i), v.rank(i));
        }
    }

}
#endif

#if 0
BOOST_AUTO_TEST_CASE(test1)
{
    const uint64_t N = 10000;
    StringFileFactory fac;
    {
        SparseArray::Builder b("x", fac, position_type(1ULL << 63), N * 0.1);

        mt19937 rng(17);
        exponential_distribution<> dist(1.0 / 1024.0);

        position_type p(0);
        for (uint64_t i = 0; i < N; ++i)
        {
            p += position_type(dist(rng) + 1);
            //cout << lexical_cast<string>(p) << endl;
            b.push_back(p);
        }
        p += position_type(1);
        b.end(p);
    }

    SparseArray a("x", fac);

    SparseArrayView v(a);

    BOOST_CHECK_EQUAL(a.count(), v.count());
    for (uint64_t i = 0; i < a.count(); ++i)
    {
        BOOST_CHECK_EQUAL(a.select(i), v.select(i));
    }

    {
        dynamic_bitset<> bs;
        BitmapRemover itr(bs);
        v.remove(itr);

        BOOST_CHECK_EQUAL(a.count(), v.count());
        for (uint64_t i = 0; i < a.count(); ++i)
        {
            BOOST_CHECK_EQUAL(a.select(i), v.select(i));
            position_type p = v.select(i);
            BOOST_CHECK_EQUAL(v.rank(p), i);
        }
    }

    {
        dynamic_bitset<> bs(v.count());

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        {
            SparseArray::Builder bld("y", fac, a.select(a.count() - 1), a.count());
            for (uint64_t i = 0; i < v.count(); ++i)
            {
                if (dist(rng) < 0.01)
                {
                    bs[i] = true;
                }
                else
                {
                    bld.push_back(v.select(i));
                }
            }
            bld.end(a.select(a.count() - 1));
        }

        BitmapRemover itr(bs);
        v.remove(itr);

        SparseArray b("y", fac);

        BOOST_CHECK_EQUAL(b.count(), v.count());
        for (uint64_t i = 0; i < b.count(); ++i)
        {
            BOOST_CHECK_EQUAL(b.select(i), v.select(i));
            position_type p = v.select(i);
            BOOST_CHECK_EQUAL(v.rank(p), i);
        }
    }

    {
        dynamic_bitset<> bs(v.count());

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        {
            SparseArray::Builder bld("y", fac, a.select(a.count() - 1), a.count());
            for (uint64_t i = 0; i < v.count(); ++i)
            {
                if (dist(rng) < 0.01)
                {
                    bs[i] = true;
                }
                else
                {
                    bld.push_back(v.select(i));
                }
            }
            bld.end(a.select(a.count() - 1));
        }

        BitmapRemover itr(bs);
        v.remove(itr);

        SparseArray b("y", fac);

        BOOST_CHECK_EQUAL(b.count(), v.count());
        for (uint64_t i = 0; i < b.count(); ++i)
        {
            BOOST_CHECK_EQUAL(b.select(i), v.select(i));
            position_type p = v.select(i);
            BOOST_CHECK_EQUAL(v.rank(p), i);
        }
    }

}
#endif

#include "testEnd.hh"
