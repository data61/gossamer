#ifndef UNBOUNDEDPROGRESSMONITOR_HH
#define UNBOUNDEDPROGRESSMONITOR_HH

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class UnboundedProgressMonitor
{
public:
    void tick(uint64_t pX)
    {
        if (pX >= mNext)
        {
            mNext += mGap;
            LOG(mLog, info) << pX << mUnit;
        }
    }
    
    void end()
    {
        mLog(info, "done");
    }

    UnboundedProgressMonitor(Logger& pLog, uint64_t pGap, const std::string& pUnit)
        : mLog(pLog), mUnit(pUnit), mGap(pGap), mNext(mGap)
    {
    }

private:
    Logger& mLog;
    std::string mUnit;
    uint64_t mGap;
    uint64_t mNext;
};

#endif // UNBOUNDEDPROGRESSMONITOR_HH
