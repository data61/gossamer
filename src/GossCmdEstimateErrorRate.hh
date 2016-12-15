// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#ifndef GOSSCMDESTIMATEERRORRATE_HH
#define GOSSCMDESTIMATEERRORRATE_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdEstimateErrorRate : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdEstimateErrorRate(const std::string& pRef)
        : mRef(pRef)
    {
    }

private:
    const std::string mRef;
};


class GossCmdFactoryEstimateErrorRate : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryEstimateErrorRate();
};

#endif // GOSSCMDESTIMATEERRORRATE_HH
