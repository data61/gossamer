// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDPRUNETIPS_HH
#define GOSSCMDPRUNETIPS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdPruneTips : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdPruneTips(const std::string& pIn, const std::string& pOut,
                     boost::optional<uint64_t> pCutoff,
                     boost::optional<double> pRelCutoff,
                     const uint64_t& pThreads, const uint64_t& pIterations)
        : mIn(pIn), mOut(pOut), mCutoff(pCutoff), mRelCutoff(pRelCutoff),
          mThreads(pThreads), mIterations(pIterations)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const boost::optional<uint64_t> mCutoff;
    const boost::optional<double> mRelCutoff;
    const uint64_t mThreads;
    const uint64_t mIterations;
};
class GossCmdFactoryPruneTips : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryPruneTips();
};

#endif // GOSSCMDPRUNETIPS_HH
