/**  \file
 * Testing SimpleHashSet.
 *
 */

#include "BoundedQueue.hh"
#include <vector>
#include <boost/random.hpp>


using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestBoundedQueue
#include "testBegin.hh"


class Writer
{
public:
    void operator()()
    {
        mt19937 rng(mSeed);
        uniform_int<> dist(1, 10);
        variate_generator<mt19937&,uniform_int<> > gen(rng,dist);

        for (uint64_t i = 1; i < 10000; ++i)
        {
            mQueue.put(i);

            boost::this_thread::sleep(boost::posix_time::milliseconds(gen()));
        }
        mQueue.finish();
    }

    Writer(BoundedQueue<uint64_t>& pQueue, uint64_t pSeed)
        : mQueue(pQueue), mSeed(pSeed)
    {
    }

private:
    BoundedQueue<uint64_t>& mQueue;
    uint64_t mSeed;
};

class Reader
{
public:
    void operator()()
    {
        mt19937 rng(mSeed);
        uniform_int<> dist(1, 10);
        variate_generator<mt19937&,uniform_int<> > gen(rng,dist);

        uint64_t p = 0;
        uint64_t x = 0;
        while (mQueue.get(x))
        {
            BOOST_CHECK(p < x);

            boost::this_thread::sleep(boost::posix_time::milliseconds(gen()));
        }
    }

    Reader(BoundedQueue<uint64_t>& pQueue, uint64_t pSeed)
        : mQueue(pQueue), mSeed(pSeed)
    {
    }

private:
    BoundedQueue<uint64_t>& mQueue;
    uint64_t mSeed;
};

BOOST_AUTO_TEST_CASE(testEmpty)
{
    BoundedQueue<uint64_t> q(10);
    Writer w(q, 19);
    Reader r(q, 23);
    boost::thread_group g;
    g.create_thread(r);
    g.create_thread(w);
    g.join_all();
}

#include "testEnd.hh"
