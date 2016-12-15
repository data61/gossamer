// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ESTIMATEGRAPHSTATISTICS_HH
#define ESTIMATEGRAPHSTATISTICS_HH

#ifndef STDINT_H
#include <stdint.h>
#define INT_H
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif


class EstimateGraphStatistics
{
public:
    EstimateGraphStatistics(
                Logger& pLog,
                const std::map<uint64_t,uint64_t>& pHisto,
                double pGoodRhomerCutoff = 0.0001,
                double pOutlierLimit = 0.999);

    void report(uint64_t pK) const;
    bool modelFits() const;
    uint64_t estimateTrimPoint() const;
    double estimateRhomerCoverage() const;

private:
    struct Impl;
    std::shared_ptr<Impl> mPImpl;
};


class EstimateCoverageOnly
{
public:
    EstimateCoverageOnly(
                Logger& pLog,
                const std::map<uint64_t,uint64_t>& pHisto,
                double pOutlierLimit = 0.999);

    bool modelFits() const;
    double estimateRhomerCoverage() const;

private:
    struct Impl;
    std::shared_ptr<Impl> mPImpl;
};

#endif // ESTIMATEGRAPHSTATISTICS_HH
