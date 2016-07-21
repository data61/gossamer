#include "Utils.hh"
#include "Atomic.hh"

#include "Utils.hh"

#include <stdint.h>
#include <boost/thread.hpp>
#include <boost/random.hpp>

#define GOSS_TEST_MODULE TestAtomic
#include "testBegin.hh"

using namespace boost;
using namespace std;

struct Counter
{
    Atomic mValue;

    Counter()
        : mValue(0)
    {
    }

    void adjust(uint64_t pAdj)
    {
        mValue += pAdj;
    }

    void inc()
    {
        ++mValue;
    }
};


struct IncThread : private boost::noncopyable
{
    static const uint64_t N = 1000000ull;
    uint64_t mSeed;
    Counter& mCounter;
    uint64_t mAdj;

    IncThread(Counter& pCounter, uint64_t pSeed)
        : mSeed(pSeed), mCounter(pCounter), mAdj(0)
    {
    }

    void runImpl()
    {
        mt19937 rng(mSeed);

        for (uint64_t i = 0; i < N; ++i)
        {
            double r = rng();
            if (r > 0.75)
            {
                mCounter.inc();
                ++mAdj;
            }
            else if (r > 0.5)
            {
                mCounter.adjust(3);
                mAdj += 3;
            }
            else if (r > 0.25)
            {
                mCounter.adjust(7);
                mAdj += 7;
            }
        }
    }

    void run()
    {
        try {
            runImpl();
        }
        catch (...)
        {
        }
    }
};


BOOST_AUTO_TEST_CASE(test_inc_race)
{
    Counter counter;
    boost::shared_ptr<IncThread> inc1(new IncThread(counter, 17));
    boost::shared_ptr<IncThread> inc2(new IncThread(counter, 23));

#if 1
    thread_group grp;
    grp.create_thread(boost::bind(&IncThread::run, inc1.get()));
    grp.create_thread(boost::bind(&IncThread::run, inc2.get()));
    grp.join_all();
#else
    inc1->run();
    inc2->run();
#endif

    BOOST_CHECK_EQUAL(counter.mValue.get(), inc1->mAdj + inc2->mAdj);
}

#include "testEnd.hh"
