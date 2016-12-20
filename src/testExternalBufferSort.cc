// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "ExternalBufferSort.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <random>

#include "StringFileFactory.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestExternalBufferSort
#include "testBegin.hh"

typedef vector<uint8_t> Item;

class Checker
{
public:
    vector<Item>::const_iterator curr;
    vector<Item>::const_iterator last;

    void push_back(const Item& x)
    {
        BOOST_CHECK(curr != last);
        BOOST_CHECK(*curr == x);
        ++curr;
    }

    void done()
    {
        BOOST_CHECK(curr == last);
    }

    void end()
    {
    }

    Checker(const vector<Item>& xs)
        : curr(xs.begin()), last(xs.end())
    {
    }
};

BOOST_AUTO_TEST_CASE(test0)
{
    StringFileFactory fac;
    ExternalBufferSort sorter(1024, fac);

    std::mt19937 rng(19);
    std::uniform_real_distribution<> dist;

    vector<Item> xs;
    for (uint64_t i = 0; i < 10; ++i)
    {
        uint64_t x = dist(rng) * 24;
        Item itm;
        for (uint64_t j = 0; j < x; ++j)
        {
            itm.push_back(dist(rng) * 256);
        }
        sorter.push_back(itm);
        xs.push_back(itm);
    }

    sort(xs.begin(), xs.end());

    Checker c(xs);
    sorter.sort(c);
    c.done();
}

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    ExternalBufferSort sorter(1024, fac);

    std::mt19937 rng(19);
    std::uniform_real_distribution<> dist;

    vector<Item> xs;
    for (uint64_t i = 0; i < 8 * 1024; ++i)
    {
        uint64_t x = dist(rng) * 24;
        Item itm;
        for (uint64_t j = 0; j < x; ++j)
        {
            itm.push_back(dist(rng) * 256);
        }
        sorter.push_back(itm);
        xs.push_back(itm);
    }

    sort(xs.begin(), xs.end());

    Checker c(xs);
    sorter.sort(c);
    c.done();
}

#include "testEnd.hh"
