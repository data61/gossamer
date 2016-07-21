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
