#include "ExternalVarPushSorter.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <boost/random.hpp>

#include "StringFileFactory.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestExternalVarPushSorter
#include "testBegin.hh"


class Checker
{
public:
    vector<uint64_t>::const_iterator curr;
    vector<uint64_t>::const_iterator end;

    void push_back(uint64_t x)
    {
        BOOST_CHECK(curr != end);
        BOOST_CHECK_EQUAL(*curr, x);
        ++curr;
    }

    void done()
    {
        BOOST_CHECK(curr == end);
    }

    Checker(const vector<uint64_t>& xs)
        : curr(xs.begin()), end(xs.end())
    {
    }
};

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    ExternalVarPushSorter<uint64_t> sorter(fac, 1000);

    mt19937 rng(19);
    uniform_real<> dist;
    variate_generator<mt19937&,uniform_real<> > gen(rng,dist);

    vector<uint64_t> xs;
    for (uint64_t i = 0; i < 10000; ++i)
    {
        uint64_t x = gen() * (1ULL << 56);
        sorter.push_back(x);
        xs.push_back(x);
    }

    sort(xs.begin(), xs.end());

    Checker c(xs);
    sorter.sort(c);
    c.done();
}

#include "testEnd.hh"
