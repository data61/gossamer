
#include "Utils.hh"

#include <boost/dynamic_bitset.hpp>
#include <boost/random.hpp>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestUtils
#include "testBegin.hh"


BOOST_AUTO_TEST_CASE(test1)
{
    BOOST_CHECK_EQUAL(Gossamer::log2(1), 0);
    BOOST_CHECK_EQUAL(Gossamer::log2(2), 1);
    BOOST_CHECK_EQUAL(Gossamer::log2(3), 2);
    BOOST_CHECK_EQUAL(Gossamer::log2(4), 2);
    BOOST_CHECK_EQUAL(Gossamer::log2(5), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(6), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(7), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(8), 3);
    BOOST_CHECK_EQUAL(Gossamer::log2(9), 4);
}

BOOST_AUTO_TEST_CASE(test2)
{
    uint64_t w = 0x5;
    BOOST_CHECK_EQUAL(Gossamer::select1(w, 0), 0);
    BOOST_CHECK_EQUAL(Gossamer::select1(w, 1), 2);
    for (uint64_t i = 0; i < 64; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::select1(0xFFFFFFFFFFFFFFFFull, i), i);
    }
    for (uint64_t i = 0; i < 32; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::select1(0x5555555555555555ull, i), 2*i+0);
        BOOST_CHECK_EQUAL(Gossamer::select1(0xAAAAAAAAAAAAAAAAull, i), 2*i+1);
    }
    for (uint64_t i = 0; i < 16; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::select1(0x1111111111111111ull, i), 4*i+0);
        BOOST_CHECK_EQUAL(Gossamer::select1(0x2222222222222222ull, i), 4*i+1);
        BOOST_CHECK_EQUAL(Gossamer::select1(0x4444444444444444ull, i), 4*i+2);
        BOOST_CHECK_EQUAL(Gossamer::select1(0x8888888888888888ull, i), 4*i+3);
    }
}

BOOST_AUTO_TEST_CASE(testClz)
{
    uint64_t x = 1ull << 63;
    for (uint64_t i = 0; i <= 64; ++i)
    {
        BOOST_CHECK_EQUAL(Gossamer::count_leading_zeroes(x), i);
        x >>= 1;
    }
}

#include "testEnd.hh"
