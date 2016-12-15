// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GRAPH_HH
#define GRAPH_HH

#ifndef GRAPHESSENTIALS_HH
#include "GraphEssentials.hh"
#endif 

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef SPARSEARRAY_HH
#include "SparseArray.hh"
#endif

#ifndef SPARSEARRAYVIEW_HH
#include "SparseArrayView.hh"
#endif

#ifndef VARIABLEBYTEARRAY_HH
#include "VariableByteArray.hh"
#endif

#ifndef BACKGROUNDCONSUMER_HH
#include "BackgroundConsumer.hh"
#endif

#ifndef BACKGROUNDBLOCKCONSUMER_HH
#include "BackgroundBlockConsumer.hh"
#endif

#ifndef PROPERTIES_HH
#include "Properties.hh"
#endif

#ifndef BOOST_DYNAMIC_BITSET_HPP
#include <boost/dynamic_bitset.hpp>
#define BOOST_DYNAMIC_BITSET_HPP
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_BITSET
#include <bitset>
#define STD_BITSET
#endif

class Graph;
typedef boost::shared_ptr<Graph> GraphPtr;

class Graph : public GraphEssentials<Graph>
{
public:
    static const uint64_t version = 2011101014ULL; 
    // Version history
    // 2011101014   - allow asymmetric graphs
    // 2011091601   - use SparseArrayView
    // 2011071101   - use SparseArray for VariableByteArray
    // 2010072301   - use VariableByteArray for counts
    // 2010062301   - introduce version tracking

    struct Header
    {
        uint64_t version;
        uint64_t K;

        enum {
            fAsymmetric = 0,
            fLastFlag = 64
        };
        std::bitset<fLastFlag> flags;
    };

    class Iterator;

    // Largest k-mer size that will work for 128-bit words.
    // Note: The otherwise-largest value is used as a sentinel, hence the -2.
    static const uint64_t MaxK = sizeof(Gossamer::position_type) * 4 - 2;
    static uint64_t maxK()
    {
        return MaxK;
    }

    class Builder
    {
    public:
        struct DTag {};
        typedef TaggedNum<DTag> D;

        void push_back(const Gossamer::position_type& pEdge, uint64_t pCount)
        {
            mEdgesBuilderBackground.push_back(pEdge);
            mCountsBuilderBackground.push_back(pCount);
            ++mHist[pCount];
        }

        void end();

        /**
         * Retrieve information about the buidler.
         */
        PropertyTree stat() const;

        Builder(uint64_t pK, const std::string& pBaseName, FileFactory& pFactory, Gossamer::rank_type pNumEdges, bool pAsymmetric = false);
        Builder(uint64_t pK, const std::string& pBaseName, FileFactory& pFactory, D pD, bool pAsymmetric = false);

    private:
        const std::string mBaseName;
        FileFactory& mFactory;
        uint64_t mK;
        SparseArray::Builder mEdgesBuilder;
        BackgroundBlockConsumer<SparseArray::Builder> mEdgesBuilderBackground;
        VariableByteArray::Builder mCountsBuilder;
        BackgroundBlockConsumer<VariableByteArray::Builder> mCountsBuilderBackground;
        std::map<uint64_t,uint64_t> mHist;
    };

    class MarkSeen
    {
    public:
        void operator()(const Edge& pEdge, const Gossamer::rank_type& pRank) const
        {
            mSeen[pRank] = 1;
        }

        MarkSeen(const Graph& pGraph, boost::dynamic_bitset<>& pSeen)
            : mSeen(pSeen)
        {
        }

    private:
        boost::dynamic_bitset<>& mSeen;
    };

    class PathCounter
    {
    public:
        bool operator()(const Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            ++mValue;
            return true;
        }

        uint64_t value() const
        {
            return mValue;
        }

        PathCounter()
            : mValue(0)
        {
        }

    private:
        uint64_t mValue;
    };

    class Iterator
    {
    public:
        bool valid() const
        {
            return mRnk < mEdgesView->count();
        }

