// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "RunLengthCodedSet.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestRunLengthCodedSet
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(test0)
{
    std::vector<RunLengthCodedSet> rs(10);

    rs[1].append(0);
    rs[1].append(2);

    BOOST_CHECK_EQUAL(rs[1].count(), 2);
    BOOST_CHECK_EQUAL(rs[1].select(0), 0);
    BOOST_CHECK_EQUAL(rs[1].select(1), 2);
}


BOOST_AUTO_TEST_CASE(test1)
{
    std::vector<RunLengthCodedSet> rs(10);

    rs[8].append(0);
    rs[5].append(0);
    rs[0].append(0);
    rs[7].append(0);
    rs[3].append(0);
    rs[1].append(0);
    rs[8].append(1);
    rs[5].append(1);
    rs[0].append(1);
    rs[7].append(1);
    rs[4].append(1);
    rs[2].append(1);
    rs[9].append(2);
    rs[6].append(2);
    rs[0].append(2);
    rs[7].append(2);
    rs[3].append(2);
    rs[1].append(2);
    rs[9].append(3);
    rs[6].append(3);
    rs[0].append(3);
    rs[7].append(3);
    rs[4].append(3);
    rs[2].append(3);

    BOOST_CHECK_EQUAL(rs[0].count(), 4);

    BOOST_CHECK_EQUAL(rs[1].count(), 2);
    BOOST_CHECK_EQUAL(rs[1].select(0), 0);
    BOOST_CHECK_EQUAL(rs[1].select(1), 2);

    BOOST_CHECK_EQUAL(rs[2].count(), 2);
    BOOST_CHECK_EQUAL(rs[2].select(0), 1);
    BOOST_CHECK_EQUAL(rs[2].select(1), 3);

    BOOST_CHECK_EQUAL(rs[3].count(), 2);
    BOOST_CHECK_EQUAL(rs[3].select(0), 0);
    BOOST_CHECK_EQUAL(rs[3].select(1), 2);

    BOOST_CHECK_EQUAL(rs[4].count(), 2);
    BOOST_CHECK_EQUAL(rs[4].select(0), 1);
    BOOST_CHECK_EQUAL(rs[4].select(1), 3);

    BOOST_CHECK_EQUAL(rs[5].count(), 2);
    BOOST_CHECK_EQUAL(rs[5].select(0), 0);
    BOOST_CHECK_EQUAL(rs[5].select(1), 1);

    BOOST_CHECK_EQUAL(rs[6].count(), 2);
    BOOST_CHECK_EQUAL(rs[6].select(0), 2);
    BOOST_CHECK_EQUAL(rs[6].select(1), 3);

    BOOST_CHECK_EQUAL(rs[7].count(), 4);

    BOOST_CHECK_EQUAL(rs[8].count(), 2);
    BOOST_CHECK_EQUAL(rs[8].select(0), 0);
    BOOST_CHECK_EQUAL(rs[8].select(1), 1);

    BOOST_CHECK_EQUAL(rs[9].count(), 2);
    BOOST_CHECK_EQUAL(rs[9].select(0), 2);
    BOOST_CHECK_EQUAL(rs[9].select(1), 3);
}

#include "testEnd.hh"
