
#include "LineSource.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <boost/random.hpp>


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