        const std::pair<Edge,uint32_t>& operator*() const
        {
            return mCurr;
        }

        void operator++()
        {
            BOOST_ASSERT(valid());
            ++mRnk;
            if (valid())
            {
                mCurr.first = Edge(mEdgesView->select(mRnk));
                mCurr.second = (*mCounts)[mEdgesView->originalRank(mRnk)];
            }
        }

        Iterator(const Graph& pGraph)
            : mEdgesView(&pGraph.edges()),
              mCounts(&pGraph.counts()),
              mRnk(0),
              mCurr(Edge(Gossamer::position_type(0)), 0)
        {
            if (valid())
            {
                mCurr.first = Edge(mEdgesView->select(mRnk));
                mCurr.second = (*mCounts)[mEdgesView->originalRank(mRnk)];
            }
        }
    private:

        const SparseArrayView* mEdgesView;
        const VariableByteArray* mCounts;
        Gossamer::rank_type mRnk;
        std::pair<Edge,uint32_t> mCurr;
    };

    /// Iterate over all nodes with outgoing edges.  Nodes with only
    /// incoming edges are not returned, though their reverse complements are.
    class NodeIterator
    {
    public:
        bool valid() const
        {
            return mValid;
        }

        Node operator*() const
        {
            return mNode;
        }

        void operator++()
        {
            BOOST_ASSERT(valid());
            advance();
        }

        NodeIterator(const Graph& pGraph)
            : mGraph(pGraph), mItr(pGraph), mNode(Gossamer::position_type(0)), mValid(true)
        {
            mValid = mItr.valid();
            if (mValid)
            {
                mNode = mGraph.from((*mItr).first);
            }
        }

    private:
        void advance()
        {
            // The following initialisation means nothing;
            // this will get clobbered before it's read.
            Node node(Gossamer::position_type(0));
            do
            {
                mValid = mItr.valid();
                if (!mValid)
                {
                    return;
                }
                Edge e = (*mItr).first;
                node = mGraph.from(e);
                ++mItr;
            } while (node == mNode);
            mNode = node;
        }

        const Graph& mGraph;
        Iterator mItr;
        Node mNode;
        bool mValid;
    };

    /**
     *
     */
    class LazyIterator
    {
    public:

        /**
         * Return true iff there is at least one remaining edge/count pair.
         */
        bool valid() const
        {
            return mEdgesItr.valid();
        }

        /**
         * Get the current edge/count pair.
         */
        std::pair<Edge,uint32_t> operator*() const
        {
            return std::pair<Edge,uint32_t>(Edge(*mEdgesItr), *mCountsItr);
        }

        /**
         * Advance the iterator the the next edge/count pair it it exists.
         */
        void operator++()
        {
            BOOST_ASSERT(mEdgesItr.valid());
            BOOST_ASSERT(mCountsItr.valid());
            ++mEdgesItr;
            ++mCountsItr;
        }

        /**
         * Return true iff the graph is asymmetric.
         */
        bool asymmetric() const
        {
            return mHeader.flags[Header::fAsymmetric];
        }

        /**
         * Return the k-mer size for the graph being iterated over.
         */
        uint64_t K() const
        {
            return mHeader.K;
        }

        /**
         * Return the edge count for the graph being iterated over.
         */
        Gossamer::rank_type count() const
        {
            return mCount;
        }

        /**
         * Simple kmer comparison.
         */
        bool operator<(const LazyIterator& pRhs) const
        {
            return (**this).first < (*pRhs).first;
        }

        LazyIterator(const std::string& pBaseName, FileFactory& pFactory);

    private:

        Header mHeader;
        Gossamer::rank_type mCount;
        SparseArray::LazyIterator mEdgesItr;
        VariableByteArray::LazyIterator mCountsItr;
    };


    Iterator iterator() const
    {
        return Iterator(*this);
    }

    static LazyIterator lazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    {
        return LazyIterator(pBaseName, pFactory);
    }

