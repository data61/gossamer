// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ASYNCMERGE_HH
#define ASYNCMERGE_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class AsyncMerge
{
public:
    template <typename Kind>
    static void merge(const std::vector<std::string>& pParts, const std::vector<uint64_t>& pSizes,
                      const std::string& pGraphName,
                      uint64_t pK, uint64_t pN, uint64_t pNumThreads, uint64_t pBufferSize,
                      FileFactory& pFactory);
};

#include "AsyncMerge.tcc"

#endif // ASYNCMERGE_HH
