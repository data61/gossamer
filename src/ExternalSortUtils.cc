// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "ExternalSortUtils.hh"
#include <boost/lexical_cast.hpp>

void
ExternalSortBase::genFileName(const std::string& pBase,
        uint64_t pNum, std::string& pName)
{
    pName = pBase;
    pName += boost::lexical_cast<std::string>(pNum);
    pName += ".sort-run";
}

void
ExternalSortBase::genFileName(
        const std::string& pDir,
        const std::string& pBase,
        uint64_t pNum, std::string& pName)
{
    pName = pDir;
    pName += "/";
    pName += pBase;
    pName += boost::lexical_cast<std::string>(pNum);
    pName += ".sort-run";
}


