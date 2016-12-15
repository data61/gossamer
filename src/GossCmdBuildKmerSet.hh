// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDBUILDKMERSET_HH
#define GOSSCMDBUILDKMERSET_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildKmerSet : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    template<typename KmerSrc> void operator()(const GossCmdContext& pCxt, KmerSrc& pKmerSrc);

    GossCmdBuildKmerSet(const uint64_t& pK, const uint64_t& pS, const uint64_t& pN,
                        const uint64_t& pT, const std::string& pKmerSetName)
        : mK(pK), mS(pS), mN(pN), mT(pT), mKmerSetName(pKmerSetName),
          mFastaNames(), mFastqNames(), mLineNames()
    {
    }

    GossCmdBuildKmerSet(const uint64_t& pK, const uint64_t& pS, const uint64_t& pN,
                      const uint64_t& pT, const std::string& pKmerSetName,
                      const strings& pFastaNames, const strings& pFastqNames, const strings& pLineNames)
        : mK(pK), mS(pS), mN(pN), mT(pT), mKmerSetName(pKmerSetName),
          mFastaNames(pFastaNames), mFastqNames(pFastqNames), mLineNames(pLineNames)
    {
    }

private:
    const uint64_t mK;
    const uint64_t mS;
    const uint64_t mN;
    const uint64_t mT;
    const std::string mKmerSetName;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mLineNames;
};

class GossCmdFactoryBuildKmerSet : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildKmerSet();
};

#include "GossCmdBuildKmerSet.tcc"

#endif // GOSSCMDBUILDKMERSET_HH
