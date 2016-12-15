// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "VariableByteArray.hh"

#include "Debug.hh"

#include <iostream>

using namespace Gossamer;
using namespace std;

static Debug showBitmapSizeAndCount("variable-byte-array-size-and-count",
                "when opening a variable-byte-array, show the size and count of the continuation bitmaps.");

void
VariableByteArray::Builder::end()
{
    mOrder0.end();
    mOrder1Present.end(bitmap_traits::init(mPosition0));
    mOrder1.end();
    mOrder2Present.end(bitmap_traits::init(mPosition1));
    mOrder2.end();
}


VariableByteArray::Builder::Builder(const std::string& pBaseName, FileFactory& pFactory,
                                    uint64_t pNumItems, double pFrac)
    : mPosition0(0), mPosition1(0),
      mOrder0(pBaseName + ".ord0", pFactory),
      mOrder1Present(pBaseName + ".ord1p", pFactory, bitmap_traits::init(pNumItems), pNumItems * 0.001),
      //mOrder1Present(pBaseName + ".ord1p", pFactory, bitmap_traits::init(pNumItems)),
      mOrder1(pBaseName + ".ord1", pFactory),
      mOrder2Present(pBaseName + ".ord2p", pFactory, bitmap_traits::init(pNumItems), pNumItems * 0.001),
      //mOrder2Present(pBaseName + ".ord2p", pFactory, bitmap_traits::init(pNumItems)),
      mOrder2(pBaseName + ".ord2", pFactory)
{
}


void
VariableByteArray::remove(const std::string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".ord0");
    pFactory.remove(pBaseName + ".ord1");
    pFactory.remove(pBaseName + ".ord2");
    SparseArray::remove(pBaseName + ".ord1p", pFactory);
    SparseArray::remove(pBaseName + ".ord2p", pFactory);
}

VariableByteArray::VariableByteArray(const std::string& pBaseName,
                                     FileFactory& pFactory)
    : mOrder0(pBaseName + ".ord0", pFactory),
      mOrder1Present(pBaseName + ".ord1p", pFactory),
      mOrder1(pBaseName + ".ord1", pFactory),
      mOrder2Present(pBaseName + ".ord2p", pFactory),
      mOrder2(pBaseName + ".ord2", pFactory)
{
    if (showBitmapSizeAndCount.on())
    {
        cerr << "Opening: " << pBaseName << endl;
        cerr << "\tord1p\t" << mOrder1Present.size() << '\t' << mOrder1Present.count() << endl;
        cerr << "\tord2p\t" << mOrder2Present.size() << '\t' << mOrder2Present.count() << endl;
    }
}
