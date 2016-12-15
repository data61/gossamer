// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDBUILDSUBGRAPH_HH
#define GOSSCMDBUILDSUBGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildSubgraph : public GossCmd
{
    typedef std::vector<std::string> strings;

public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildSubgraph(const std::string& pIn, const std::string& pOut,
                         const strings& pFastaNames, const strings& pFastqNames, const strings& pLineNames,
                         uint64_t pRadius, bool pLinearPaths, uint64_t pB)
        : mIn(pIn), mOut(pOut), 
          mFastaNames(pFastaNames), mFastqNames(pFastqNames), mLineNames(pLineNames),
          mRadius(pRadius), mLinearPaths(pLinearPaths), mB(pB)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mLineNames;
    const uint64_t mRadius;
    bool mLinearPaths;
    const uint64_t mB;
};


class GossCmdFactoryBuildSubgraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildSubgraph();
};

#endif // GOSSCMDBUILDSUBGRAPH_HH
