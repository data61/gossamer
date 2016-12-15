// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef APP_HH
#define APP_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef GOSSOPTION_HH
#include "GossOption.hh"
#endif

#ifndef GOSSCMDCONTEXT_HH
#include "GossCmdContext.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

class App
{
public:

    virtual const char* name() const = 0;

    virtual const char* version() const = 0;

    FileFactory& fileFactory();

    Logger& logger();

    void help(bool pExit = true);

    virtual int main(int argc, char* argv[]);

    App(const GossOptions& pGlobalOpts, const GossOptions& pCommonOpts)
        : mGlobalOpts(pGlobalOpts), mCommonOpts(pCommonOpts)
    {
    }

protected:
    
    const GossOptions& mGlobalOpts;
    const GossOptions& mCommonOpts;
};

#endif
