// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDCOMPUTENEARKMERS_HH
#define GOSSCMDCOMPUTENEARKMERS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdComputeNearKmers : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdComputeNearKmers(const std::string& pIn, uint64_t pNumThreads)
        : mIn(pIn), mNumThreads(pNumThreads)
    {
    }

private:
    const std::string mIn;
    const uint64_t mNumThreads;
};


class GossCmdFactoryComputeNearKmers : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryComputeNearKmers();
};

#endif // GOSSCMDCOMPUTENEARKMERS_HH
