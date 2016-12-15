// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SUPERPATHID_HH
#define SUPERPATHID_HH

#ifndef TAGGGEDNUM_HH
#include "TaggedNum.hh"
#endif

struct SuperPathIdTag {};
typedef TaggedNum<SuperPathIdTag> SuperPathId;

namespace std
{
    template <>
    struct hash<SuperPathId>
    {
        uint64_t operator()(const SuperPathId& pId) const
        {
            return std::hash<uint64_t>()(pId.value());
        }
    };
}

#endif // SUPERPATHID_HH
