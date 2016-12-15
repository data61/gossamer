// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDTRIMGRAPH_HH
#define GOSSCMDTRIMGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdTrimGraph : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdTrimGraph(const std::string& pIn, const std::string& pOut,
                     uint64_t pC, bool pInferCutoff, bool pEstimateOnly,
                     const boost::optional<uint64_t>& pScaleCutoffByK)
        : mIn(pIn), mOut(pOut), mC(pC),
          mInferCutoff(pInferCutoff), mEstimateOnly(pEstimateOnly),
          mScaleCutoffByK(pScaleCutoffByK)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const uint64_t mC;
    const bool mInferCutoff;
    const bool mEstimateOnly;
    const boost::optional<uint64_t> mScaleCutoffByK;
};


class GossCmdFactoryTrimGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryTrimGraph();
};

#endif // GOSSCMDTRIMGRAPH_HH
