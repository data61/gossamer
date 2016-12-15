// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BACKGROUNDMULTICONSUMER_HH
#define BACKGROUNDMULTICONSUMER_HH

#ifndef BOUNDEDQUEUE_HH
#include "BoundedQueue.hh"
#endif

#ifndef THREADGROUP_HH
#include "ThreadGroup.hh"
#endif

template <typename T>
class BackgroundMultiConsumer
{
public:
    typedef T value_type;

    class Holder
    {
    public:
        virtual ~Holder() {}
    };
    typedef std::shared_ptr<Holder> HolderPtr;

    template <typename Consumer>
    class PushBackConsHolder : public Holder
    {
    public:
        void operator()()
        {
            T itm;
            while (mQueue.get(itm))
            {
                mCons.push_back(itm);
            }
        }

        PushBackConsHolder(BoundedQueue<T,true>& pQueue, Consumer& pCons)
            : mQueue(pQueue), mCons(pCons)
        {
        }

    private:
        BoundedQueue<T,true>& mQueue;
        Consumer& mCons;
    };

    template <typename Consumer>
    class ApplyConsHolder : public Holder
    {
    public:
        void operator()()
        {
            T itm;
            while (mQueue.get(itm))
            {
                mCons(itm);
            }
        }

        ApplyConsHolder(BoundedQueue<T,true>& pQueue, Consumer& pCons)
            : mQueue(pQueue), mCons(pCons)
        {
        }

    private:
        BoundedQueue<T,true>& mQueue;
        Consumer& mCons;
    };

    void push_back(const value_type& pItem)
    {
        mQueue.put(pItem);
    }

    void end()
    {
        mFinished = true;
        mQueue.finish();
    }

    void wait()
    {
        end();
        mThreads.join();
        mJoined = true;
    }

    template <typename Consumer>
    void add(Consumer& pCons)
    {
        addPushBack<Consumer>(pCons);
    }

    template <typename Consumer>
    void addPushBack(Consumer& pCons)
    {
        HolderPtr h(new PushBackConsHolder<Consumer>(mQueue, pCons));
        mHolders.push_back(h);
        mThreads.create(static_cast<PushBackConsHolder<Consumer>&>(*h));
    }

    template <typename Consumer>
    void addApply(Consumer& pCons)
    {
        HolderPtr h(new ApplyConsHolder<Consumer>(mQueue, pCons));
        mHolders.push_back(h);
        mThreads.create(static_cast<ApplyConsHolder<Consumer>&>(*h));
    }
    
    void sync(uint64_t pNumConsumers)
    {
        mQueue.sync(pNumConsumers);
    }

    BackgroundMultiConsumer(uint64_t pNumBufItems)
        : mQueue(pNumBufItems), mFinished(false), mJoined(false)
    {
    }

    ~BackgroundMultiConsumer()
    {
        if (!mFinished)
        {
            end();
        }
        if (!mJoined)
        {
            mThreads.join();
        }
    }

private:
    BoundedQueue<value_type,true> mQueue;
    ThreadGroup mThreads;
    std::vector<HolderPtr> mHolders;
    bool mFinished;
    bool mJoined;
};

#endif // BACKGROUNDMULTICONSUMER_HH
