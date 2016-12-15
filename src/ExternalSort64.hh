// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef EXTERNALSORT64_HH
#define EXTERNALSORT64_HH

#ifndef EXTERNALSORTUTILS_HH
#include "ExternalSortUtils.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif // LOGGER_HH

template <typename T, typename V = ExternalSortCodec<T> >
class ExternalSort : public ExternalSortBase
{
public:
    template <typename SrcItr, typename DestItr>
    static void sort(SrcItr& pSrc, DestItr& pDest,
                     FileFactory& pFactory, uint64_t pBufSpace,
                     Logger* pLogger);
};


#include "ExternalSort64.tcc"

#endif // EXTERNALSORT64_HH
