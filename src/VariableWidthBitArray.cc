// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "VariableWidthBitArray.hh"

VariableWidthBitArray::Builder::Builder(const std::string& pBaseName, FileFactory& pFactory)
    : mFile(pBaseName, pFactory),
      mPos(0), mCurrWord(0)
{
}

void
VariableWidthBitArray::Builder::flush()
{
    mFile.push_back(mCurrWord);
}

void
VariableWidthBitArray::Builder::end()
{
    flush();
    mFile.end();
}

VariableWidthBitArray::VariableWidthBitArray(const std::string& pBaseName, FileFactory& pFactory)
    : mFile(pBaseName, pFactory)
{
}


