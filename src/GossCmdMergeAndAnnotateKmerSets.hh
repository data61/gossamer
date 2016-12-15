// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDMERGEANDANNOTATEKMERSETS_HH
#define GOSSCMDMERGEANDANNOTATEKMERSETS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdMergeAndAnnotateKmerSets : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdMergeAndAnnotateKmerSets(const std::string& pLhs, const std::string& pRhs, const std::string& pOut)
        : mLhs(pLhs), mRhs(pRhs), mOut(pOut)
    {
    }

private:
    const std::string mLhs;
    const std::string mRhs;
    const std::string mOut;
};


class GossCmdFactoryMergeAndAnnotateKmerSets : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryMergeAndAnnotateKmerSets();
};

#endif // GOSSCMDMERGEANDANNOTATEKMERSETS_HH
