#ifndef GOSSREADDISPATCHER_HH
#define GOSSREADDISPATCHER_HH

#ifndef BACKGROUNDMULTICONSUMER_HH
#include "BackgroundMultiConsumer.hh"
#endif

#ifndef GOSSREADHANDLER_HH
#include "GossReadHandler.hh"
#endif

#ifndef STD_UTILITY
#define STD_UTILITY
#include <utility>
#endif

#ifndef STD_VECTOR
#define STD_VECTOR
#include <vector>
#endif

class GossReadDispatcher : public GossReadHandler
{
public:

    void operator()(const GossRead& pRead)
    {
        mConsumer.push_back(pRead.clone());
    }

    void operator()(const GossRead& pLhs, const GossRead& pRhs)
    {
        throw 2;
    }

    void end()
    {
        mConsumer.wait();
    }

    GossReadDispatcher(const std::vector<GossReadHandlerPtr>& pHandlers)
        : mHandlers(pHandlers), mConsumer(1024)
    {
        for (uint64_t i = 0; i < mHandlers.size(); ++i)
        {
            mConsumer.addApply(*mHandlers[i]);
        }
    }

private:
    const std::vector<GossReadHandlerPtr> mHandlers;
    BackgroundMultiConsumer<GossReadPtr> mConsumer;
};

class GossPairDispatcher : public GossReadHandler
{
public:

    void operator()(const GossRead& pRead)
    {
        throw 1;
    }

    void operator()(const GossRead& pLhs, const GossRead& pRhs)
    {
        mConsumer.push_back(std::make_pair(pLhs.clone(), pRhs.clone()));
    }

    void end()
    {
        mConsumer.wait();
    }

    GossPairDispatcher(const std::vector<GossReadHandlerPtr>& pHandlers)
        : mHandlers(pHandlers), mConsumer(1024)
    {
        for (uint64_t i = 0; i < mHandlers.size(); ++i)
        {
            mConsumer.addApply(*mHandlers[i]);
        }
    }

private:
    const std::vector<GossReadHandlerPtr> mHandlers;
    BackgroundMultiConsumer<std::pair<GossReadPtr, GossReadPtr> > mConsumer;
};

#endif // GOSSREADDISPATCHER_HH
