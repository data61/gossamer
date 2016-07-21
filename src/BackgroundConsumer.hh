#ifndef BACKGROUNDCONSUMER_HH
#define BACKGROUNDCONSUMER_HH

#ifndef BOUNDEDQUEUE_HH
#include "BoundedQueue.hh"
#endif

template <typename Consumer>
class BackgroundConsumer
{
public:
    typedef typename Consumer::value_type value_type;

    class ConsWorker
    {
    public:
        void operator()()
        {
            value_type val;
            while (mQueue.get(val))
            {
                mCons.push_back(val);
            }
        }

        ConsWorker(BoundedQueue<value_type>& pQueue, Consumer& pCons)
            : mQueue(pQueue), mCons(pCons)
        {
        }

    private:
        BoundedQueue<value_type>& mQueue;
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
        mThread.join();
        mJoined = true;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("queue", mQueue.stat());
        return t;
    }

    BackgroundConsumer(Consumer& pCons, uint64_t pNumBufItems)
        : mQueue(pNumBufItems), mCons(mQueue, pCons), mThread(mCons), mFinished(false), mJoined(false)
    {
    }

    ~BackgroundConsumer()
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
    BoundedQueue<value_type> mQueue;
    ConsWorker mCons;
    boost::thread mThread;
    bool mFinished;
    bool mJoined;
};

#endif // BACKGROUNDCONSUMER_HH
