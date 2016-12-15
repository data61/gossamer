// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "EstimateGraphStatistics.hh"
#include "LevenbergMarquardt.hh"
#include "GossamerException.hh"
#include "Utils.hh"
#include <vector>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/math/distributions/chi_squared.hpp>
#include <cmath>
#include <sstream>

using namespace std;
using namespace boost;


namespace
{

    LevenbergMarquardt::vec_t
    kmerModel(const LevenbergMarquardt::params_t& pParams,
              const LevenbergMarquardt::vec_t& pX)
    {
        LevenbergMarquardt::vec_t ys(pX.size());

        double mix = pParams[0];
        double lambda = pParams[1];
        double mean = pParams[2];
        double stddev = pParams[3];

        if (stddev < 0.0 || lambda < 0.0
                || mix < 0.0 || mix > 1.0 || mean < 0.0)
        {
            throw LevenbergMarquardt::DomainError();
        }

        boost::math::poisson poissDist(lambda);
        boost::math::normal normDist(mean,stddev);

        double massAtZero = mix * pdf(poissDist, 0)
            + (1.0 - mix) * pdf(normDist, 0);

        double scale = 1000.0 / (1.0 - massAtZero);

        for (uint64_t i = 0; i < pX.size(); ++i)
        {
            ys[i] = scale * (mix * pdf(poissDist, pX[i])
                             + (1.0 - mix) * pdf(normDist, pX[i]));
        }
        return ys;
    };

    LevenbergMarquardt::vec_t
    gaussianModel(const LevenbergMarquardt::params_t& pParams,
                  const LevenbergMarquardt::vec_t& pX)
    {
        LevenbergMarquardt::vec_t ys(pX.size());

        double w = pParams[0];
        double mean = pParams[1];
        double stddev = pParams[2];

        if (stddev < 0.0 || w < 0.0)
        {
            throw LevenbergMarquardt::DomainError();
        }

        boost::math::normal normDist(mean,stddev);

        for (uint64_t i = 0; i < pX.size(); ++i)
        {
            ys[i] = w * pdf(normDist, pX[i]);
        }
        return ys;
    };


    struct CleanedUpData
    {
        uint64_t mTotalRhomerCount;
        double mScale;
        vector<pair<double,double> > mData;
        vector<double> mInitialParams;

        CleanedUpData(const map<uint64_t,uint64_t>& pHisto,
                      double pOutlierLimit)
            : mInitialParams(4)
        {
            if (pHisto.size() < 50)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info(
                        "Not enough data to estimate coverage."));
            }

            uint64_t rhomerCount = 0;
            for (map<uint64_t,uint64_t>::const_iterator ii = pHisto.begin();
                 ii != pHisto.end(); ++ii)
            {
                rhomerCount += ii->second;
            }
            mTotalRhomerCount = rhomerCount;
            double scale = 1000.0 / rhomerCount;
            uint64_t outlierCutoff
                = (uint64_t)(rhomerCount * pOutlierLimit + 0.99);

            uint64_t newMaxX = 0;
            uint64_t c = 0;
            for (map<uint64_t,uint64_t>::const_iterator ii = pHisto.begin();
                 ii != pHisto.end(); ++ii)
            {
                if (ii->first > newMaxX)
                {
                    newMaxX = ii->first;
                }
                mData.push_back(pair<double,double>(
                        ii->first, ii->second * scale));
                c += ii->second;
                if (c > outlierCutoff)
                {
                    break;
                }
            }
            mScale = scale;
            mInitialParams[0] = 0.5;
            mInitialParams[1] = 1.0;
            mInitialParams[2] = newMaxX * 0.5;
            mInitialParams[3] = newMaxX * 0.25;
        }
    };


    struct Evaluation
    {
        uint64_t mDegreesOfFreedom;
        double mChiSq;
        std::vector<double> mEstParams;
        std::vector<double> mEstStdDev;
        bool mEvaluationOk;
 
        bool ok() const { return mEvaluationOk; }

        Evaluation(Logger& pLog, const CleanedUpData& pData,
                   const LevenbergMarquardt& pSolver)
        {
            mEvaluationOk = pSolver.evaluate(pLog, mEstParams, mEstStdDev, mChiSq);
            mDegreesOfFreedom = pData.mData.size() - mEstParams.size();
        }
    };

    string
    errorBars(double pX, double pDelta)
    {
        stringstream s;
        int precisionD = static_cast<int>(-log(pDelta) / log(10.0));
        double prec = pow(10.0, precisionD + 2.0);
        double x = (int64_t)(pX * prec + 0.5) / prec;
        double d = (int64_t)(pDelta * prec + 0.5) / prec;
        s << x << "(+/- " << d << ')';
        return s.str();
    }
}

struct EstimateGraphStatistics::Impl
{
    Logger& mLog;
    CleanedUpData mData;
    LevenbergMarquardt mSolver;
    Evaluation mEvaluation;
    double mGoodRhomerCutoff;
    uint64_t mTrimPoint;
    double mEstimatedCoverage;

    Impl(Logger& pLog, const map<uint64_t,uint64_t>& pHisto, double pOutlierLimit)
        : mLog(pLog), mData(pHisto, pOutlierLimit),
          mSolver(kmerModel, mData.mInitialParams, mData.mData),
          mEvaluation(pLog, mData, mSolver)
    {
    }

