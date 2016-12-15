// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDTRIMPATHS_HH
#define GOSSCMDTRIMPATHS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdTrimPaths : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdTrimPaths(const std::string& pIn, const std::string& pOut, uint32_t pC, uint64_t pThreads)
        : mIn(pIn), mOut(pOut), mC(pC), mThreads(pThreads)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const uint32_t mC;
    const uint64_t mThreads;
};
class GossCmdFactoryTrimPaths : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryTrimPaths();
};

#endif // GOSSCMDTRIMPATHS_HH
