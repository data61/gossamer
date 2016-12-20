// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "LineSource.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <random>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestPlainLineSource
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;

    fac.addFile("empty", "");

    PlainLineSource src(FileThunkIn(fac, "empty"));
    BOOST_CHECK_EQUAL(src.valid(), false);
}

BOOST_AUTO_TEST_CASE(test2)
{
    StringFileFactory fac;

    fac.addFile("oneA", "abc");
    fac.addFile("oneB", "abc\n");

    {
        PlainLineSource src(FileThunkIn(fac, "oneA"));
        BOOST_CHECK_EQUAL(src.valid(), true);
        BOOST_CHECK_EQUAL(*src, "abc");
        ++src;
        BOOST_CHECK_EQUAL(src.valid(), false);
    }
    {
        PlainLineSource src(FileThunkIn(fac, "oneB"));
        BOOST_CHECK_EQUAL(src.valid(), true);
        BOOST_CHECK_EQUAL(*src, "abc");
        ++src;
        BOOST_CHECK_EQUAL(src.valid(), false);
    }
}

#include "testEnd.hh"
