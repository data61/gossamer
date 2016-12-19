// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef MULTITHREADEDBATCHTASK_HH
#define MULTITHREADEDBATCHTASK_HH

#ifndef PROGRESSMONITOR_HH
#include "ProgressMonitor.hh"
#endif // PROGRESSMONITOR_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif // STD_VECTOR

#ifndef STD_THREAD
#include <thread>
#define STD_THREAD
#endif // STD_THREAD

#ifndef STD_MUTEX
#include <mutex>
#define STD_MUTEX
#endif

#ifndef STD_CONDITION_VARIABLE
#include <condition_variable>
#define STD_CONDITION_VARIABLE
#endif

#ifndef BOOST_STATIC_ASSERT_HPP
#include <boost/static_assert.hpp>
#define BOOST_STATIC_ASSERT_HPP
#endif // BOOST_STATIC_ASSERT_HPP

#ifndef BOOST_EXCEPTION_PTR_HPP
#include <boost/exception_ptr.hpp>
#define BOOST_EXCEPTION_PTR_HPP
#endif // BOOST_EXCEPTION_PTR_HPP

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
#endif

class MultithreadedBatchTask : private boost::noncopyable
{
public:
    class WorkThread;
    typedef std::shared_ptr<WorkThread> WorkThreadPtr;

    class MonitorThread : private boost::noncopyable
    {
    public:
        MonitorThread(ProgressMonitorBase& pProgressMon);

        void notify();

    private:
        ProgressMonitorBase& mProgressMon;
        std::vector<WorkThreadPtr> mThreads;

        std::mutex mMutex;
        std::condition_variable mCondVar;
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
        std::mutex mReportDataMutex;
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
    void addThread(const std::shared_ptr<T>& pThread)
    {
        // T must be derived from WorkThread.
        BOOST_STATIC_ASSERT((boost::is_base_of<WorkThread,T>::value));
        mMonThread.addThread(std::static_pointer_cast<WorkThread>(pThread));
    }

    void operator()()
    {
        mMonThread();
    }

private:
    MonitorThread mMonThread;
};

#endif
