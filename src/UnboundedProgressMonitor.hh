// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
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
