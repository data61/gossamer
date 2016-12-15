// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GraphTrimmer.hh"

void
GraphTrimmer::writeTrimmedGraph(Graph::Builder& pBuilder) const
{
    count_map_t::const_iterator ii = mCounts.begin();
    count_map_t::const_iterator ee = mCounts.end();

    for (uint64_t i = 0; i < mGraph.count(); ++i)
    {
        if (mDeletedEdges[i])
        {
            continue;
        }
        Graph::Edge e = mGraph.select(i);
        uint32_t c;
        if (ii != ee && ii->first == i)
        {
            c = ii->second;
            ++ii;
        }
        else
        {
            c = mGraph.multiplicity(e);
        }
        pBuilder.push_back(e.value(), c);
    }

    pBuilder.end();
}

