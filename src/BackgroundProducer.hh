#ifndef BACKGROUNDPRODUCER_HH
#define BACKGROUNDPRODUCER_HH

#ifndef BOUNDEDQUEUE_HH
#include "BoundedQueue.hh"
#endif

template <typename Producer>
class BackgroundProducer
{
public:
    typedef typename Producer::value_type value_type;

    class ProdWorker
    {
    public:
        void operator()()
        {
            while (mProd.valid())
            {
                mQueue.put(*mProd);
                ++mProd;
            }
            mQueue.finish();
        }

        ProdWorker(BoundedQueue<value_type>& pQueue, Producer& pProd)
            : mQueue(pQueue), mProd(pProd)
        {
        }

    private:
        BoundedQueue<value_type>& mQueue;
        Producer& mProd;
    };

    bool valid() const
    {
        return mValid;
    }

    const value_type& operator*() const
    {
        return mItem;
    }

    void operator++()
    {
        mValid = mQueue.get(mItem);
    }

    BackgroundProducer(Producer& pProd, uint64_t pNumBufItems)
        : mQueue(pNumBufItems), mProd(mQueue, pProd), mThread(mProd)
    {
        mValid = mQueue.get(mItem);
    }

    ~BackgroundProducer()
    {
        mThread.join();
    }

private:
    BoundedQueue<value_type> mQueue;
    ProdWorker mProd;
    boost::thread mThread;
    bool mValid;
    value_type mItem;
};

#endif // BACKGROUNDPRODUCER_HH
