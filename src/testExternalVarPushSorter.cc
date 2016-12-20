// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "ExternalVarPushSorter.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <random>

#include "StringFileFactory.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestExternalVarPushSorter
#include "testBegin.hh"


class Checker
{
public:
    vector<uint64_t>::const_iterator curr;
    vector<uint64_t>::const_iterator end;

    void push_back(uint64_t x)
    {
        BOOST_CHECK(curr != end);
        BOOST_CHECK_EQUAL(*curr, x);
        ++curr;
    }

    void done()
    {
        BOOST_CHECK(curr == end);
    }

    Checker(const vector<uint64_t>& xs)
        : curr(xs.begin()), end(xs.end())
    {
    }
};

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    ExternalVarPushSorter<uint64_t> sorter(fac, 1000);

    std::mt19937 rng(19);
    std::uniform_real_distribution<> dist;

    vector<uint64_t> xs;
    for (uint64_t i = 0; i < 10000; ++i)
    {
        uint64_t x = dist(rng) * (1ULL << 56);
        sorter.push_back(x);
        xs.push_back(x);
    }

    sort(xs.begin(), xs.end());

    Checker c(xs);
    sorter.sort(c);
    c.done();
}

#include "testEnd.hh"
