// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

template<typename Lhs, typename Rhs, typename Dest>
void
ExternalSortBase::merge(Lhs& pLhs, Rhs& pRhs, Dest& pDest)
{
    while (pLhs.valid() && pRhs.valid())
    {
        if (*pLhs < *pRhs)
        {
            pDest.push_back(*pLhs);
            ++pLhs;
            continue;
        }
        else
        {
            pDest.push_back(*pRhs);
            ++pRhs;
            continue;
        }
    }
    copy(pLhs, pDest);
    copy(pRhs, pDest);
}


