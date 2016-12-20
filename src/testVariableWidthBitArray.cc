// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "VariableWidthBitArray.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <iostream>
#include <string>
#include <random>
#include <boost/tuple/tuple.hpp>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestVariableWidthBitArray
#include "testBegin.hh"


#if 1
BOOST_AUTO_TEST_CASE(test1)
{
    static const uint64_t N = 50;
    StringFileFactory fac;
    typedef boost::tuple<uint64_t,uint64_t,uint64_t> Item;
    std::vector<Item> nums;
    {
        VariableWidthBitArray::Builder b("x", fac);

        mt19937 rng(17);
        uniform_real_distribution<> dist;

        uint64_t p = 0;
        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t n = 16 * dist(rng);
            uint64_t j = dist(rng) * (1ULL << n);
            // cerr << p << '\t' << n << '\t' << j << endl;
            b.push_back(j, n);
            nums.push_back(Item(p,n,j));
            p += n;
        }
        b.end();
    }

    VariableWidthBitArray v("x", fac);
    VariableWidthBitArray::Iterator itr(v.iterator());

    //cerr << "checking...." << endl;
    for (uint64_t i = 0; i < N && itr.valid(); ++i)
    {
        Item x = nums[i];
        // cerr << x.get<0>() << '\t' << x.get<1>() << '\t' << x.get<2>() << '\t'; //  << endl;
        BOOST_CHECK_EQUAL(x.get<2>(), v.get(x.get<0>(), x.get<1>()));
        BOOST_CHECK_EQUAL(x.get<2>(), itr.get(x.get<1>()));
    }
}
#endif

#include "testEnd.hh"
