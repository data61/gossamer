// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDBUILDEDGEINDEX_HH
#define GOSSCMDBUILDEDGEINDEX_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildEdgeIndex : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildEdgeIndex(const std::string& pIn, uint64_t pNumThreads, uint64_t pCacheRate)
        : mIn(pIn), mNumThreads(pNumThreads), mCacheRate(pCacheRate)
    {
    }

private:
    const std::string mIn;
    const uint64_t mNumThreads;
    const uint64_t mCacheRate;
};


class GossCmdFactoryBuildEdgeIndex : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildEdgeIndex();
};

#endif // GOSSCMDBUILDEDGEINDEX_HH