    // The length of the k-mers used for building the graph.
    //
    uint64_t K() const
    {
        return mHeader.K;
    }

    // Is the graph asymmetric?
    //
    bool asymmetric() const
    {
        return mHeader.flags[Header::fAsymmetric];
    }

    // The number of edges in the graph.
    //
    Gossamer::rank_type count() const
    {
        return mEdgesView.count();
    }

    // Return the number of edges that occur before the
    // given edge in the sorted list of edges.
    //
    Gossamer::rank_type rank(const Edge& pEdge) const
    {
        return mEdgesView.rank(pEdge.value());
    }

    // Return the ranks of the two given edges.
    //
    std::pair<Gossamer::rank_type,Gossamer::rank_type>
    rank(const Edge& pLhs, const Edge& pRhs) const
    {
        return mEdgesView.rank(pLhs.value(), pRhs.value());
    }

    // Return the 'pRank'th edge from the sorted list
    // of edges.
    //
    Edge select(Gossamer::rank_type pRank) const
    {
        return Edge(mEdgesView.select(pRank));
    }

    // Does the given edge exist?
    //
    bool access(const Edge& pEdge) const
    {
        return mEdgesView.access(pEdge.value());
    }

    // Does the given edge exist?
    //
    bool accessAndRank(const Edge& pEdge, Gossamer::rank_type& pRank) const
    {
        return mEdgesView.accessAndRank(pEdge.value(), pRank);
    }

    // What is the count associated with this edge.
    //
    uint32_t multiplicity(const Edge& pEdge) const
    {
        return multiplicity(rank(pEdge));
    }

    // What is the count associated with the indicated edge.
    //
    uint32_t multiplicity(Gossamer::rank_type pEdgeRank) const
    {
        return mCounts[mEdgesView.originalRank(pEdgeRank)];
    }

    // Does this edge have a forward sense?
    //
    bool forwardSense(const Edge& pEdge) const
    {
        return asymmetric() ? forwardSense(rank(pEdge)) : true;
    }

    // Does the indicated edge have a forward sense?
    //
    bool forwardSense(Gossamer::rank_type pEdgeRank) const
    {
        return asymmetric() ? (multiplicity(pEdgeRank) > 0) : true;
    }

    // Assuming this node has only one out-going edge,
    // return that edge.
    //
    Edge onlyOutEdge(const Node& pNode) const
    {
        BOOST_ASSERT(outDegree(pNode) == 1);
        return select(rank(Edge(pNode.value() << 2)));
    }

    // Assuming this node has only one out-going edge,
    // return that edge.
    //
    Edge onlyOutEdge(const Node& pNode, Gossamer::rank_type& pRank) const
    {
        BOOST_ASSERT(outDegree(pNode) == 1);
        pRank = rank(Edge(pNode.value() << 2));
        return select(pRank);
    }

    // Assuming this node has only one in-coming edge,
    // return that edge.
    //
    Edge onlyInEdge(const Node& pNode) const
    {
        return reverseComplement(onlyOutEdge(reverseComplement(pNode)));
    }
    // Return true if there is no edge proceeding
    // from the same node as this edge proceeds from
    // that has a greater multiplicity.
    //
    bool majorityEdge(const Edge& pEdge) const;

    // Find the lowest numbered edge such that no
    // other edge proceeding from the same node has
    // greater multiplicity.
    //
    Edge majorityEdge(const Node& pNode) const;

    // Return the multiplicity of the given edge.
    //
    uint64_t weight(uint64_t pEdgeRank) const
    {
        return mCounts[pEdgeRank];
    }

    // Return the multiplicity of the given edge.
    //
    uint64_t weight(const Edge& pEdge) const
    {
        return mCounts[rank(pEdge)];
    }

    // Compute the sum of the multiplicities for all the edges
    // in the given linear segment.
    //
    uint64_t weight(const Edge& pBegin, const Edge& pEnd) const;

