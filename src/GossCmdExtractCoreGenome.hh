// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDEXTRACTCOREGENOME_HH
#define GOSSCMDEXTRACTCOREGENOME_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdExtractCoreGenome : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdExtractCoreGenome(const std::vector<std::string>& pSrcs, const std::string& pDest)
        : mSrcs(pSrcs), mDest(pDest)
    {
    }

private:
    const std::vector<std::string> mSrcs;
    const std::string mDest;
};


class GossCmdFactoryExtractCoreGenome : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryExtractCoreGenome();
};

#endif // GOSSCMDEXTRACTCOREGENOME_HH
