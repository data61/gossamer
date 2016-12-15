// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDTHREADPAIRS_HH
#define GOSSCMDTHREADPAIRS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

#ifndef PAIRLINKER_HH
#include "PairLinker.hh"
#endif

class GossCmdThreadPairs : public GossCmd
{
public:

    typedef PairLinker::Orientation Orientation;

    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdThreadPairs(const std::string& pIn,
                       const strings& pFastas, const strings& pFastqs, const strings& pLines,
                       uint64_t pMinLinkCount, bool pInferCoverage, uint64_t pExpectedCoverage,
                       uint64_t pExpectedInsertSize, double pInsertStdDevFactor, double pInsertTolerance,
                       Orientation pOrientation, uint64_t pNumThreads, 
                       uint64_t pCacheRate, uint64_t pSearchRadius,
                       bool pEstimateOnly, bool pConsolidatePaths, bool pFillGaps,
                       uint64_t pMaxGap, bool pRemScaf)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mMinLinkCount(pMinLinkCount),
          mInferCoverage(pInferCoverage),
          mExpectedCoverage(pExpectedCoverage),
          mExpectedInsertSize(pExpectedInsertSize), mInsertStdDevFactor(pInsertStdDevFactor),
          mInsertTolerance(pInsertTolerance), mOrientation(pOrientation), mNumThreads(pNumThreads),
          mCacheRate(pCacheRate), mSearchRadius(pSearchRadius),
          mEstimateOnly(pEstimateOnly), mConsolidatePaths(pConsolidatePaths), mFillGaps(pFillGaps),
          mMaxGap(pMaxGap), mRemScaf(pRemScaf)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const uint64_t mMinLinkCount;
    const bool mInferCoverage;
    const uint64_t mExpectedCoverage;
    const uint64_t mExpectedInsertSize;
    const double mInsertStdDevFactor;
    const double mInsertTolerance;
    const Orientation mOrientation;
    const uint64_t mNumThreads;
    const uint64_t mCacheRate;
    const uint64_t mSearchRadius;
    const bool mEstimateOnly;
    const bool mConsolidatePaths;
    const bool mFillGaps;
    const uint64_t mMaxGap;
    const bool mRemScaf;
};

class GossCmdFactoryThreadPairs : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryThreadPairs();
};

#endif // GOSSCMDTHREADPAIRS_HH
