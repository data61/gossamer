// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BACKGROUNDBLOCKCONSUMER_HH
#define BACKGROUNDBLOCKCONSUMER_HH

#ifndef BOUNDEDQUEUE_HH
#include "BoundedQueue.hh"
#endif

template <typename Consumer>
class BackgroundBlockConsumer
{
public:
    typedef typename Consumer::value_type value_type;
    typedef std::vector<value_type> block_type;
    typedef std::shared_ptr<block_type> block_ptr_type;

    class ConsBlockWorker
    {
    public:
        void operator()()
        {
            block_ptr_type blkPtr;
            while (mQueue.get(blkPtr))
            {
                const block_type& blk(*blkPtr);
                for (uint64_t i = 0; i < blk.size(); ++i)
                {
                    mCons.push_back(blk[i]);
                }
            }
        }

        ConsBlockWorker(BoundedQueue<block_ptr_type>& pQueue, Consumer& pCons)
            : mQueue(pQueue), mCons(pCons)
        {
        }

    private:
        BoundedQueue<block_ptr_type>& mQueue;
        Consumer& mCons;
    };

    void push_back(const value_type& pVal)
    {
        mCurrBlock->push_back(pVal);
        if (mCurrBlock->size() == mBlkSize)
        {
            mQueue.put(mCurrBlock);
            mCurrBlock = block_ptr_type(new block_type);
            mCurrBlock->reserve(mBlkSize);
        }
    }

    void end()
    {
        if (mFinished)
        {
            return;
        }
        mFinished = true;
        if (mCurrBlock->size())
        {
            mQueue.put(mCurrBlock);
            mCurrBlock = block_ptr_type();
        }
        mQueue.finish();
    }

    void wait()
    {
        end();
        mThread.join();
        mJoined = true;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("queue", mQueue.stat());
        return t;
    }

    BackgroundBlockConsumer(Consumer& pCons, uint64_t pNumBufItems, uint64_t pBlkSize)
        : mBlkSize(pBlkSize), mQueue(pNumBufItems), mCons(mQueue, pCons),
          mThread(mCons), mFinished(false), mJoined(false)
    {
        mCurrBlock = block_ptr_type(new block_type);
        mCurrBlock->reserve(mBlkSize);
    }

    ~BackgroundBlockConsumer()
    {
        if (!mFinished)
        {
            end();
        }
        if (!mJoined)
        {
            mThread.join();
        }
    }

private:
    const uint64_t mBlkSize;
    BoundedQueue<block_ptr_type> mQueue;
    block_ptr_type mCurrBlock;
    ConsBlockWorker mCons;
    std::thread mThread;
    bool mFinished;
    bool mJoined;
};

#endif // BACKGROUNDBLOCKCONSUMER_HH
