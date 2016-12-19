// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef WORKQUEUE_HH
#define WORKQUEUE_HH

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <deque>
#include "ThreadGroup.hh"

class WorkQueue
{
public:
    typedef boost::function<void (void)> Item;
    typedef std::deque<Item> Items;

private:
    class Worker;
    friend class Worker;
    typedef std::shared_ptr<Worker> WorkerPtr;

    std::mutex mMutex;
    std::condition_variable mCond;
    Items mItems;
    uint64_t mWaiters;
    bool mFinished;
    bool mJoined;
    std::vector<WorkerPtr> mWorkers;
    ThreadGroup mThreads;

    class Worker
    {
    public:
        void operator()()
        {
            while (true)
            {
                Item itm;
                {
                    std::unique_lock<std::mutex> lock(mQueue.mMutex);
                    while (mQueue.mItems.empty() && !mQueue.mFinished)
                    {
                        //std::cerr << "waiting...." << std::endl;
                        ++mQueue.mWaiters;
                        mQueue.mCond.wait(lock);
                        --mQueue.mWaiters;
                    }
                    if (mQueue.mItems.empty() && mQueue.mFinished)
                    {
                        return;
                    }
                    //std::cerr << "evaluating...." << std::endl;
                    itm = mQueue.mItems.front();
                    mQueue.mItems.pop_front();
                }
                itm();
            }
        }

        Worker(WorkQueue& pQueue)
            : mQueue(pQueue)
        {
        }

    private:
        WorkQueue& mQueue;
    };

public:

    void push_back(const Item& pItem)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mItems.push_back(pItem);
        if (mWaiters > 0)
        {
            mCond.notify_one();
        }
    }

    void wait()
    {
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mFinished = true;
            mCond.notify_all();
        }
        mThreads.join();
        mJoined = true;
    }

    WorkQueue(uint64_t pNumThreads)
        : mWaiters(0), mFinished(false), mJoined(false)
    {
        //std::cerr << "creating WorkQueue with " << pNumThreads << " threads." << std::endl;
        mWorkers.reserve(pNumThreads);
        for (uint64_t i = 0; i < pNumThreads; ++i)
        {
            auto w = std::shared_ptr<Worker>(new Worker(*this));
            mWorkers.push_back(w);
            mThreads.create(*w);
        }
    }

    ~WorkQueue()
    {
        if (!mJoined)
        {
            wait();
        }
    }
};

#endif // WORKQUEUE_HH
