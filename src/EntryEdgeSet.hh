// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ENTRYEDGESET_HH
#define ENTRYEDGESET_HH

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

#ifndef INTEGERARRAY_HH
#include "IntegerArray.hh"
#endif

class EntryEdgeSet : public GraphEssentials<EntryEdgeSet>
{
public:
    static const uint64_t version = 2011041901ULL; 
    // Version history
    // 2011020801   - initial version
    // 2011041901   - add lengths.

    struct Header
    {
        uint64_t version;
        uint64_t K;

        Header()
        {
        }

        Header(const std::string& pFileName, FileFactory& pFactory);
    };

    // The maximum number of bits we allow for start edge ranks.
    static const uint64_t RankBits = 40;

    static void build(const Graph& pGraph, const std::string& pBaseName, 
                      FileFactory& pFactory, Logger& pLog, uint64_t pThreads=1);

    // The length of the k-mers used for building the graph.
    //
    uint64_t K() const
    {
        return mHeader.K;
    }

    // The number of edges in the graph.
    //
    Gossamer::rank_type count() const
    {
        return mEdges.count();
    }

    // Return the number of edges that occur before the
    // given edge in the sorted list of edges.
    //
    Gossamer::rank_type rank(const Edge& pEdge) const
    {
        return mEdges.rank(pEdge.value());
    }

    // Return the ranks of the two given edges.
    //
    std::pair<uint64_t,uint64_t> rank(const Edge& pLhs, const Edge& pRhs) const
    {
        return mEdges.rank(pLhs.value(), pRhs.value());
    }

    // Return the 'pRank'th edge from the sorted list
    // of edges.
    //
    Edge select(const Gossamer::rank_type& pRank) const
    {
        return Edge(mEdges.select(pRank));
    }

    // Does the given edge exist?
    //
    bool access(const Edge& pEdge) const
    {
        std::pair<uint64_t,uint64_t> r = rank(pEdge, Edge(pEdge.value() + 1));
        return r.second - r.first;
    }

    // Does the given edge exist?
    //
    bool accessAndRank(const Edge& pEdge, Gossamer::rank_type& pRank) const
    {
        return mEdges.accessAndRank(pEdge.value(), pRank);
    }

    // Return the count of the linear segment.
    // (The rounded mean of the counts of underlying edges.)
    uint32_t multiplicity(const Edge& pEdge) const
    {
        return multiplicity(rank(pEdge));
    }

    // Return the count of the linear segment.
    // (The rounded mean of the counts of underlying edges.)
    uint32_t multiplicity(const Gossamer::rank_type& pEdgeRank) const
    {
        return mCounts[pEdgeRank];
    }

    // Return the number of edges in the linear segment.
    uint64_t length(const Gossamer::rank_type& pEdgeRank) const
    {
        return mLengths[pEdgeRank];
    }

    // Returns the rank of the start edge of the reverse complement segment!
    // (NOT the rank of the last edge in this segment; rather, its reverse complement.)
    Gossamer::rank_type endRank(const Gossamer::rank_type& pEdgeRank) const
    {
        return mEnds[pEdgeRank].asUInt64();
    }

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    EntryEdgeSet(const std::string& pBaseName, FileFactory& pFactory);

private:
    Header mHeader;
    SparseArray mEdges;
    VariableByteArray mCounts;
    VariableByteArray mLengths;
    IntegerArrayPtr mEndsHolder;
    IntegerArray& mEnds;
};

// std::hash
namespace std {
    template<>
    struct hash<EntryEdgeSet::Node>
    {
        std::size_t operator()(const EntryEdgeSet::Node& pValue) const
        {
            return std::hash<Gossamer::position_type>()(pValue.value());
        }
    };
}

#endif // ENTRYEDGESET_HH
