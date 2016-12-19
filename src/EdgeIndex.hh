// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef EDGEINDEX_HH
#define EDGEINDEX_HH

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

#ifndef SUPERGRAPH_HH
#include "SuperGraph.hh"
#endif

#ifndef BOOST_UNORDERED_MAP_HPP
#include <boost/unordered_map.hpp>
#define BOOST_UNORDERED_MAP_HPP
#endif

class EdgeIndex
{
public:
    static constexpr uint64_t version = 2011092301ULL;
    // Version history
    // 2011082901   - first readable/writable version
    // 2011092301   - changed PathIndex to a dense array

    struct Header
    {
        uint64_t version;
        uint64_t div;
    };

    typedef uint32_t PathId;
    typedef uint32_t SegmentRank;
    typedef uint32_t SegmentOffset;
    typedef uint32_t EdgeOffset;
    typedef std::pair<SegmentRank,EdgeOffset> SegmentAndOffset;
    typedef std::vector<SegmentAndOffset> SegmentIndex;
    typedef std::pair<PathId,SegmentOffset> PathIdAndOffset;
    typedef std::pair<SuperPathId,SegmentOffset> SuperPathIdAndOffset;
    typedef std::vector<PathIdAndOffset> PathIndex;

    /**
     * Iff pEdge is an edge in the graph, then bind pSegRank to the rank of
     * containing linear segment, bind offset to the distance from the start of the
     * linear segment, and return true. Return false otherwise.
     */
    bool segment(const Gossamer::position_type& pEdge, SegmentRank& pSegRank, EdgeOffset& pOffset) const
    {
        Graph::Edge e(pEdge);
        uint64_t r = 0;
        if (!mGraph.accessAndRank(e, r))
        {
            return false;
        }

        const uint64_t m = (1ULL << mDiv) - 1;
        if (r & m)
        {
            return false;
        }

        r = r >> mDiv;
        pSegRank = mSegmentIndex[r].first;
        pOffset = mSegmentIndex[r].second;
        return true;
    }

    bool segment(const Gossamer::rank_type& pRank, SegmentRank& pSegRank, EdgeOffset& pOffset) const
    {
        const uint64_t m = (1ULL << mDiv) - 1;
        if (pRank & m)
        {
            return false;
        }

        uint64_t r = pRank >> mDiv;
        pSegRank = mSegmentIndex[r].first;
        pOffset = mSegmentIndex[r].second;
        return true;
    }


    /**
     * Iff pSegRank identifies a linear segment which is part of a unique
     * super-path, then return true, otherwise return false.
     * If true, then pId is bound to the super-path id, and pOffset is bound
     * to the distance from the start of the given segment.
     */
    bool superpath(const uint64_t& pSegRank, SuperPathIdAndOffset& pInfo) const
    {
        BOOST_ASSERT(pSegRank < mMulti.size());
        if (mMulti[pSegRank])
        {
            return false;
        }

        const PathIdAndOffset& x(mPathIndex[pSegRank]);
        pInfo = std::make_pair(SuperPathId(x.first), x.second);
        return true;
    }

    /**
     * Saves the EdgeIndex.
     */
    void write(const std::string& pBaseName, FileFactory& pFactory) const;

    static std::unique_ptr<EdgeIndex> read(const std::string& pBaseName, FileFactory& pFactory,
                                         const Graph& pGraph);

    static std::unique_ptr<EdgeIndex> create(const Graph& pGraph, const EntryEdgeSet& pEntryEdges,
                                           const SuperGraph& pSuper, uint64_t pDiv, 
                                           uint64_t pNumThreads, Logger& pLog);

private:

    EdgeIndex(const Graph& pGraph, uint64_t pDiv);

    const uint64_t mDiv;
    const Graph& mGraph;
    SegmentIndex mSegmentIndex;
    PathIndex mPathIndex;
    boost::dynamic_bitset<> mMulti;
};

#endif // EDGEINDEX_HH
