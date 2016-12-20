// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BOUNDEDQUEUE_HH
#define BOUNDEDQUEUE_HH

#ifndef STD_THREAD
#include <thread>
#define STD_THREAD
#endif

#ifndef STD_MUTEX
#include <mutex>
#define STD_MUTEX
#endif

#ifndef STD_CONDITION_VARIABLE
#include <condition_variable>
#define STD_CONDITION_VARIABLE
#endif

#ifndef BOOST_PTR_CONTAINER_PTR_DEQUE_HPP
#include <boost/ptr_container/ptr_deque.hpp>
#define BOOST_PTR_CONTAINER_PTR_DEQUE_HPP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef DEQUE_HH
#include "Deque.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef PROFILE_HH
#include "Profile.hh"
#endif

template <typename T, bool W = false>
class BoundedQueue
{
public:
    /**
     * Put an item on to the shared queue.
     * If the queue is at it maximum size, then wait until a place becomes available.
     */
    void put(const T& pItem)
    {
        Profile::Context pc("BoundedQueue::put");
        {
            std::unique_lock<std::mutex> lock(mMutex);
            while (mItems.size() == mMaxItems)
            {
                Profile::Context pc("BoundedQueue::put::wait");
                mFullWaits++;
                mFullCond.wait(lock);
            }
            mItems.push_back(pItem);
            if (mWaiters > 0)
            {
                mEmptyCond.notify_one();
            }
        }
    }

    /**
     * Get an item from the shared queue.
     * If there are no items, then wait for an item to arrive.
     * If finish() is called, then don't retrieve an item, and
     * return false, otherwise return true.
     */
    bool get(T& pItem)
    {
        Profile::Context pc("BoundedQueue::get");
        std::unique_lock<std::mutex> lock(mMutex);
        while (mItems.size() == 0 && !mFinished)
        {
            Profile::Context pc("BoundedQueue::get::wait");
            mEmptyWaits++;
            ++mWaiters;
            if (W)
            {
                mWaitersCond.notify_one();
            }
            mEmptyCond.wait(lock);
            --mWaiters;
        }
        if (mItems.size() == 0)
        {
            BOOST_ASSERT(mFinished);
            return false;
        }
        BOOST_ASSERT(mItems.size());
        if (mItems.size() == mMaxItems)
        {
            mFullCond.notify_one();
        }
        std::swap(pItem, mItems.front());
        mItems.pop_front();
        return true;
    }

    /**
     * Indicate there are no more items coming.
     */
    void finish()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mFinished = true;
        mEmptyCond.notify_all();
    }

    /**
     * Block until pNumConsumers are blocked waiting for input.
     */
    void sync(uint64_t pNumConsumers)
    {
        BOOST_ASSERT(W);
        std::unique_lock<std::mutex> lock(mMutex);
        while (mWaiters < pNumConsumers)
        {
            mWaitersCond.wait(lock);
        }
    }

    /**
     * Retrieve information about the behaviour of the queue.
     */
    PropertyTree stat() const
    {
        PropertyTree t;
        t.putProp("empty-waits", mEmptyWaits);
        t.putProp("full-waits", mFullWaits);
        return t;
    }

    BoundedQueue(uint64_t pMaxItems)
        : mMaxItems(pMaxItems), mFinished(false), mFullWaits(0), mEmptyWaits(0), mWaiters(0)
    {
    }

private:
    const uint64_t mMaxItems;
    Deque<T> mItems;
    bool mFinished;
    std::mutex mMutex;
    std::condition_variable mFullCond;
    std::condition_variable mEmptyCond;
    std::condition_variable mWaitersCond;
    uint64_t mFullWaits;
    uint64_t mEmptyWaits;
    uint64_t mWaiters;
};

#endif // BOUNDEDQUEUE_HH
