// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "SmallBaseVector.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestSmallBaseVector
#include "testBegin.hh"

void
checkEditDistance(const char* a, const char* b, size_t dist)
{
    // std::cout << "Comparing \"" << a << "\" and \"" << b << "\"\n";
    SmallBaseVector abv;
    BOOST_CHECK(SmallBaseVector::make(a, abv));
    SmallBaseVector bbv;
    BOOST_CHECK(SmallBaseVector::make(b, bbv));
    BOOST_CHECK_EQUAL(abv.editDistance(bbv), dist);
    BOOST_CHECK_EQUAL(bbv.editDistance(abv), dist);
}

BOOST_AUTO_TEST_CASE(test_editdistance)
{
    checkEditDistance("", "", 0);
    checkEditDistance("", "A", 1);
    checkEditDistance("", "AC", 2);
    checkEditDistance("", "ACG", 3);
    checkEditDistance("ACGTACGT", "ACGTACGT", 0);
    checkEditDistance("ACGTACGT", "ACGTCGT", 1);
    checkEditDistance("ACGTACGT", "CGTACGT", 1);
    checkEditDistance("ACGTACGT", "ACGTACG", 1);
    checkEditDistance("ACGTACGT", "ACGTCCGT", 1);
    checkEditDistance("AAAAAAAA", "CCCCCCCC", 8);
}

#include "testEnd.hh"
