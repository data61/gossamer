// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing BlendedSort.
 *
 */

#include "BlendedSort.hh"
#include <vector>
#include <random>
#include <math.h>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestBlendedSort
#include "testBegin.hh"


class Cmp
{
public:
    static const uint64_t zero()
    {
        return 0;
    }

    uint64_t radix(uint64_t pIdx) const
    {
        return static_cast<uint32_t>(floor(mItems[pIdx]));
    }

    bool operator()(const uint64_t& pLhs, const uint64_t& pRhs) const
    {
        return mItems[pLhs] < mItems[pRhs];
    }

    void show(std::ostream& pOut, const uint64_t& pIdx) const
    {
        pOut << mItems[pIdx];
    }

    Cmp(const vector<double>& pItems)
        : mItems(pItems)
    {
    }

private:
    const vector<double>& mItems;
};

BOOST_AUTO_TEST_CASE(testEmpty)
{
    std::vector<double> items;
    std::vector<uint64_t> perm;
    Cmp cmp(items);
    BlendedSort<uint64_t>::sort(1, perm, 32, cmp);
}

BOOST_AUTO_TEST_CASE(testOne)
{
    std::mt19937 rng(19);
    std::uniform_real_distribution<> dist;

    static const uint64_t N = 1024 * 1024;

    std::vector<double> items;
    std::vector<uint64_t> perm;
    for (uint64_t i = 0; i < N; ++i)
    {
        items.push_back(dist(rng) * N);
        perm.push_back(i);
    }

    Cmp cmp(items);
    BlendedSort<uint64_t>::sort(1, perm, 32, cmp);

    std::vector<double> x(items);
    std::sort(x.begin(), x.end());

    for (uint64_t i = 0; i < N; ++i)
    {
        BOOST_CHECK_EQUAL(x[i], items[perm[i]]);
    }
}

BOOST_AUTO_TEST_CASE(testTwo)
{
    std::mt19937 rng(19);
    std::uniform_real_distribution<> dist;

    static const uint64_t N = 1024 * 1024;

    std::vector<double> items;
    std::vector<uint64_t> perm;
    for (uint64_t i = 0; i < N; ++i)
    {
        items.push_back(dist(rng) * N);
        perm.push_back(i);
    }

    Cmp cmp(items);
    BlendedSort<uint64_t>::sort(1, perm, 27, cmp);

    std::vector<double> x(items);
    std::sort(x.begin(), x.end());

    for (uint64_t i = 0; i < N; ++i)
    {
        BOOST_CHECK_EQUAL(x[i], items[perm[i]]);
    }
}

#include "testEnd.hh"


