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

#include "BoundedQueue.hh"
#include "ThreadGroup.hh"
#include <vector>
#include <chrono>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestBoundedQueue
#include "testBegin.hh"


class Writer
{
public:
    void operator()()
    {
        mt19937 rng(mSeed);
        uniform_int_distribution<> dist(1, 10);

        for (uint64_t i = 1; i < 10000; ++i)
        {
            mQueue.put(i);

            std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
        }
        mQueue.finish();
    }

    Writer(BoundedQueue<uint64_t>& pQueue, uint64_t pSeed)
        : mQueue(pQueue), mSeed(pSeed)
    {
    }

private:
    BoundedQueue<uint64_t>& mQueue;
    uint64_t mSeed;
};

class Reader
{
public:
    void operator()()
    {
        mt19937 rng(mSeed);
        uniform_int_distribution<> dist(1, 10);

        uint64_t p = 0;
        uint64_t x = 0;
        while (mQueue.get(x))
        {
            BOOST_CHECK(p < x);

            std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
        }
    }

    Reader(BoundedQueue<uint64_t>& pQueue, uint64_t pSeed)
        : mQueue(pQueue), mSeed(pSeed)
    {
    }

private:
    BoundedQueue<uint64_t>& mQueue;
    uint64_t mSeed;
};

BOOST_AUTO_TEST_CASE(testEmpty)
{
    BoundedQueue<uint64_t> q(10);
    Writer w(q, 19);
    Reader r(q, 23);
    ThreadGroup g;
    g.create(r);
    g.create(w);
    g.join();
}

#include "testEnd.hh"
