// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "LevenbergMarquardt.hh"
#include "GossamerException.hh"
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <cmath>

#undef DEBUG

namespace
{
    static const double sH = 1e-4;
    static const double sMinLambda = 1e-20;
    static const double sMaxLambda = 1e+20;
    static const double sLambdaUp = 10.0;
    static const double sLambdaDown = 10.0;
    static const double sConvergenceLimit = 1e-6;
    static const size_t sMinPasses = 2;
    static const double sSmallNumber = 1e-30;
}

using namespace boost::numeric::ublas;

LevenbergMarquardt::LevenbergMarquardt(
        const function_t& pFunction,
        const std::vector<double>& pInitialParams,
        const std::vector<std::pair<double,double> >& pData,
        size_t pMaxPasses,
        double pLambda)
    : mDimensions(pInitialParams.size()),
          mPoints(pData.size()),
          mX(pData.size()),
          mY(pData.size()),
          mErrY(pData.size()),
          mParams(pInitialParams.size()),
          mFunction(pFunction),
          mMaxPasses(pMaxPasses),
          mLambda(pLambda)
{
    for (size_t i = 0; i < mDimensions; ++i)
    {
        mParams[i] = pInitialParams[i];
    }
    for (size_t i = 0; i < mPoints; ++i)
    {
        mX[i] = pData[i].first;
        mY[i] = pData[i].second;
        mErrY[i] = 1.0;
    }
    solve();
}


bool
LevenbergMarquardt::evaluate(Logger& pLog, std::vector<double>& pParams,
                             std::vector<double>& pStdDev,
                             double& pChiSq) const
{
#ifdef DEBUG
    std::cerr << "Evaluating\n";
#endif
    matrix<double> jacobian(mPoints, mDimensions);
    vector<double> ys = mFunction(mParams, mX);

    for (size_t i = 0; i < mDimensions; ++i)
    {
        unit_vector<double> p(mDimensions, i);
        matrix_column<matrix<double> >(jacobian, i)
            = element_div((mFunction(mParams + sH * p, mX) - ys), sH * mErrY);
    }
    vector<double> residuals = element_div(ys - mY, mErrY);

    double chiSq = inner_prod(residuals, residuals);

    matrix<double> jacobian2 = prod(trans(jacobian), jacobian);
#ifdef DEBUG
    std::cerr << "Jacobian^2 = " << jacobian2 << "\n";
#endif

    matrix<double> covariance(mDimensions,mDimensions);
    try {
        permutation_matrix<size_t> perm(mDimensions);
        if (lu_factorize(jacobian2, perm))
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info(
                    "Singular matrix in LevenbergMarquardt::evaluate"));
        }
        covariance.assign(identity_matrix<double>(mDimensions));
        lu_substitute(jacobian2, perm, covariance);
#ifdef DEBUG
        std::cerr << "Covariance matrix = " << covariance << "\n";
#endif
    }
    catch (boost::numeric::ublas::internal_logic&)
    {
        // Ill-conditioned matrix. Cross fingers and hope.
        pLog(error, "System appears to be ill-conditioned. Variances may not be reliable.");
        return false;
    }

    pChiSq = chiSq;
    chiSq = std::sqrt(chiSq / (mPoints - mDimensions));
    pParams.resize(mDimensions);
    pStdDev.resize(mDimensions);
    for (size_t i = 0; i < mDimensions; ++i)
    {
        pParams[i] = mParams[i];
        pStdDev[i] = std::sqrt(covariance(i,i)) * chiSq;
#ifdef DEBUG
        std::cerr << "stddev[" << i << "] = " << pStdDev[i] << "\n";
#endif
    }
#ifdef DEBUG
    for (size_t i = 0; i < mDimensions; ++i)
    {
        for (size_t j = 0; j < mDimensions; ++j)
        {
            covariance(i,j) /= pStdDev[i] * pStdDev[j];
        }
    }

    std::cerr << "Correlation matrix = " << covariance << "\n";
    std::cerr << "Final params = " << mParams << "\n";
    std::cerr << "Final chisq = " << pChiSq << "\n";
