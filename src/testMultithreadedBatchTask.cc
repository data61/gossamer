/**  \file
 * Testing MultithreadedBatchTask.
 *
 */

#include "MultithreadedBatchTask.hh"

#include "GossamerException.hh"
#include "StringFileFactory.hh"
#include <vector>
#include <deque>
#include <queue>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestMultithreadedBatchTask
#include "testBegin.hh"

namespace {
    void sleepFor100msec()
    {
        boost::system_time timeout
            = boost::get_system_time()
            + boost::posix_time::milliseconds(100);
        boost::thread::sleep(timeout);
    }
}

struct AnException : public std::exception
{
};

struct InfinitelyLoopingWorker : public MultithreadedBatchTask::WorkThread
{
    void operator()()
    {
        for (;;)
        {
            if (!reportWorkDone(0))
            {
                return;
            }
            sleepFor100msec();
        }
    }

    InfinitelyLoopingWorker(MultithreadedBatchTask& pTask)
        : MultithreadedBatchTask::WorkThread(pTask)
    {
    }
};

struct ExceptionThrowingWorker : public MultithreadedBatchTask::WorkThread
{
    void operator()()
    {
        for (uint64_t i = 0; i < mWaitCycles; ++i)
        {
            if (!reportWorkDone(0))
            {
                return;
            }
            sleepFor100msec();
        }
        BOOST_THROW_EXCEPTION(AnException());
    }

    ExceptionThrowingWorker(MultithreadedBatchTask& pTask, uint64_t pWaitCycles)
        : MultithreadedBatchTask::WorkThread(pTask), mWaitCycles(pWaitCycles)
    {
    }

    uint64_t mWaitCycles;
};

struct WorkingWorker : public MultithreadedBatchTask::WorkThread
{
    void operator()()
    {
        for (uint64_t work = 0; work < mWorkToDo; ++work)
        {
            if (!reportWorkDone(work))
            {
                return;
            }
            sleepFor100msec();
        }
        mDoneNormally = true;
    }

    uint64_t mWorkToDo;
    bool mDoneNormally;

    WorkingWorker(MultithreadedBatchTask& pTask, uint64_t pWorkToDo)
        : MultithreadedBatchTask::WorkThread(pTask), mWorkToDo(pWorkToDo),
          mDoneNormally(false)
    {
    }
};


BOOST_AUTO_TEST_CASE(testNoThreads)
{
    StringFileFactory fac;
    Logger log("log", fac);
    ProgressMonitor progress(log, 0, 1);

    MultithreadedBatchTask task(progress);
    task();
}

BOOST_AUTO_TEST_CASE(testOneThread)
{
    const uint64_t workToDo = 10;

    StringFileFactory fac;
    Logger log("log", fac);
    ProgressMonitor progress(log, workToDo, 1);

    MultithreadedBatchTask task(progress);

    boost::shared_ptr<WorkingWorker> thr(new WorkingWorker(task, workToDo));
    task.addThread(thr);

    task();

    BOOST_CHECK(thr->mDoneNormally);
}

BOOST_AUTO_TEST_CASE(testTwoThreads)
{
    const uint64_t workToDo = 10;

    StringFileFactory fac;
    Logger log("log", fac);
    ProgressMonitor progress(log, workToDo*2, 1);

    MultithreadedBatchTask task(progress);

    boost::shared_ptr<WorkingWorker> thr1(new WorkingWorker(task, workToDo));
    boost::shared_ptr<WorkingWorker> thr2(new WorkingWorker(task, workToDo));
    task.addThread(thr1);
    task.addThread(thr2);

    task();

    BOOST_CHECK(thr1->mDoneNormally);
    BOOST_CHECK(thr2->mDoneNormally);
}

BOOST_AUTO_TEST_CASE(testExceptionDelivery)
{
    const uint64_t workToDo = 10;

    StringFileFactory fac;
    Logger log("log", fac);
    ProgressMonitor progress(log, workToDo*2, 1);

    MultithreadedBatchTask task(progress);

    boost::shared_ptr<InfinitelyLoopingWorker> thr1(new InfinitelyLoopingWorker(task));
    boost::shared_ptr<ExceptionThrowingWorker> thr2(new ExceptionThrowingWorker(task, 2));
    // shared_ptr<InfinitelyLoopingWorker> thr3(new InfinitelyLoopingWorker(task));
    task.addThread(thr1);
    task.addThread(thr2);
    // task.addThread(thr3);

    BOOST_CHECK_THROW(task(), AnException);
}

#include "testEnd.hh"
