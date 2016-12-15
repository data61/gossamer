// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDCONTEXT_HH
#define GOSSCMDCONTEXT_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

#ifndef BOOST_PROGRAM_OPTIONS_HH
#include <boost/program_options.hpp>
#define BOOST_PROGRAM_OPTIONS_HH
#endif

struct GossCmdContext
{
    FileFactory& fac;
    Logger& log;
    const std::string& cmdName;
    const boost::program_options::variables_map& opts;

    GossCmdContext(FileFactory& pFactory,
                   Logger& pLogger,
                   const std::string& pCmdName,
                   const boost::program_options::variables_map& pOpts)
        : fac(pFactory), log(pLogger), cmdName(pCmdName), opts(pOpts)
    {
    }
};

#endif // GOSSCMDCONTEXT_HH
