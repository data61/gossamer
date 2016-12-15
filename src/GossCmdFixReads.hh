// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDFIXREADS_HH
#define GOSSCMDFIXREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdFixReads : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdFixReads(const std::string& pIn, const strings& pFastas, const strings& pFastqs, const strings& pLines,
                    const std::string& pOut, const uint64_t pNumThreads)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines),
          mOut(pOut), mNumThreads(pNumThreads)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const std::string mOut;
    const uint64_t mNumThreads;
};


class GossCmdFactoryFixReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryFixReads();
};

#endif // GOSSCMDFIXREADS_HH
