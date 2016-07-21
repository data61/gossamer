#ifndef WORKQUEUE_HH
#define WORKQUEUE_HH

#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <deque>

//#include <iostream>

class WorkQueue
{
public:
    typedef boost::function<void (void)> Item;
    typedef std::deque<Item> Items;

    class Worker
    {
    public:
        void operator()()
        {
            while (true)
            {
                Item itm;
                {
                    boost::unique_lock<boost::mutex> lock(mMutex);
                    while (mItems.empty() && !mFinished)
                    {
                        //std::cerr << "waiting...." << std::endl;
                        ++mWaiters;
                        mCond.wait(lock);
                        --mWaiters;
                    }
                    if (mItems.empty() && mFinished)
                    {
                        return;
                    }
                    //std::cerr << "evaluating...." << std::endl;
                    itm = mItems.front();
                    mItems.pop_front();
                }
                itm();
            }
        }

        Worker(boost::mutex& pMutex, boost::condition_variable& pCond, Items& pItems, uint64_t& pWaiters, bool& pFinished)
            : mMutex(pMutex), mCond(pCond), mItems(pItems), mWaiters(pWaiters), mFinished(pFinished)
        {
        }

    private:
        boost::mutex& mMutex;
        boost::condition_variable& mCond;
        Items& mItems;
        uint64_t& mWaiters;
        bool& mFinished;
    };
    typedef boost::shared_ptr<Worker> WorkerPtr;

    void push_back(const Item& pItem)
    {
        boost::unique_lock<boost::mutex> lock(mMutex);
        mItems.push_back(pItem);
        if (mWaiters > 0)
        {
            mCond.notify_one();
        }
    }

    void wait()
    {
        {
            boost::unique_lock<boost::mutex> lock(mMutex);
            mFinished = true;
            mCond.notify_all();
        }
        mThreads.join_all();
        mJoined = true;
    }

    WorkQueue(uint64_t pNumThreads)
        : mWaiters(0), mFinished(false), mJoined(false)
    {
        //std::cerr << "creating WorkQueue with " << pNumThreads << " threads." << std::endl;
        for (uint64_t i = 0; i < pNumThreads; ++i)
        {
            mWorkers.push_back(WorkerPtr(new Worker(mMutex, mCond, mItems, mWaiters, mFinished)));
            mThreads.create_thread(*mWorkers.back());
        }
    }

    ~WorkQueue()
    {
        if (!mJoined)
        {
            wait();
        }
    }

private:
    boost::mutex mMutex;
    boost::condition_variable mCond;
    Items mItems;
    uint64_t mWaiters;
    bool mFinished;
    bool mJoined;
    std::deque<WorkerPtr> mWorkers;
    boost::thread_group mThreads;
};

#endif // WORKQUEUE_HH
