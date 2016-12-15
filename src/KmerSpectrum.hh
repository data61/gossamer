// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef KMERSPECTRUM_HH
#define KMERSPECTRUM_HH

#include "GossReadHandler.hh"
#include "RankSelect.hh"
#include "KmerSet.hh"

class KmerSpectrum
{
public:
    typedef std::vector<std::string> strings;

    static GossReadHandlerPtr singleRowBuilder(uint64_t pK, const std::string& pVarName,
                                               const std::string& pFileName);

    static GossReadHandlerPtr multiRowBuilder(uint64_t pK, const std::string& pVarName,
                                              const std::string& pFileName);

    static GossReadHandlerPtr sparseSingleRowBuilder(const KmerSet& pKmerSet,
                                              const std::string& pKmersName,
                                              const std::string& pVarName,
                                              const std::string& pFileName);

    static GossReadHandlerPtr sparseMultiRowBuilder(const KmerSet& pKmerSet,
                                              const std::string& pKmersName,
                                              const strings& pInitKmerSets,
                                              bool pPerFile,
                                              FileFactory& pFactory);
};

#endif // KMERSPECTRUM_HH
