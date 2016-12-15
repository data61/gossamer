// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDBUILDENTRYEDGESET_HH
#define GOSSCMDBUILDENTRYEDGESET_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildEntryEdgeSet : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildEntryEdgeSet(const std::string& pIn, const uint64_t& pThreads)
        : mIn(pIn), mThreads(pThreads)
    {
    }

private:
    const std::string mIn;
    const uint64_t mThreads;
};

class GossCmdFactoryBuildEntryEdgeSet : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildEntryEdgeSet();
};

#endif // GOSSCMDBUILDENTRYEDGESET_HH
