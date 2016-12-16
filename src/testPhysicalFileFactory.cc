// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "PhysicalFileFactory.hh"
#include "StringFileFactory.hh"
#include "GossamerException.hh"

#include <vector>
#include <sstream>
#include <string>
#include <iostream>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestPhysicalFileFactory
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(testOpenBadly)
{
    PhysicalFileFactory fac;
    BOOST_CHECK_THROW(fac.in("snark/snark/snark"), Gossamer::error);
    BOOST_CHECK_THROW(fac.out("snark/snark/snark"), Gossamer::error);
}

BOOST_AUTO_TEST_CASE(testMoreInputDetail)
{
    PhysicalFileFactory fac;
    try
    {
        fac.in("snark/snark/snark");
    }
    catch (Gossamer::error& exc)
    {
        int const* en = get_error_info<errinfo_errno>(exc);
        BOOST_CHECK(en != NULL);
        BOOST_CHECK_EQUAL(*en, ENOENT);

        string const* st = get_error_info<errinfo_file_name>(exc);
        BOOST_CHECK(st != NULL);
        BOOST_CHECK_EQUAL(*st, "snark/snark/snark");

        int const* li = get_error_info<throw_line>(exc);
        BOOST_CHECK(li != NULL);
        BOOST_CHECK_EQUAL(*li, 64);

        const char* const* fi = get_error_info<throw_file>(exc);
        BOOST_CHECK(fi != NULL);
        // the return string maybe an abs or relative path, not just
        // the file name.
        //BOOST_CHECK_EQUAL(*fi, string("PhysicalFileFactory.cc"));
        BOOST_CHECK( string(*fi).find("PhysicalFileFactory.cc") != string::npos );
    }
}

BOOST_AUTO_TEST_CASE(testMoreOutputDetail)
{
    PhysicalFileFactory fac;
    try
    {
        fac.out("snark/snark/snark");
    }
    catch (Gossamer::error& exc)
    {
        int const* en = get_error_info<errinfo_errno>(exc);
        BOOST_CHECK(en != NULL);
        BOOST_CHECK_EQUAL(*en, ENOENT);

        string const* st = get_error_info<errinfo_file_name>(exc);
        BOOST_CHECK(st != NULL);
        BOOST_CHECK_EQUAL(*st, "snark/snark/snark");

        int const* li = get_error_info<throw_line>(exc);
        BOOST_CHECK(li != NULL);
        BOOST_CHECK_EQUAL(*li, 163);

        const char* const* fi = get_error_info<throw_file>(exc);
        BOOST_CHECK(fi != NULL);
        // the return string maybe an abs or relative path, not just
        // the file name.
        //BOOST_CHECK_EQUAL(*fi, string("PhysicalFileFactory.cc"));
        BOOST_CHECK( string(*fi).find("PhysicalFileFactory.cc") != string::npos );
    }
}

#include "testEnd.hh"
