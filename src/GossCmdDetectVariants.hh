// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDDETECTVARIANTS_HH
#define GOSSCMDDETECTVARIANTS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdDetectVariants : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdDetectVariants(const std::string& pRef, const std::string& pTarget)
        : mRef(pRef), mTarget(pTarget)
    {
    }

private:
    const std::string mRef;
    const std::string mTarget;
};


class GossCmdFactoryDetectVariants : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryDetectVariants();
};

#endif // GOSSCMDDETECTVARIANTS_HH