    void calculateEstimates(double pGoodRhomerCutoff)
    {
        mGoodRhomerCutoff = pGoodRhomerCutoff;

        // double mix = mEvaluation.mEstParams[0];
        // double lambda = mEvaluation.mEstParams[1];
        double mean = mEvaluation.mEstParams[2];
        double stddev = mEvaluation.mEstParams[3];

        boost::math::normal normDist(mean,stddev);

        double leftTailMass = cdf(normDist, 0);
        double rhomerCutoff = quantile(normDist,
                                pGoodRhomerCutoff + leftTailMass);
        if (rhomerCutoff < 0.0)
        {
            rhomerCutoff = 0.0;
        }
        mTrimPoint = rhomerCutoff;
        mEstimatedCoverage = mean;
    }
};

EstimateGraphStatistics::EstimateGraphStatistics(
        Logger& pLog, const map<uint64_t,uint64_t>& pHisto,
        double pGoodRhomerCutoff, double pOutlierLimit)
    : mPImpl(new Impl(pLog, pHisto, pOutlierLimit))
{
    if (modelFits())
    {
        mPImpl->calculateEstimates(pGoodRhomerCutoff);
    }
}


void
EstimateGraphStatistics::report(uint64_t pK) const
{
    using namespace boost::math;
    using namespace Gossamer;

    Logger& log(mPImpl->mLog);

    if (!modelFits())
    {
        log(info, "Could not estimate paramters.");
        return;
    }

    Evaluation& eval = mPImpl->mEvaluation;

    double mix = eval.mEstParams[0];
    double lambda = eval.mEstParams[1];
    double mean = eval.mEstParams[2];
    double stddev = eval.mEstParams[3];

    double mixSD = eval.mEstStdDev[0];
    double lambdaSD = eval.mEstStdDev[1];
    double meanSD = eval.mEstStdDev[2];
    double stddevSD = eval.mEstStdDev[3];

    poisson poissDist(lambda + lambdaSD);
    normal normDist(mean, stddev - stddevSD);

    uint64_t trimPoint = mPImpl->mTrimPoint;

    double normalMassAtZero = cdf(normDist, 0);
    double poissMassAtZero = cdf(poissDist, 0);

    log(info, "Estimates at 99% confidence interval:");
    log(info, "    Proportion of error rho-mers = "
        + errorBars(100 * mix, 3 * 100 * mixSD) + "%");
    log(info, "    rho-mer coverage = " + errorBars(mean, 3 * meanSD));
    log(info, "Estimated trim point = "
        + lexical_cast<string>(trimPoint));

    double adjustedTrimPoint = trimPoint > 0 ? trimPoint - 0.5 : trimPoint;

    double trimmedFromNorm
        = 100.0 * (cdf(normDist, adjustedTrimPoint) - normalMassAtZero)
                / (1.0 - normalMassAtZero);
    double trimmedFromPoiss
        = 100.0 * (cdf(poissDist, adjustedTrimPoint) - poissMassAtZero)
                / (1.0 - poissMassAtZero);
    log(info, "Estimated real kmers to be trimmed = "
        + lexical_cast<string>(clamp(0.0, trimmedFromNorm, 100.0)) + "%");
    log(info, "Estimated error kmers to be trimmed = "
        + lexical_cast<string>(clamp(0.0, trimmedFromPoiss, 100.0)) + "%");
}


bool
EstimateGraphStatistics::modelFits() const
{
    Evaluation& eval = mPImpl->mEvaluation;
    if (!eval.ok())
    {
        return false;
    }
    boost::math::chi_squared chiSqDist(eval.mDegreesOfFreedom);
    return eval.mChiSq < quantile(chiSqDist, 0.99);
}


uint64_t
EstimateGraphStatistics::estimateTrimPoint() const
{
    return mPImpl->mTrimPoint;
}


double
EstimateGraphStatistics::estimateRhomerCoverage() const
{
    return mPImpl->mEstimatedCoverage;
}


struct EstimateCoverageOnly::Impl
{
    static const size_t sMinPoints = 50;
    bool mModelFit;
    double mEstimatedCoverage;

    Impl(Logger& pLog, const map<uint64_t,uint64_t>& pHisto, double pOutlierLimit)
        : mModelFit(false)
    {
        if (pHisto.size() < sMinPoints)
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info(
                    "Not enough data to estimate coverage."));
        }

        uint64_t estCoverageX = 0;
        uint64_t estCoverageY = 0;
        mModelFit = false;
        uint64_t prevX = 0;
        uint64_t prevY = 0;
        uint64_t i = 0;
        for (map<uint64_t,uint64_t>::const_iterator ii = pHisto.begin();
             i < sMinPoints; ++i, ++ii)
        {
            uint64_t x = ii->first;
            uint64_t y = ii->second;

            if (i < 1)
            {
                prevX = x;
                prevY = y;
                continue;
            }

            if (prevX + 1 != x)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info(
                        "Coverage histogram appears to be discontinuous. "
                        "Cannot estimate coverage."));
            }

            if (prevY < y)
            {
                mModelFit = true;
            }
            if (mModelFit && estCoverageY < y)
            {
                estCoverageX = x;
                estCoverageY = y;
            }

            prevX = x;
            prevY = y;
        }
        mEstimatedCoverage = estCoverageX;
    }
};


bool
EstimateCoverageOnly::modelFits() const
{
    return mPImpl->mModelFit;
}


double
EstimateCoverageOnly::estimateRhomerCoverage() const
{
    return mPImpl->mEstimatedCoverage;
}


EstimateCoverageOnly::EstimateCoverageOnly(
        Logger& pLog,
        const map<uint64_t,uint64_t>& pHisto,
        double pOutlierLimit)
    : mPImpl(new Impl(pLog, pHisto, pOutlierLimit))
{
}

