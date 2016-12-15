// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDANNOTATEKMERS_HH
#define GOSSCMDANNOTATEKMERS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdAnnotateKmers : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdAnnotateKmers(const std::string& pRef, const std::string& pAnnotList, const std::string& pMergeList,
                         uint64_t pNumThreads)
        : mRef(pRef), mAnnotList(pAnnotList), mMergeList(pMergeList), mNumThreads(pNumThreads)
    {
    }

private:
    const std::string mRef;
    const std::string mAnnotList;
    const std::string mMergeList;
    const uint64_t mNumThreads;
};


class GossCmdFactoryAnnotateKmers : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryAnnotateKmers();
};

#endif // GOSSCMDANNOTATEKMERS_HH
