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

