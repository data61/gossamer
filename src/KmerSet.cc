// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "FileFactory.hh"
#include "KmerSet.hh"

#include <string>

using namespace std;

void
KmerSet::remove(const string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    SparseArray::remove(pBaseName + ".kmers", pFactory);
}
