// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossReadBaseString.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGossReadBaseString
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test_iterator)
{
    string l("x");
    string r("NACTTTTGATGCAATGTCAAATTCTCCNCGTCATTCGCAACTGAATACAAGNGAATTTGGAAGGAGAATNTGGTA");
    string q("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
    GossReadBaseString x(l, r, q);

    GossRead::Iterator i(x, 15);

    BOOST_CHECK(i.valid());
}

#include "testEnd.hh"
