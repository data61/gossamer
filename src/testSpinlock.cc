#include "Utils.hh"
#include "Spinlock.hh"

#include "Utils.hh"

#include <stdint.h>
#include <boost/thread.hpp>
#include <boost/random.hpp>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestSpinlock
#include "testBegin.hh"

struct Counter
{
    Spinlock mLock;
    int64_t mValue;

    Counter()
        : mValue(0)
    {
    }

    void adjust(int64_t pAdj)
    {
        SpinlockHolder lock(mLock);
        mValue += pAdj;
    }
};


struct ThreadBase : private boost::noncopyable
{
    virtual void runImpl() = 0;

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


struct IncThread : public ThreadBase
{
    static const uint64_t N = 1000000ull;
    Counter& mCounter;
    int64_t mAdj;

    IncThread(Counter& pCounter)
        : mCounter(pCounter), mAdj(0)
    {
    }

    void runImpl()
    {
        mt19937 rng(17);

        for (uint64_t i = 0; i < N; ++i)
        {
            int64_t adj = rng() > 0.5 ? 1 : 0;
            mCounter.adjust(adj);
            mAdj += adj;
        }
    }
};


struct DecThread : public ThreadBase
{
    static const uint64_t N = 1000000ull;
    Counter& mCounter;
    int64_t mAdj;

    DecThread(Counter& pCounter)
        : mCounter(pCounter), mAdj(0)
    {
    }

    void runImpl()
    {
        mt19937 rng(23);

        for (uint64_t i = 0; i < N; ++i)
        {
            int64_t adj = rng() > 0.5 ? -1 : 0;
            mCounter.adjust(adj);
            mAdj += adj;
        }
    }
};


BOOST_AUTO_TEST_CASE(test_inc_dec_race)
{
    Counter counter;
    boost::shared_ptr<IncThread> inc(new IncThread(counter));
    boost::shared_ptr<DecThread> dec(new DecThread(counter));

    thread_group grp;
    grp.create_thread(boost::bind(&ThreadBase::run, inc.get()));
    grp.create_thread(boost::bind(&ThreadBase::run, dec.get()));
    grp.join_all();

    BOOST_CHECK_EQUAL(counter.mValue, inc->mAdj + dec->mAdj);
}

#include "testEnd.hh"
