// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef LEVENBERGMARQUARDT_HH
#define LEVENBERGMARQUARDT_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef BOOST_UTILITY_HPP
#include <boost/utility.hpp>
#define BOOST_UTILITY_HPP
#endif

#ifndef BOOST_FUNCTION_HPP
#include <boost/function.hpp>
#define BOOST_FUNCTION_HPP
#endif

#ifndef BOOST_NUMERIC_UBLAS_VECTOR_HPP
#include <boost/numeric/ublas/vector.hpp>
#define BOOST_NUMERIC_UBLAS_VECTOR_HPP
#endif

#ifndef BOOST_NUMERIC_UBLAS_MATRIX_HPP
#include <boost/numeric/ublas/matrix.hpp>
#define BOOST_NUMERIC_UBLAS_MATRIX_HPP
#endif

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif


class LevenbergMarquardt : private boost::noncopyable
{
public:
    struct DomainError {};

    typedef boost::numeric::ublas::vector<double> params_t;
    typedef boost::numeric::ublas::vector<double> vec_t;
    typedef boost::function<vec_t (const params_t &, const vec_t&)> function_t;

    LevenbergMarquardt(const function_t& pFunction,
                       const std::vector<double>& pInitialParams,
                       const std::vector<std::pair<double,double> >& pData,
                       size_t pMaxPasses = 20000,
                       double pLambda = 10);

    bool evaluate(Logger& pLog, std::vector<double>& pParams,
                  std::vector<double>& pStdDev,
                  double& pChiSq) const;

private:
    void solve();
    bool pass();
    double calculateErr(const params_t& pParams) const;

    size_t mDimensions;
    size_t mPoints;
    boost::numeric::ublas::vector<double> mX;
    boost::numeric::ublas::vector<double> mY;
    boost::numeric::ublas::vector<double> mErrY;
    double mWeight;
    params_t mParams;
    function_t mFunction;
    size_t mMaxPasses;
    double mLambda;
};

#endif // LEVENBERGMARQUARDT_HH
