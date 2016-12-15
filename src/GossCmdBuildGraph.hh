// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDBUILDGRAPH_HH
#define GOSSCMDBUILDGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildGraph : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildGraph(const uint64_t& pK, const uint64_t& pS, const uint64_t& pN,
                      const uint64_t& pT, const std::string& pGraphName,
                      const strings& pFastaNames, const strings& pFastqNames, const strings& pLineNames)
        : mK(pK), mS(pS), mN(pN), mT(pT), mGraphName(pGraphName),
          mFastaNames(pFastaNames), mFastqNames(pFastqNames), mLineNames(pLineNames)
    {
    }

private:
    const uint64_t mK;
    const uint64_t mS;
    const uint64_t mN;
    const uint64_t mT;
    const std::string mGraphName;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mLineNames;
};

class GossCmdFactoryBuildGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildGraph();
};

#endif // GOSSCMDBUILDGRAPH_HH
