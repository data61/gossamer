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

#ifndef BOOST_SHARED_PTR_HPP
#include <boost/shared_ptr.hpp>
#define BOOST_SHARED_PTR_HPP
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
    boost::shared_ptr<Impl> mPImpl;
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
    boost::shared_ptr<Impl> mPImpl;
};

#endif // ESTIMATEGRAPHSTATISTICS_HH
