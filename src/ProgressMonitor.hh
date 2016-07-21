#ifndef PROGRESSMONITOR_HH
#define PROGRESSMONITOR_HH

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

#ifndef STD_CMATH
#include <cmath>
#define STD_CMATH
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef STD_CMATH
#include <cmath>
#define STD_CMATH
#endif

#ifndef STD_IOMANIP
#include <iomanip>
#define STD_IOMANIP
#endif

#ifndef STD_SSTREAM
#include <sstream>
#define STD_SSTREAM
#endif

#ifndef BOOST_TIMER_HPP
#include <boost/timer.hpp>
#define BOOST_TIMER_HPP
#endif


class ProgressMonitorBase
{
public:
    virtual void tick(uint64_t pX) = 0;
    virtual void end() = 0;
};


class ProgressMonitor : public ProgressMonitorBase
{
public:
    template <typename Actor>
    void tick(uint64_t pX, Actor& pAct)
    {
        if (pX >= mNext)
        {
            double n = mN;
            double p = 100 * pX / n;
            log(p);
            pAct();
            mNext = mTick * (1 + (pX + mTick - 1) / mTick);
        }
    }
    
    void tick(uint64_t pX)
    {
        if (pX >= mNext)
        {
            double n = mN;
            double p = 100 * pX / n;
            log(p);
            mNext = mTick * (1 + (pX + mTick - 1) / mTick);
        }
    }
    
    void end()
    {
        double n = mN;
        double p = 100 * mNext / n;
        log(p);
    }

    ProgressMonitor(Logger& pLog, uint64_t pN, uint64_t pDivisions)
        : mLog(pLog), mNext(0), mN(pN + 1), mTick(1 + mN / pDivisions),
          mPrec(std::max(0.0, (double)std::ceil(std::log10((long double)pDivisions)) - 2))
    {
    }

private:
    
    void log(double p)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(mPrec) << p << '%';
        mLog(info, ss.str());
    }

    Logger& mLog;
    uint64_t mNext;
    const uint64_t mN;
    const uint64_t mTick;
    const uint64_t mPrec;
};


// Experimental progress monior which attempts to find the correct
// tick rate automatically.
//
class ProgressMonitorNew : public ProgressMonitorBase
{
private:
    Logger& mLog;
    const uint64_t mN;
    const double mInvN;
    const double mUpdateTime;
    uint64_t mX;
    bool mInitialising;
    uint64_t mInitStage;
    double mTicksPerSec;

    double mStartTime;
    uint64_t mPrev;
    uint64_t mNext;
    double mPrec;

    void report() const
    {
        double p = 100.0 * mX / mN;
        log(p);
    }

    void recalculateNext()
    {
        double thisTime = Gossamer::monotonicRealTimeClock();

        if (mInitialising)
        {
            mPrec = 0.8 * std::max(0.0,
                (double)std::ceil(-std::log10(mInvN * (mX - mPrev))) - 2)
                + 0.2 * mPrec;
            mPrev = mX;
            if (++mInitStage <= 3)
            {
                return;
            }
            mInitialising = false;
            mTicksPerSec = mX / (thisTime - mStartTime);
            mNext = mX + std::floor(mTicksPerSec * mUpdateTime);
            return;
        }

        double ticksPerSec = mX / (thisTime - mStartTime);
        double newPrec = std::max(0.0,
                (double)std::ceil(-std::log10(mInvN * (mX - mPrev)) - 2));

        mTicksPerSec = 0.5 * mTicksPerSec + 0.5 * ticksPerSec;
        mPrec = 0.8 * mPrec + 0.2 * newPrec;

        mNext = mX + std::floor(mTicksPerSec * mUpdateTime);
        mPrev = mX;
    }

    bool timeToReport() const
    {
        return mInitialising
            ? Gossamer::monotonicRealTimeClock() >=
                mStartTime + mUpdateTime * mInitStage
            : mX >= mNext;
    }

    void log(double p) const
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(std::ceil(mPrec)) << p << '%';
        mLog(info, ss.str());
    }

public:

    template <typename Actor>
    void tick(uint64_t pX, Actor& pAct)
    {
        mX = pX;
        if (timeToReport())
        {
            report();
            pAct();
            recalculateNext();
        }
    }
    
    void tick(uint64_t pX)
    {
        mX = pX;
        if (timeToReport())
        {
            report();
            recalculateNext();
        }
    }

    void end()
    {
        report();
    }

    ProgressMonitorNew(Logger& pLog, uint64_t pN, double pUpdateTime = 5.0)
        : mLog(pLog), mN(pN + 1), mInvN(1.0 / mN), mUpdateTime(pUpdateTime),
          mX(0), mInitialising(true), mInitStage(1),
          mTicksPerSec(1.0 / pUpdateTime),
          mPrev(0), mNext(0), mPrec(0)
    {
        mStartTime = Gossamer::monotonicRealTimeClock();
    }
};


#endif // PROGRESSMONITOR_HH
