
#include "FixedWidthBitArray.hh"
#include "StringFileFactory.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <boost/random.hpp>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestFixedWidthBitVector
#include "testBegin.hh"


#if 1
BOOST_AUTO_TEST_CASE(test1)
{
    static const uint64_t N = 100000;
    StringFileFactory fac;
    vector<uint64_t> nums;
    {
        FixedWidthBitArray<4>::Builder b("x", fac);

        mt19937 rng(17);
        uniform_int<> dist(0, 15);
        variate_generator<mt19937&,uniform_int<> > gen(rng, dist);

        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t x = gen();
            b.push_back(x);
            nums.push_back(x);
        }
        b.end();
    }

    FixedWidthBitArray<4> v("x", fac);
    FixedWidthBitArray<4>::Iterator itr(v.iterator());

    for (uint64_t i = 0; i < N && itr.valid(); ++i, ++itr)
    {
        BOOST_CHECK_EQUAL(nums[i], v.get(i));
        BOOST_CHECK_EQUAL(nums[i], *itr);
    }
}
#endif

#if 1
BOOST_AUTO_TEST_CASE(test2)
{
    static const uint64_t N = 100000;
    StringFileFactory fac;
    vector<uint64_t> nums;
    {
        FixedWidthBitArray<7>::Builder b("x", fac);

        mt19937 rng(17);
        uniform_int<> dist(0, 127);
        variate_generator<mt19937&,uniform_int<> > gen(rng, dist);

        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t x = gen();
            b.push_back(x);
            nums.push_back(x);
        }
        b.end();
    }

    FixedWidthBitArray<7> v("x", fac);
    FixedWidthBitArray<7>::Iterator itr(v.iterator());

    for (uint64_t i = 0; i < N && itr.valid(); ++i, ++itr)
    {
        BOOST_CHECK_EQUAL(nums[i], v.get(i));
        BOOST_CHECK_EQUAL(nums[i], *itr);
    }
}
#endif

#include "testEnd.hh"