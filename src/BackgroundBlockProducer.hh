// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef BACKGROUNDBLOCKPRODUCER_HH
#define BACKGROUNDBLOCKPRODUCER_HH

#ifndef BOUNDEDQUEUE_HH
#include "BoundedQueue.hh"
#endif

template <typename Producer>
class BackgroundBlockProducer
{
public:
    typedef typename Producer::value_type value_type;
    typedef Deque<value_type> Block;
    typedef std::shared_ptr<Block> BlockPtr;

    class ProdWorker
    {
    public:
        void operator()()
        {
            BlockPtr blk(new Block);
            blk->reserve(mBlkSz);

            while (mProd.valid())
            {
                blk->push_back(*mProd);
                if (blk->size() == mBlkSz)
                {
                    mQueue.put(blk);
                    blk = BlockPtr(new Block);
                    blk->reserve(mBlkSz);
                }
                ++mProd;
            }
            if (blk->size() > 0)
            {
                mQueue.put(blk);
                blk = BlockPtr();
            }
            mQueue.finish();
        }

        ProdWorker(BoundedQueue<BlockPtr>& pQueue, Producer& pProd, uint64_t pBlkSz)
            : mQueue(pQueue), mProd(pProd), mBlkSz(pBlkSz)
        {
        }

    private:
        BoundedQueue<BlockPtr>& mQueue;
        Producer& mProd;
        uint64_t mBlkSz;
    };

    bool valid() const
    {
        return mValid && mItems->size();
    }

    const value_type& operator*() const
    {
        return mItems->front();
    }

    void operator++()
    {
        mItems->pop_front();
        while (mValid && mItems->size() == 0)
        {
            mValid = mQueue.get(mItems);
        }
    }

    BackgroundBlockProducer(Producer& pProd, uint64_t pNumBufItems, uint64_t pBlkSz)
        : mQueue(pNumBufItems), mProd(mQueue, pProd, pBlkSz), mThread(mProd)
    {
        mValid = mQueue.get(mItems);
        while (mValid && mItems->size() == 0)
        {
            mValid = mQueue.get(mItems);
        }
    }

    ~BackgroundBlockProducer()
    {
        if (mValid)
        {
            // XXX interrupt support?
        }
        mThread.join();
    }

private:
    BoundedQueue<BlockPtr> mQueue;
    ProdWorker mProd;
    std::thread mThread;
    bool mValid;
    BlockPtr mItems;
};



#endif // BACKGROUNDBLOCKPRODUCER_HH
