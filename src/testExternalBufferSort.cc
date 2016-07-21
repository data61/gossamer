#include "ExternalBufferSort.hh"

#include <vector>
#include <iostream>
#include <string>
#include <boost/dynamic_bitset.hpp>
#include <boost/random.hpp>

#include "StringFileFactory.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestExternalBufferSort
#include "testBegin.hh"

typedef vector<uint8_t> Item;

class Checker
{
public:
    vector<Item>::const_iterator curr;
    vector<Item>::const_iterator last;

    void push_back(const Item& x)
    {
        BOOST_CHECK(curr != last);
        BOOST_CHECK(*curr == x);
        ++curr;
    }

    void done()
    {
        BOOST_CHECK(curr == last);
    }

    void end()
    {
    }

    Checker(const vector<Item>& xs)
        : curr(xs.begin()), last(xs.end())
    {
    }
};

BOOST_AUTO_TEST_CASE(test0)
{
    StringFileFactory fac;
    ExternalBufferSort sorter(1024, fac);

    mt19937 rng(19);
    uniform_real<> dist;
    variate_generator<mt19937&,uniform_real<> > gen(rng,dist);

    vector<Item> xs;
    for (uint64_t i = 0; i < 10; ++i)
    {
        uint64_t x = gen() * 24;
        Item itm;
        for (uint64_t j = 0; j < x; ++j)
        {
            itm.push_back(gen() * 256);
        }
        sorter.push_back(itm);
        xs.push_back(itm);
    }

    sort(xs.begin(), xs.end());

    Checker c(xs);
    sorter.sort(c);
    c.done();
}

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    ExternalBufferSort sorter(1024, fac);

    mt19937 rng(19);
    uniform_real<> dist;
    variate_generator<mt19937&,uniform_real<> > gen(rng,dist);

    vector<Item> xs;
    for (uint64_t i = 0; i < 8 * 1024; ++i)
    {
        uint64_t x = gen() * 24;
        Item itm;
        for (uint64_t j = 0; j < x; ++j)
        {
            itm.push_back(gen() * 256);
        }
        sorter.push_back(itm);
        xs.push_back(itm);
    }

    sort(xs.begin(), xs.end());

    Checker c(xs);
    sorter.sort(c);
    c.done();
}

#include "testEnd.hh"