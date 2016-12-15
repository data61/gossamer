// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDMERGE_HH
#define GOSSCMDMERGE_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

template<typename T>
class GossCmdMerge : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdMerge(const strings& pIns, const uint64_t& pMaxMerge, const std::string& pOut)
        : mIns(pIns), mMaxMerge(pMaxMerge), mOut(pOut)
    {
    }

private:

    void merge(const strings& pIn, const std::string& pOut, const GossCmdContext& pCxt);

    const strings mIns;
    const uint64_t mMaxMerge;
    const std::string mOut;
};

template<typename T>
class GossCmdFactoryMerge : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryMerge();
};

#include "GossCmdMerge.tcc"

#endif // GOSSCMDMERGE_HH