#endif
    return true;
}


void
LevenbergMarquardt::solve()
{
    size_t i = 0;
    bool converged;
    do
    {
#ifdef DEBUG
        std::cerr << "Pass " << i << "\n";
#endif
        converged = pass();
        ++i;
    } while ((i < sMinPasses)
              || ((i < mMaxPasses) && (mLambda < sMaxLambda) && !converged));
}


bool
LevenbergMarquardt::pass()
{
    matrix<double> jacobian(mPoints, mDimensions);

    vector<double> ys = mFunction(mParams, mX);

    for (size_t i = 0; i < mDimensions; ++i)
    {
        unit_vector<double> p(mDimensions, i);
        matrix_column<matrix<double> >(jacobian, i)
               = element_div(mFunction(mParams + sH * p, mX) - ys, sH * mErrY);
    }

    vector<double> residuals = element_div(ys - mY, mErrY);
    double prevErr = inner_prod(residuals, residuals);

#ifdef DEBUG
    std::cerr << "Start of pass\n";
    std::cerr << "Old err = " << prevErr << "\n";
    std::cerr << "Old params = " << mParams << "\n";
    std::cerr << "lambda = " << mLambda << "\n";
#endif

#ifdef DEBUG
//    std::cerr << "residuals = " << residuals << "\n";
//    std::cerr << "Jacobian matrix = " << jacobian << "\n";
#endif

    matrix<double> jacobian2 = prod(trans(jacobian), jacobian);
    vector<double> gradient = prod(trans(jacobian), residuals);
#ifdef DEBUG
    std::cerr << "Jacobian^2 = " << jacobian2 << "\n";
    std::cerr << "Gradient = " << gradient << "\n";
#endif
    for (size_t i = 0; i < mDimensions; ++i)
    {
        // Levenberg's correction.
        jacobian2(i,i) += mLambda * jacobian2(i,i);
    }
#ifdef DEBUG
    std::cerr << "A = " << jacobian2 << "\n";
#endif

    try {
        permutation_matrix<size_t> perm(mDimensions);
        if (lu_factorize(jacobian2, perm))
        {
#ifdef DEBUG
            std::cerr << "Singular matrix; rejecting this step.\n";
#endif
            mLambda /= sLambdaDown;
            return false;
        }
        lu_substitute(gradient, jacobian2, perm);
    }
    catch (boost::numeric::ublas::internal_logic&)
    {
        // This can happen if the matrix is ill conditioned.
#ifdef DEBUG
        std::cerr << "Near-singular matrix; rejecting this step.\n";
#endif
        mLambda /= sLambdaDown;
        return false;
    }
#ifdef DEBUG
    std::cerr << "Delta = " << gradient << "\n";
#endif
    vector<double> newParams = mParams - gradient;
#ifdef DEBUG
    std::cerr << "New params = " << newParams << "\n";
#endif

    try
    {
        residuals = element_div(mFunction(newParams, mX) - mY, mErrY);
        vector<double> p(mDimensions);
        for (size_t i = 0; i < mDimensions; ++i)
        {
            p[i] = sH;
        }
        mFunction(mParams + p, mX);
    }
    catch (DomainError err)
    {
#ifdef DEBUG
        std::cerr << "Domain error; rejecting this step.\n";
#endif
        mLambda *= sLambdaUp;
        return false;
    }
    catch (...)
    {
        throw;
    }
    
    double newErr = inner_prod(residuals, residuals);

#ifdef DEBUG
    std::cerr << "New err = " << newErr << "\n";
#endif
#ifdef DEBUG
    std::cerr << "End of pass\n";
#endif

    if (newErr < prevErr)
    {
        mParams = newParams;
        if (mLambda > sMinLambda)
        {
            mLambda /= sLambdaDown;
        }
        double relErr = prevErr - newErr;
        if (newErr > sSmallNumber)
        {
            relErr /= newErr;
        }
        return relErr < sConvergenceLimit;
    }
    else
    {
        mLambda *= sLambdaUp;
        return false;
    }
}