    // For the given edge, find the terminating edge
    // in the sequence of edges starting with the given
    // edge such that neither the forwards nor reverse
    // complement paths contain any branches.
    //
    template <typename Visitor>
    Edge linearPath(const Edge& pBegin, Visitor& pVis) const;

    class NullVisitor
    {
    public:
        bool operator()(const Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            return true;
        }
    };

    // For the given edge, find the terminating edge
    // in the sequence of edges starting with the given
    // edge such that neither the forwards nor reverse
    // complement paths contain any branches.
    //
    Edge linearPath(const Edge& pBegin) const
    {
        NullVisitor null;
        return linearPath(pBegin, null);
    }

    uint64_t pathLength(const Edge& pBegin, const Edge& pEnd) const
    {
        PathCounter acc;
        visitPath(pBegin, pEnd, acc);
        return acc.value();
    }

    template <typename Visitor>
    void visitPath(const Edge& pBegin, const Edge& pEnd, Visitor& pVis) const;

    void tracePath(const Edge& pBegin, const Edge& pEnd, SmallBaseVector& pSeq) const
    {
        seq(from(pBegin), pSeq);
        tracePath1(pBegin, pEnd, pSeq);
    }

    void tracePath1(const Edge& pBegin, const Edge& pEnd, SmallBaseVector& pSeq) const;

    void tracePath(const Edge& pBegin, const Edge& pEnd, std::vector<Edge>& pEdges) const;

    void tracePath(const Edge& pBegin, const Edge& pEnd, std::vector<Node>& pNodes) const;

    template <typename Visitor>
    void visitMajorityPath(const Node& pBegin, uint64_t pLength, Visitor& pVis) const;

    void traceMajorityPath(const Node& pBegin, uint64_t pLength, SmallBaseVector& pSeq) const;

    template <typename Visitor>
    void inTerminals(Visitor& pVisitor) const;

    template <typename Visitor>
    void linearSegments(Visitor& pVisitor) const;

    template <typename Visitor>
    void dfs(Visitor& pVis) const;

    template <typename Visitor>
    void dfs(const Node& pBegin, Visitor& pVis) const
    {
        boost::dynamic_bitset<> seen(mEdgesView.count());
        dfs(pBegin, seen, pVis);
    }

    template <typename Visitor>
    void dfs(const Node& pBegin, boost::dynamic_bitset<>& pSeen, Visitor& pVis) const;

    template <typename Visitor>
    void bfs(const Node& pBegin, boost::dynamic_bitset<>& pSeen, Visitor& pVis) const;

    const SparseArrayView& edges() const
    {
        return mEdgesView;
    }

    const VariableByteArray& counts() const
    {
        return mCounts;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("edges", mEdges.stat());
        t.putSub("counts", mCounts.stat());

        t.putProp("count", count());
        t.putProp("K", K());

        uint64_t s = sizeof(Header);
        s += t("edges").as<uint64_t>("storage");
        s += t("counts").as<uint64_t>("storage");
        t.putProp("storage", s);

        return t;
    }

    void remove(const boost::dynamic_bitset<>& pBitmap);

    /**
     * Return a histogram giving the frequency of different edge counts.
     */
    static std::map<uint64_t,uint64_t> hist(const std::string& pBaseName, FileFactory& pFactory);

    static GraphPtr open(const std::string& pBaseName, FileFactory& pFactory);

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

private:

    Graph(const std::string& pBaseName, FileFactory& pFactory);

    Header mHeader;
    SparseArray mEdges;
    SparseArrayView mEdgesView;
    VariableByteArray mCounts;
};


namespace std
{
    template <>
    struct hash<Graph::Edge>
    {
        uint64_t operator()(const Graph::Edge& pEdge) const
        {
            return std::hash<Gossamer::position_type>()(pEdge.value());
        }
    };

    template <>
    struct hash<Graph::Node>
    {
        uint64_t operator()(const Graph::Node& pNode) const
        {
            return std::hash<Gossamer::position_type>()(pNode.value());
        }
    };
}


#include "Graph.tcc"

#endif // GRAPH_HH
