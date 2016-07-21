#ifndef GRAPHTRIMMER_HH
#define GRAPHTRIMMER_HH

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

#ifndef BOOST_DYNAMIC_BITSET_HPP
#include <boost/dynamic_bitset.hpp>
#define BOOST_DYNAMIC_BITSET_HPP
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

class GraphTrimmer
{
public:
    class EdgeTrimVisitor
    {
    private:
        GraphTrimmer& mTrimmer;

    public:
        EdgeTrimVisitor(GraphTrimmer& pTrimmer)
            : mTrimmer(pTrimmer)
        {
        }

        void operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            mTrimmer.deleteEdge(pEdge);
        }
    };

    bool modified() const
    {
        return mModified;
    }

    bool edgeDeleted(Graph::Edge pEdge) const
    {
        return edgeDeleted(mGraph.rank(pEdge));
    }

    bool edgeDeleted(uint64_t pEdgeRank) const
    {
        return mDeletedEdges[pEdgeRank];
    }

    void deleteEdge(uint64_t pRank, uint64_t pRankRC)
    {
        mModified = true;
        mDeletedEdges[pRank] = true;
        mDeletedEdges[pRankRC] = true;
    }

    void deleteEdge(Graph::Edge pEdge)
    {
        deleteEdge(mGraph.rank(pEdge),
                   mGraph.rank(mGraph.reverseComplement(pEdge)));
    }

    void changeCount(Graph::Edge pEdge, uint32_t pNewCount)
    {
        changeCount(mGraph.rank(pEdge),
                    mGraph.rank(mGraph.reverseComplement(pEdge)),
                    pNewCount);
    }

    void changeCount(uint64_t pEdgeRank, uint64_t pEdgeRankRC, uint32_t pNewCount)
    {
        mModified = true;
        mCounts[pEdgeRank] = pNewCount;
        mCounts[pEdgeRankRC] = pNewCount;
    }

    uint64_t removedEdgesCount() const
    {
        return mDeletedEdges.count();
    }

    void writeTrimmedGraph(Graph::Builder& pBuilder) const;

    GraphTrimmer(const Graph& pGraph)
        : mGraph(pGraph), mDeletedEdges(pGraph.count()), mModified(false)
    {
    }

private:
    typedef std::map<uint64_t,uint32_t> count_map_t;
    const Graph& mGraph;
    boost::dynamic_bitset<> mDeletedEdges;
    count_map_t mCounts;
    bool mModified;
};


#endif // GRAPHTRIMMER_HH
