// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef LOGGER_HH
#define LOGGER_HH

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef STD_TIME_H
#include <time.h>
#define STD_TIME_H
#endif

#define LOG(L, S)       if ((L).sev() <= S) (L).stream(S, true)()
#define LOGSTREAM(L, S) if ((L).sev() <= S) (L).stream(S, false)()

class Logger
{
public:

    class Stream
    {
    public:

        std::ostream& operator()()
        {
            return mOut;
        }

        Stream(std::ostream& pOut)
            : mOut(pOut)
        {
        }

        ~Stream()
        {
            mOut << std::endl;
            mOut.flush();
        }

    private:
        std::ostream& mOut;
    };

    void timestamp(Severity pSev)
    {
        mOut << now() << '\t' << boost::lexical_cast<const char*>(pSev) << '\t';
    }

    Stream stream(Severity pSev, bool pStamp)
    {
        if (pStamp)
        {
            timestamp(pSev);
        }
        return Stream(mOut);
    }

    const Severity& sev() const
    {
        return mSev;
    }

    Severity& sev()
    {
        return mSev;
    }

    Logger& operator()(Severity pSev, const std::string& pStr)
    {
        if (mSev > pSev)
        {
            return *this;
        }
        timestamp(pSev);
        mOut << pStr
             << std::endl;
        return *this;
    }

    void sync()
    {
        mOut.flush();
    }

    Logger(const std::string& pName, FileFactory& pFactory, Severity pSev = info)
        : mOutHolder(pFactory.out(pName)), mOut(**mOutHolder), mSev(pSev)
    {
    }

    Logger(std::ostream& pOut, Severity pSev = info)
        : mOut(pOut), mSev(pSev)
    {
    }
private:

    const std::string& now()
    {
        time_t rt;
        time(&rt);
        struct tm* ti;
        ti = localtime(&rt);
        mNow = asctime(ti);
        mNow.erase(mNow.size() - 1); // drop the \n
        return mNow;
    }

    FileFactory::OutHolderPtr mOutHolder;
    std::ostream& mOut;
    Severity mSev;
    std::string mNow;
};

typedef std::shared_ptr<Logger> LoggerPtr;

#endif // LOGGER_HH
