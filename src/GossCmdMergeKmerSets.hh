// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDMERGEKMERSETS_HH
#define GOSSCMDMERGEKMERSETS_HH

#ifndef GOSSCMDMERGE_HH
#include "GossCmdMerge.hh"
#endif

#ifndef KMERSET_HH
#include "KmerSet.hh"
#endif

typedef GossCmdMerge<KmerSet> GossCmdMergeKmerSets;
typedef GossCmdFactoryMerge<KmerSet> GossCmdFactoryMergeKmerSets;

#if 0
class GossCmdMergeKmerSets : public GossCmdMerge<KmerSet>
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt)
    {
        static_cast<GossCmdMerge<KmerSet> >(*this)(pCxt);
    }

    GossCmdMergeKmerSets(const strings& pIns, const uint64_t& pV, const uint64_t& pMaxMerge, const std::string& pOut)
        : GossCmdMerge<KmerSet>(pIns, pV, pMaxMerge, pOut)
    {
    }

};

class GossCmdFactoryMergeKmerSets : public GossCmdFactoryMerge<KmerSet>
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
    {
        return static_cast<GossCmdFactoryMerge<KmerSet> >(*this).create(pApp, pOpts);
    }

    GossCmdFactoryMergeKmerSets()
        : GossCmdFactoryMerge<KmerSet>()
    {
    }
};
#endif

#endif // GOSSCMDMERGEKMERSETS_HH
