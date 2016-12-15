// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
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

#ifndef STD_CHRONO
#include <chrono>
#define STD_CHRONO
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
// tick rate and display precision automatically.
//
class ProgressMonitorNew : public ProgressMonitorBase
{
private:
    typedef std::chrono::steady_clock monotonic_clock;

    Logger& mLog;
    const uint64_t mN;
    const double mInvN;
    const double mUpdateTime;
    uint64_t mX;
    bool mInitialising;
    uint64_t mInitStage;
    double mTicksPerSec;

    monotonic_clock::time_point mStartTime;
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
        using namespace std::chrono;

        auto thisTime = monotonic_clock::now();

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
            mTicksPerSec = mX * 1.0e6
                / duration_cast<microseconds>(thisTime - mStartTime).count();
            mNext = mX + std::floor(mTicksPerSec * mUpdateTime);
            return;
        }

        double ticksPerSec = mX * 1.0e6
            / duration_cast<microseconds>(thisTime - mStartTime).count();
        double newPrec = std::max(0.0,
                (double)std::ceil(-std::log10(mInvN * (mX - mPrev)) - 2));

        mTicksPerSec = 0.5 * mTicksPerSec + 0.5 * ticksPerSec;
        mPrec = 0.8 * mPrec + 0.2 * newPrec;

        mNext = mX + std::floor(mTicksPerSec * mUpdateTime);
        mPrev = mX;
    }

    bool timeToReport() const
    {
        if (mInitialising) {
            using namespace std::chrono;

            double rightNow = 1.0e-6 * duration_cast<microseconds>(
                monotonic_clock::now() - mStartTime
            ).count();
            return rightNow >= mUpdateTime * mInitStage;
        }
        else {
            return mX >= mNext;
        }
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
        mStartTime = monotonic_clock::now();
    }
};


#endif // PROGRESSMONITOR_HH
