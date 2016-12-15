// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDCOUNTCOMPONENTS_HH
#define GOSSCMDCOUNTCOMPONENTS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdCountComponents : public GossCmd
{
    typedef std::vector<std::string> strings;

public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdCountComponents(const std::string& pIn, const std::string& pOut,
                           const strings& pFastaNames, const strings& pFastqNames, const strings& pLineNames)
        : mIn(pIn), mOut(pOut),
          mFastaNames(pFastaNames), mFastqNames(pFastqNames), mLineNames(pLineNames)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mLineNames;
};


class GossCmdFactoryCountComponents : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryCountComponents();
};

#endif // GOSSCMDCOUNTCOMPONENTS_HH
