/**  \file
 * Testing FeistelHash.
 *
 */

#include "FeistelHash.hh"
#include <vector>
#include <boost/random.hpp>
#include <thread>


#define GOSS_TEST_MODULE TestFeistelHash
#include "testBegin.hh"

using namespace boost;
using namespace std;

BOOST_AUTO_TEST_CASE(testMutant)
{
    typedef FeistelHash::Item Item;
    Item x(0, 0);
    Item y = FeistelHash::hash(x);
    Item z = FeistelHash::unhash(y);
    BOOST_CHECK_EQUAL(x.first, z.first);
    BOOST_CHECK_EQUAL(x.second, z.second);
}

BOOST_AUTO_TEST_CASE(test4)
{
    mt19937 rng(19);
    uniform_int<> dist(0, 1 << 24);
    variate_generator<mt19937&,uniform_int<> > gen(rng,dist);

    static const uint64_t N = 1024 * 1024;
    for (uint64_t i = 0; i < N; ++i)
    {
        typedef FeistelHash::Item Item;
        Item x(gen(), gen());
        Item y = FeistelHash::hash(x);
        Item z = FeistelHash::unhash(y);
        BOOST_CHECK_EQUAL(x.first, z.first);
        BOOST_CHECK_EQUAL(x.second, z.second);
    }
}

#include "testEnd.hh"

