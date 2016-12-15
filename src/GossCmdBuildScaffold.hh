// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDBUILDSCAFFOLD_HH
#define GOSSCMDBUILDSCAFFOLD_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

#ifndef PAIRLINKER_HH
#include "PairLinker.hh"
#endif

class GossCmdBuildScaffold : public GossCmd
{
public:

    typedef PairLinker::Orientation Orientation;

    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildScaffold(const std::string& pIn, 
                         const strings& pFastas, const strings& pFastqs, const strings& pLines,
                         bool pInferCoverage, uint64_t pExpectedCoverage,
                         uint64_t pExpectedInsertSize, double pInsertStdDevFactor, double pInsertTolerance,
                         Orientation pOrientation, uint64_t pNumThreads, 
                         uint64_t pCacheRate, bool pEstimateOnly)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mInferCoverage(pInferCoverage),
          mExpectedCoverage(pExpectedCoverage),
          mExpectedInsertSize(pExpectedInsertSize), mInsertStdDevFactor(pInsertStdDevFactor),
          mInsertTolerance(pInsertTolerance), mOrientation(pOrientation), mNumThreads(pNumThreads),
          mCacheRate(pCacheRate), mEstimateOnly(pEstimateOnly)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const bool mInferCoverage;
    const uint64_t mExpectedCoverage;
    const uint64_t mExpectedInsertSize;
    const double mInsertStdDevFactor;
    const double mInsertTolerance;
    const Orientation mOrientation;
    const uint64_t mNumThreads;
    const uint64_t mCacheRate;
    const bool mEstimateOnly;
};

class GossCmdFactoryBuildScaffold : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildScaffold();
};

#endif // GOSSCMDBUILDSCAFFOLD_HH
