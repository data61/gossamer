// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "BitVecSet.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestBitVecSet
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(test1)
{
    BitVecSet bvs;

    BOOST_CHECK_EQUAL(bvs.size(), 0);

    bvs.insert(BitVecSet::VecNum(0));
    BOOST_CHECK_EQUAL(bvs.size(), 1);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(0)), 0);
    BOOST_CHECK_EQUAL(bvs.count(BitVecSet::VecNum(0)), 0);

    bvs.insert(BitVecSet::VecNum(0), BitVecSet::VecPos(0), 1);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(0)), 1);
    BOOST_CHECK_EQUAL(bvs.count(BitVecSet::VecNum(0)), 1);
    BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(0), BitVecSet::VecPos(0)), 1);

    bvs.insert(BitVecSet::VecNum(0), BitVecSet::VecPos(1), 0);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(0)), 2);
    BOOST_CHECK_EQUAL(bvs.count(BitVecSet::VecNum(0)), 1);
    BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(0), BitVecSet::VecPos(0)), 1);
    BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(0), BitVecSet::VecPos(1)), 0);

    bvs.insert(BitVecSet::VecNum(0), BitVecSet::VecPos(1), 1);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(0)), 3);
    BOOST_CHECK_EQUAL(bvs.count(BitVecSet::VecNum(0)), 2);
    BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(0), BitVecSet::VecPos(0)), 1);
    BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(0), BitVecSet::VecPos(1)), 1);
    BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(0), BitVecSet::VecPos(2)), 0);
}

BOOST_AUTO_TEST_CASE(test2)
{
    BitVecSet bvs;
    bvs.insert(BitVecSet::VecNum(0));
    bvs.insert(BitVecSet::VecNum(1));
    bvs.insert(BitVecSet::VecNum(1), BitVecSet::VecPos(0), 1);
    bvs.insert(BitVecSet::VecNum(2));
    bvs.insert(BitVecSet::VecNum(3));

    BOOST_CHECK_EQUAL(bvs.size(), 4);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(0)), 0);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(1)), 1);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(2)), 0);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(3)), 0);

    //bvs.dump(cerr);
    bvs.erase(BitVecSet::VecNum(2));
    //bvs.dump(cerr);

    BOOST_CHECK_EQUAL(bvs.size(), 3);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(0)), 0);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(1)), 1);
    BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(2)), 0);
}

#if 1
BOOST_AUTO_TEST_CASE(test3)
{
    std::mt19937 rng(19);
    std::uniform_real_distribution<> dist;

    BitVecSet bvs;
    vector<dynamic_bitset<> > ref;

    for (uint64_t i = 0; i < 100; ++i)
    {
        bvs.insert(BitVecSet::VecNum(i));
        ref.push_back(dynamic_bitset<>());
    }

    for (uint64_t i = 0; i < 1000; ++i)
    {
        //bvs.dump(cerr);
        BOOST_CHECK_EQUAL(bvs.size(), ref.size());
        for (uint64_t j = 0; j < ref.size(); ++j)
        {
            BOOST_CHECK_EQUAL(bvs.size(BitVecSet::VecNum(j)), ref[j].size());
            uint64_t z = ref[j].size();
            for (uint64_t k = 0; k < z; ++k)
            {
                BOOST_CHECK_EQUAL(bvs.access(BitVecSet::VecNum(j), BitVecSet::VecPos(k)), ref[j][k]);
            }
        }

        if (dist(rng) < 0.5)
        {
            uint64_t x = dist(rng) * (ref.size() + 1);

            if (x < ref.size())
            {
                uint64_t z = bvs.size(BitVecSet::VecNum(x));
                uint64_t y = dist(rng) * (z + 1);
                bool b = dist(rng) < 0.5;

                //cerr << "ins " << x << '\t' << y << '\t' << b << endl;
                bvs.insert(BitVecSet::VecNum(x), BitVecSet::VecPos(y), b);

                dynamic_bitset<> t;
                for (uint64_t j = 0; j < y; ++j)
                {
                    t.push_back(ref[x][j]);
                }
                t.push_back(b);
                for (uint64_t j = y; j < z; ++j)
                {
                    t.push_back(ref[x][j]);
                }
                ref[x].swap(t);
            }
            else
            {
                //cerr << "ins " << x << endl;
                bvs.insert(BitVecSet::VecNum(x));
                ref.push_back(dynamic_bitset<>());
            }
        }
        else
        {
            uint64_t x = dist(rng) * ref.size();
            uint64_t z = bvs.size(BitVecSet::VecNum(x));
            if (z > 0)
            {
                uint64_t y = dist(rng) * z;
                //cerr << "del " << x << '\t' << y << endl;

                bvs.erase(BitVecSet::VecNum(x), BitVecSet::VecPos(y));

                dynamic_bitset<> t;
                for (uint64_t j = 0; j < y; ++j)
                {
                    t.push_back(ref[x][j]);
                }
                for (uint64_t j = y + 1; j < z; ++j)
                {
                    t.push_back(ref[x][j]);
                }
                ref[x].swap(t);
            }
            else
            {
                //cerr << "del " << x << endl;
                bvs.erase(BitVecSet::VecNum(x));
                ref.erase(ref.begin() + x);
            }
        }
    }
}
#endif

#include "testEnd.hh"
