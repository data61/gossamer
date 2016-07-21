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
