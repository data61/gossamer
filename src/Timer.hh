// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef TIMER_HH
#define TIMER_HH

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

#if defined(__GNUC__)
#ifndef STD_SYS_TIME_H
#include <sys/time.h>
#define STD_SYS_TIME_H
#endif
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_TIME_H
#include <time.h>
#define STD_TIME_H
#endif

class Timer
{
public:
    double check() const
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double now = tv.tv_sec + static_cast<double>(tv.tv_usec) / 1000000.0;
        return now - mThen;
    }

    void reset()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mThen = tv.tv_sec + static_cast<double>(tv.tv_usec) / 1000000.0;
    }

    Timer()
    {
        reset();
    }

private:
    double mThen;
};

class CmdTimer : public Timer
{
public:

    ~CmdTimer()
    {
        mLog(info, "total elapsed time: " + boost::lexical_cast<std::string>(check()));
    }

    CmdTimer(Logger& pLog)
        : mLog(pLog)
    {
    }
    
private:
    
    CmdTimer(const CmdTimer&);

    Logger& mLog;
};

#endif // TIMER_HH
