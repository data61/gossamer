#ifndef MULTITHREADEDBATCHTASK_HH
#define MULTITHREADEDBATCHTASK_HH

#ifndef PROGRESSMONITOR_HH
#include "ProgressMonitor.hh"
#endif // PROGRESSMONITOR_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif // STD_VECTOR

#ifndef BOOST_THREAD_HPP
#include <boost/thread.hpp>
#define BOOST_THREAD_HPP
#endif // BOOST_THREAD_HPP

#ifndef BOOST_STATIC_ASSERT_HPP
#include <boost/static_assert.hpp>
#define BOOST_STATIC_ASSERT_HPP
#endif // BOOST_STATIC_ASSERT_HPP

#ifndef BOOST_EXCEPTION_PTR_HPP
#include <boost/exception_ptr.hpp>
#define BOOST_EXCEPTION_PTR_HPP
#endif // BOOST_EXCEPTION_PTR_HPP

class MultithreadedBatchTask : private boost::noncopyable
{
public:
    class WorkThread;
    typedef boost::shared_ptr<WorkThread> WorkThreadPtr;

    class MonitorThread : private boost::noncopyable
    {
    public:
        MonitorThread(ProgressMonitorBase& pProgressMon);

        void notify();

    private:
        ProgressMonitorBase& mProgressMon;
        std::vector<WorkThreadPtr> mThreads;

        boost::mutex mMutex;
        boost::condition_variable mCondVar;
        bool mThreadSignalled;

        void addThread(const WorkThreadPtr& pThread)
        {
            mThreads.push_back(pThread);
        }

        void operator()();

        friend class MultithreadedBatchTask;
    };

    class WorkThread : private boost::noncopyable
    {
    public:
        virtual void operator()() = 0;

    protected:
        WorkThread(MultithreadedBatchTask& pTask);

        bool reportWorkDone(uint64_t pWorkDone);

        uint64_t numThreads() const
        {
            return mMonThread.mThreads.size();
        }

    private:
        void run();

        enum return_state_t
        {
            kNormalReturn = 0,
            kAborted,
            kExceptionThrown
        };

        uint64_t mThreadId;
        boost::mutex mReportDataMutex;
        uint64_t mWorkDone;
        bool mDone;
        bool mAbortRequested;
        MonitorThread& mMonThread;
        return_state_t mReturnState;
        boost::exception_ptr mThrownException;

        friend class MultithreadedBatchTask;
        friend class MonitorThread;
    };

    MultithreadedBatchTask(ProgressMonitorBase& pProgressMon)
        : mMonThread(pProgressMon)
    {
    }

    template<typename T>
    void addThread(const boost::shared_ptr<T>& pThread)
    {
        // T must be derived from WorkThread.
        BOOST_STATIC_ASSERT((boost::is_base_of<WorkThread,T>::value));
        mMonThread.addThread(boost::static_pointer_cast<WorkThread>(pThread));
    }

    void operator()()
    {
        mMonThread();
    }

private:
    MonitorThread mMonThread;
};

#endif
