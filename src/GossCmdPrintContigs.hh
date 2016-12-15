// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDPRINTPATHS_HH
#define GOSSCMDPRINTPATHS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdPrintContigs : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdPrintContigs(const std::string& pIn,
                        uint64_t pC, uint64_t pL, bool pPrintRcs,
                        bool pOmitSequence, bool pPrintLinearSegments,
                        bool pVerboseHeaders, bool pNoLineBreaks,
                        bool pPrintEntailed, uint64_t pNumThreads,
                        const std::string& pOut)
        : mIn(pIn), mC(pC), mL(pL), mPrintRcs(pPrintRcs),
          mOmitSequence(pOmitSequence), mPrintLinearSegments(pPrintLinearSegments),
          mVerboseHeaders(pVerboseHeaders), mNoLineBreaks(pNoLineBreaks),
          mPrintEntailed(pPrintEntailed), mNumThreads(pNumThreads),
          mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const uint64_t mC;
    const uint64_t mL;
    const bool mPrintRcs;
    const bool mOmitSequence;
    const bool mPrintLinearSegments;
    const bool mVerboseHeaders;
    const bool mNoLineBreaks;
    const bool mPrintEntailed;
    const uint64_t mNumThreads;
    const std::string mOut;
};


class GossCmdFactoryPrintContigs : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryPrintContigs();
};

#endif // GOSSCMDPRINTPATHS_HH
