// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "ResolveTranscripts.hh"

#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <random>
#include <boost/call_traits.hpp>
#include <boost/ptr_container/ptr_unordered_set.hpp>
#include <boost/ptr_container/ptr_deque.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/dynamic_bitset.hpp>
#include <unordered_map>
#include <unordered_set>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <set>
#include <iterator>
#include "FibHeap.hh"
#include "SimpleHashSet.hh"
#include "SimpleHashMap.hh"
#include "ArrayMap.hh"

// Various debug options.
#undef DEBUG
#undef DEBUG_EXTRACTION
#undef VERBOSE_DEBUG_EXTRACTION
#undef INTERNAL_PROGRESS
#undef MORE_INTERNAL_PROGRESS
#undef FULL_SANITY_CHECK
#undef DUMP_INTERMEDIATES
#undef RETAIN_UNMAPPED_READS
#undef DEBUG_SCC
#undef DONT_SPECIAL_CASE_PATH_TRACING
#undef DUMP_GRAPH_ON_POTENTIAL_INFINITE_LOOP

using namespace std;
using namespace boost;
using Gossamer::rank_type;
using Gossamer::position_type;

namespace
{
    static string sComponentKmersName("kmers");
    static string sComponentNodesName("nodes");

    struct edge_tag_t {};
    typedef TaggedNum<edge_tag_t> edge_t;

    struct node_tag_t {};
    typedef TaggedNum<node_tag_t> node_t;

    const uint64_t sMaxPathsPerNode = 200;
    const uint64_t sMinReadSupportThresh = 2; // Must be at least 1.
    const double sMinReadSupportRel = 0.02;

    const uint64_t sMaxEdgesPerNode = 4;

    struct ReadSuppEntry {
        uint64_t mRead;
        uint32_t mPos;
        uint32_t mPath;

        ReadSuppEntry(uint64_t pRead, uint32_t pPos, uint32_t pPath)
            : mRead(pRead), mPos(pPos), mPath(pPath)
        {
        }
    };

    typedef vector< ReadSuppEntry > read_supp_t;

    struct PathSuppEntry {
        uint32_t mSupport;
        uint32_t mLength;
        uint64_t mPath;

        PathSuppEntry()
            : mSupport(0), mLength(0), mPath(0)
        {
        }

        PathSuppEntry(uint32_t pSupport, uint32_t pLength, uint64_t pPath)
            : mSupport(pSupport), mLength(pLength), mPath(pPath)
        {
        }

        // Use reverse ordering so that better paths get sorted first.
        //
        // A path is better than another if it has higher read support or,
        // in the case of a tie, is longer.
        //
        bool operator<(const PathSuppEntry& pRhs) const
        {
            if (mSupport > pRhs.mSupport)
            {
                return true;
            }
            else if (mSupport < pRhs.mSupport)
            {
                return false;
            }
            return mLength > pRhs.mLength;
        }
    };


    template<typename C>
    struct PushBackVisitor
    {
        typedef typename call_traits<C>::reference container_reference;
        typedef typename C::const_reference const_reference;

        container_reference mContainer;

        void operator()(const_reference pItem)
        {
            mContainer.push_back(pItem);
        }

        PushBackVisitor(container_reference pContainer)
            : mContainer(pContainer)
        {
        }
    };

    struct MappedRead
    {
        vector<rank_type> mEdges;
        vector<bool> mEdgeMaps;

        MappedRead(const vector<rank_type>& pEdges, const vector<bool>& pEdgeMaps)
            : mEdges(pEdges), mEdgeMaps(pEdgeMaps)
        {
        }

        uint64_t size() const
        {
            return mEdges.size();
        }
    };

    struct VerifiedRead
    {
        vector<rank_type> mEdges;
        uint32_t mCount;

        template<typename It>
        VerifiedRead(It pBegin, It pEnd)
            : mEdges(pBegin, pEnd), mCount(1)
        {
        }

        uint64_t size() const
        {
            return mEdges.size();
        }

        uint64_t count() const
        {
            return mCount;
        }

        bool operator<(const VerifiedRead& pRhs) const
        {
            return mEdges < pRhs.mEdges;
        }

        bool operator==(const VerifiedRead& pRhs) const
        {
            return mEdges == pRhs.mEdges;
        }

        void swap(VerifiedRead& pRhs)
        {
            std::swap(mEdges, pRhs.mEdges);
            std::swap(mCount, pRhs.mCount);
        }
    };

    struct PathBundle
    {
        vector< vector<rank_type> > mPaths;
        read_supp_t mReadSupport;
        uint64_t mSingletonPath;

        rank_type singletonEdge() const
        {
            return mPaths[mSingletonPath].front();
        }

        bool hasSingletonPath() const
        {
            return mSingletonPath != ~(uint64_t)0;
        }

        bool isSingletonPath(uint64_t pPathNo) const
        {
            return mSingletonPath == pPathNo;
        }

        uint64_t ensureSingletonPath(rank_type pEdge)
        {
            if (!hasSingletonPath())
            {
                vector<rank_type> path;
                path.reserve(1);
                path.push_back(pEdge);
                mSingletonPath = mPaths.size();
                mPaths.push_back(path);
            }
            return mSingletonPath;
        }

        PathBundle()
        {
            mSingletonPath = ~(uint64_t)0;
        }

        void swap(PathBundle& pRhs)
        {
            std::swap(mPaths, pRhs.mPaths);
            std::swap(mReadSupport, pRhs.mReadSupport);
            std::swap(mSingletonPath, pRhs.mSingletonPath);
        }
    };

    struct PathsReachingNode
    {
        vector<PathBundle> mBundles;

        PathsReachingNode()
        {
            mBundles.reserve(sMaxEdgesPerNode);
        }

        void swap(PathsReachingNode& pRhs)
        {
            std::swap(mBundles, pRhs.mBundles);
        }
    };
}


namespace std {
    template<>
    struct hash<edge_t>
    {
        uint64_t operator()(const edge_t& pId) const
        {
            return hash<uint64_t>()(pId.value());
        }
    };

    template<>
    struct hash<node_t>
    {
        uint64_t operator()(const node_t& pId) const
        {
            return hash<uint64_t>()(pId.value());
        }
    };
}



struct ResolveTranscripts::Impl
{
    struct Build
    {
        dynamic_bitset<uint64_t> mContigEdges;
        dynamic_bitset<uint64_t> mGraphEdges;
        SimpleHashMap<uint64_t,uint64_t> mReadCoverage;

        Build(uint64_t pEdgesInGraph)
            : mContigEdges(pEdgesInGraph),
              mGraphEdges(pEdgesInGraph)
        {
        }
    };

    typedef DenseArray subset_t;

    // OutputIterator which builds a DenseArray from a dynamic_bitset,
    // word-at-a-time.  This is more efficient than bit-at-a-time,
    // since we can skip over words which are zero.
    struct BitsetToBuilder
    {
        typedef BitsetToBuilder self_t;

        typedef std::output_iterator_tag iterator_category;
        typedef uint64_t value_type;
        typedef uint64_t& reference;
        typedef uint64_t* pointer;
        typedef const uint64_t& const_reference;
        typedef void difference_type;

        static const uint64_t sBitsPerWord
            = std::numeric_limits<uint64_t>::digits;

        DenseArray::Builder& mBuilder;
        uint64_t mValue;
        uint64_t mPosition;

        BitsetToBuilder(DenseArray::Builder& pBuilder)
            : mBuilder(pBuilder), mValue(0), mPosition(0)
        {
        }

        reference operator*()
        {
            return mValue;
        }

        void flush()
        {
            if (mValue)
            {
                for (uint64_t i = 0; i < sBitsPerWord; ++i)
                {
                    if (mValue & 1)
                    {
                        mBuilder.push_back(mPosition + i);
                    }
                    mValue >>= 1;
                }
            }
            mValue = 0;
            mPosition += sBitsPerWord;
        }

        self_t& operator++() { flush(); return *this; }

        self_t& operator++(int) { flush(); return *this; }

        ~BitsetToBuilder()
        {
            if (mValue)
            {
                flush();
            }
        }
    };


    static void denseArrayFromBitmap(const std::string& pName,
            FileFactory& pFac, const dynamic_bitset<uint64_t>& pBitmap)
    {
        DenseArray::Builder b(pName, pFac);
        {
            BitsetToBuilder bs2b(b);
            to_block_range(pBitmap, bs2b);
        }
        b.end(pBitmap.size());
    }


    struct Component
    {
    private:
        // Ideally, this would be part of Component, but unfortunately,
        // our rank/select data structures can only be initialised by
        // construction at the moment.  Fixing this is future work.
        struct Nodes;
        friend struct Nodes;

        Graph& mGraph;
        FileFactory& mFac;
        subset_t mKmers;
        dynamic_bitset<uint64_t> mToRemove;
        vector<uint32_t> mCoverage;
        std::shared_ptr<Nodes> mNodes;

        struct Nodes
        {
            Graph& mGraph;
            subset_t& mKmers;
            uint64_t mNodeCount;
            uint64_t mOrdinaryNodeCount;
            subset_t mOrdinaryNodes;
            vector<Graph::Node> mExtraordinaryNodes;

            Nodes(Component& pComponent,
                  vector<Graph::Node>& pExtraordinaryNodes)
                : mGraph(pComponent.mGraph), mKmers(pComponent.mKmers),
                  mOrdinaryNodes(sComponentNodesName, pComponent.mFac),
                  mExtraordinaryNodes(pExtraordinaryNodes)
            {
                mOrdinaryNodeCount = mOrdinaryNodes.count();
                mNodeCount = mOrdinaryNodeCount + mExtraordinaryNodes.size();
            }

            Graph::Node underlyingNode(uint64_t pN) const
            {
                BOOST_ASSERT(pN < mNodeCount);
                if (pN < mOrdinaryNodeCount)
                {
                    Graph::Edge e = mGraph.select(
                        mKmers.select(mOrdinaryNodes.select(pN)));
                    return mGraph.from(e);
                }
                else
                {
                    return mExtraordinaryNodes[pN - mOrdinaryNodeCount];
                }
            }

            uint64_t lookupNode(const Graph::Node& pNode) const
            {
                pair<uint64_t,uint64_t> granks = mGraph.beginEndRank(pNode);
                if (granks.first < granks.second)
                {
                    pair<uint64_t,uint64_t> eranks
                        = mKmers.rank(granks.first,granks.second);
                    if (eranks.first < eranks.second)
                    {
                        return mOrdinaryNodes.rank(eranks.first);
                    }
                }
                vector<Graph::Node>::const_iterator it
                   = lower_bound(mExtraordinaryNodes.begin(),
                                 mExtraordinaryNodes.end(), pNode);
                BOOST_ASSERT(it != mExtraordinaryNodes.end());
                return it - mExtraordinaryNodes.begin() + mOrdinaryNodeCount;
            }
        };

        void buildNodes();

    public:
        uint64_t edgeCount() const
        {
            return mKmers.count();
        }

        uint64_t nodeCount() const
        {
            return mNodes->mNodeCount;
        }

        uint32_t coverage(edge_t pEdge) const
        {
            BOOST_ASSERT(pEdge.value() < mCoverage.size());
            return mCoverage[pEdge.value()];
        }

        void setCoverage(edge_t pEdge, uint32_t pCoverage)
        {
            BOOST_ASSERT(pEdge.value() < mCoverage.size());
            mCoverage[pEdge.value()] = pCoverage;
        }

        Graph::Node node(node_t pN) const
        {
            Graph::Node n = mNodes->underlyingNode(pN.value());

#ifdef FULL_SANITY_CHECK
            if (mNodes->lookupNode(n) != pN.value())
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Sanity check failed in node()"));
            }
#endif

            return n;
        }

        node_t lookupNode(const Graph::Node& pNode) const
        {
            node_t n(mNodes->lookupNode(pNode));

#ifdef FULL_SANITY_CHECK
            if (mNodes->underlyingNode(n.value()) != pNode)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Sanity check failed in lookupNode()"));
            }
#endif
            return n;
        }

        edge_t edgeSelect(uint64_t pEdgeR) const
        {
            return edge_t(pEdgeR);
        }

        uint64_t edgeRank(edge_t pE) const
        {
            return pE.value();
        }

        node_t nodeSelect(uint64_t pNodeR) const
        {
            return node_t(pNodeR);
        }

        uint64_t nodeRank(node_t pN) const
        {
            return pN.value();
        }

        node_t from(edge_t pEdge) const
        {
            BOOST_ASSERT(pEdge.value() < mKmers.count());
            Graph::Node n = mGraph.from(mGraph.select(mKmers.select(pEdge.value())));
            return lookupNode(n);
        }

        node_t to(edge_t pEdge) const
        {
            BOOST_ASSERT(pEdge.value() < mKmers.count());
            Graph::Node n = mGraph.to(mGraph.select(mKmers.select(pEdge.value())));
            return lookupNode(n);
        }

        uint64_t countInEdges(node_t pN) const
        {
            uint64_t retval = 0;

            pair<uint64_t,uint64_t> rcRanks
                = mGraph.beginEndRank(mGraph.reverseComplement(
                    underlyingNode(pN)));

            for (uint64_t i = rcRanks.first; i < rcRanks.second; ++i)
            {
                if (mKmers.access(mGraph.rank(
                        mGraph.reverseComplement(mGraph.select(i)))))
                {
                    ++retval;
                }
            }

            return retval;
        }

        void fetchInEdges(node_t pN, vector<edge_t>& pEdges) const
        {
            pair<uint64_t,uint64_t> rcRanks
                = mGraph.beginEndRank(mGraph.reverseComplement(underlyingNode(pN)));

            pEdges.clear();
            for (uint64_t i = rcRanks.first; i < rcRanks.second; ++i)
            {
                uint64_t rnk;
                if (mKmers.accessAndRank(mGraph.rank(
                        mGraph.reverseComplement(mGraph.select(i))), rnk))
                {
                    BOOST_ASSERT(rnk < mCoverage.size());
                    pEdges.push_back(edge_t(rnk));
                }
            }
        }

        uint64_t countOutEdges(node_t pN) const
        {
            pair<uint64_t,uint64_t> ranks
                = mGraph.beginEndRank(underlyingNode(pN));

            return mKmers.countRange(ranks.first, ranks.second);
        }

        void fetchOutEdges(node_t pN, vector<edge_t>& pEdges) const
        {
            pair<uint64_t,uint64_t> ranks
                = mGraph.beginEndRank(underlyingNode(pN));

            pEdges.clear();
            for (uint64_t i = ranks.first; i < ranks.second; ++i)
            {
                uint64_t rnk;
                if (mKmers.accessAndRank(i, rnk))
                {
                    BOOST_ASSERT(rnk < mCoverage.size());
                    pEdges.push_back(edge_t(rnk));
                }
            }
        }

        void sanityCheckGraph() const
        {
            vector<edge_t> inEdges;
            vector<edge_t> outEdges;
            inEdges.reserve(sMaxEdgesPerNode);
            outEdges.reserve(sMaxEdgesPerNode);

            uint64_t inDegreeSum = 0;
            uint64_t outDegreeSum = 0;

            for (uint64_t i = 0; i < edgeCount(); ++i)
            {
                edge_t e = edgeSelect(i);
                node_t f = from(e);
                node_t t = to(e);

                fetchOutEdges(f, outEdges);
                bool fromNodeOK = false;
                for (edge_t oe: outEdges)
                {
                    fromNodeOK |= oe == e;
                }
                BOOST_ASSERT(fromNodeOK);

                fetchInEdges(t, inEdges);
                bool toNodeOK = false;
                for (edge_t ie: inEdges)
                {
                    toNodeOK |= ie == e;
                }
                BOOST_ASSERT(toNodeOK);
            }

            for (uint64_t i = 0; i < nodeCount(); ++i)
            {
                node_t n = nodeSelect(i);

                fetchInEdges(n, inEdges);
                fetchOutEdges(n, outEdges);
                inDegreeSum += inEdges.size();
                outDegreeSum += outEdges.size();

                for (edge_t e: inEdges)
                {
                    BOOST_ASSERT(to(e) == n);
                }

                for (edge_t e: outEdges)
                {
                    BOOST_ASSERT(from(e) == n);
                }
            }
            BOOST_ASSERT(inDegreeSum == outDegreeSum);
        }

        bool empty() const
        {
            return !mNodes->mNodeCount;
        }

        bool scheduledForRemove(edge_t pEdge) const
        {
            BOOST_ASSERT(pEdge.value() < mToRemove.size());
            return mToRemove[pEdge.value()];
        }

        void scheduleForRemove(const boost::dynamic_bitset<uint64_t>& pEdges)
        {
            BOOST_ASSERT(pEdges.size() == mToRemove.size());
            mToRemove |= pEdges;
        }

        void scheduleForRemove(edge_t pEdge)
        {
            BOOST_ASSERT(pEdge.value() < mToRemove.size());
            mToRemove[pEdge.value()] = true;
        }

        bool subsetAccess(uint64_t pEdgeRank) const
        {
            return mKmers.access(pEdgeRank);
        }

        edge_t subsetRank(uint64_t pEdgeRank) const
        {
            return edge_t(mKmers.rank(pEdgeRank));
        }

        uint64_t subsetSelect(edge_t pE) const
        {
            return mKmers.select(pE.value());
        }

        Graph::Node underlyingNode(node_t pNode) const
        {
            return mNodes->underlyingNode(pNode.value());
        }

        Graph::Edge underlyingEdge(edge_t pEdge) const
        {
            return mGraph.select(mKmers.select(pEdge.value()));
        }

        template<typename It>
        Component(Graph& pGraph, FileFactory& pFac, It pCovBegin, It pCovEnd);

        void dumpByVert(const vector<node_t>& pComponent, ostream& pOut) const;

        void dumpByEdge(const vector<edge_t>& pComponent, ostream& pOut) const;
    };


    static inline void
    seqPath(const Graph& pGraph, const vector<rank_type>& pPath,
            SmallBaseVector& pVec)
    {
        if (pPath.empty())
        {
            return;
        }
        pGraph.seq(pGraph.from(pGraph.select(pPath[0])), pVec);
        for (rank_type edge: pPath)
        {
            pVec.push_back(pGraph.select(edge).value() & 3);
        }
    }


    struct Transcript
    {
        vector<rank_type> mEdges;
        double mFPKM;


        template<typename It>
        Transcript(It pBegin, It pEnd)
            : mEdges(pBegin, pEnd),
              mFPKM(0)
        {
        }

        void seq(Graph& pGraph, SmallBaseVector& pVec) const
        {
            seqPath(pGraph, mEdges, pVec);
        }

        struct CompareByLength
        {
            bool operator()(const Transcript& pLhs,
                            const Transcript& pRhs) const
            {
                return pLhs.mEdges.size() < pRhs.mEdges.size();
            }
        };
    };

    class ShortestPaths
    {
    public:
        typedef uint64_t distance_type;

    private:
        Impl& mImpl;
        dynamic_bitset<uint64_t> mSubcomponent;

        static const uint64_t sCacheSize = 1000ull;

        typedef std::unordered_map<node_t,distance_type> distance_map_t;

        boost::mt19937 mRng;
        typedef std::unordered_map<node_t,distance_map_t> cache_map_t;
        cache_map_t mCache;

        void shortestPathsFrom(node_t pFrom);

        void ejectElementFromCache()
        {
            boost::uniform_int<> dist(0, mCache.size() - 1);
            cache_map_t::iterator it = mCache.begin();
            std::advance(it, dist(mRng));
            mCache.erase(it);
        }

    public:
        ShortestPaths(Impl& pImpl, const vector<node_t>& pSubcomponent)
            : mImpl(pImpl), mRng(19991124ull)
        {
            const Component& comp = *mImpl.mComponent;
            mSubcomponent.resize(comp.nodeCount());
            for (node_t n: pSubcomponent)
            {
                mSubcomponent[comp.nodeRank(n)] = true;
            }
        }

        optional<distance_type> distance(node_t pFrom, node_t pTo);

        void shortestPath(node_t pFrom, node_t pTo, std::set<edge_t>& pPath);
    };

    // Tarjan's SCC algorithm uses depth-first search. However, these
    // graphs can be far too big to use the C++ stack for this purpose,
    // so we have to use an explicit stack.
    //
    // You are not expected to understand this.
    //
    struct SCC
    {
        const Impl& mImpl;
        dynamic_bitset<uint64_t> mOnStack;
        typedef unordered_map<node_t, pair<uint64_t,uint64_t> > index_t;
        index_t mIndex;
        uint64_t mDFOrdering;
        deque<node_t> mS;
        deque< vector<node_t> > mSCCs;

        // The contents of a stack frame.
        struct StackFrame
        {
            // Pseudo program counter.
            enum { pcENTRY, pcLOOP, pcRETURN } pc;

            // Current vertex.
            node_t v;

            // Out edges from v, and which one we are currently working on.
            vector<edge_t> outEdges;
            uint64_t i;

            // Child vertex.
            node_t w;

            StackFrame(node_t pV)
                : pc(pcENTRY), v(pV), w(0)
            {
            }
        };
        deque<StackFrame> mCallStack;

        void scc(node_t pV)
        {
            const Component& comp = *mImpl.mComponent;

            if (mIndex.count(pV))
            {
                return;
            }

            mCallStack.push_back(StackFrame(pV));

            while (!mCallStack.empty())
            {
                StackFrame& f = mCallStack.back();
                switch (f.pc)
                {
                    case StackFrame::pcENTRY:
                    {
                        node_t v = f.v;
                        mIndex.emplace(v, make_pair(mDFOrdering,mDFOrdering));
#ifdef DEBUG_SCC
                        std::cerr << "ENTRY scc(" << v << ")\n";
                        std::cerr << "  index[" << v << "] = lowlink["
                                   << v << "] = " << mDFOrdering << '\n';
#endif

                        ++mDFOrdering;
                        mS.push_back(v);
                        mOnStack[comp.nodeRank(v)] = true;
#ifdef DEBUG_SCC
                        std::cerr << "  node stack depth " << mS.size() << '\n';
#endif

                        f.outEdges.reserve(sMaxEdgesPerNode);
                        comp.fetchOutEdges(v, f.outEdges);
#ifdef DEBUG_SCC
                        std::cerr << "  " << f.outEdges.size() << " out edges\n";
#endif

                        f.i = 0;
                        f.pc = StackFrame::pcLOOP;
                        break;
                    }

                    case StackFrame::pcRETURN:
                    {
                        uint64_t& vlowlink = mIndex[f.v].second;
                        vlowlink = std::min(vlowlink, mIndex[f.w].second);
                        ++f.i;
                        f.pc = StackFrame::pcLOOP;
                        // Fallthrough
                    }

                    case StackFrame::pcLOOP:
                    {
                        if (f.i >= f.outEdges.size())
                        {
                            // The loop over children is done.
                            const index_t::mapped_type& index = mIndex[f.v];
                            if (index.first == index.second)
                            {
                                // We have a SCC!
#ifdef DEBUG_SCC
                                std::cerr << "  Extracting SCC\n";
#endif
                                vector<node_t> scc;
                                node_t w;
                                do
                                {
                                    Gossamer::ensureCapacity(scc);
                                    w = mS.back();
                                    mS.pop_back();
                                    mOnStack[comp.nodeRank(w)] = false;
                                    scc.push_back(w);
#ifdef DEBUG_SCC
                                    std::cerr << "    " << w << '\n';
#endif
                                } while (w != f.v);

                                mSCCs.push_back(std::move(scc));
                            }

#ifdef DEBUG_SCC
                            std::cerr << "RETURN scc(" << f.v << ")\n";
#endif
                            // Return from recursive call.
                            mCallStack.pop_back();
                            break;
                        }
#ifdef DEBUG_SCC
                        std::cerr << "  child edge " << f.i << '\n';
#endif

                        f.w = comp.to(f.outEdges[f.i]);
                        index_t::iterator wit = mIndex.find(f.w);
#ifdef DEBUG_SCC
                        std::cerr << "  w = " << f.w << '\n';
#endif
                        if (wit == mIndex.end())
                        {
                            f.pc = StackFrame::pcRETURN;
                            mCallStack.push_back(StackFrame(f.w));
#ifdef DEBUG_SCC
                            std::cerr << "  call stack depth " << mCallStack.size() << '\n';
#endif
                            break;
                        }
                        else if (mOnStack[comp.nodeRank(f.w)])
                        {
                            uint64_t& vlowlink = mIndex[f.v].second;
                            vlowlink = std::min(vlowlink, wit->second.first);
                        }
                        ++f.i;
                        // f.pc = StackFrame::pcLOOP;
                        break;
                    }
                }
            }
        }

        void init()
        {
            const Component& comp = *mImpl.mComponent;
            mOnStack.resize(comp.nodeCount());
            mDFOrdering = 0;
        }

        void cleanup()
        {
            mIndex.clear();
        }

        SCC(const Impl& pImpl)
            : mImpl(pImpl)
        {
            init();
            const Component& comp = *mImpl.mComponent;
            for (uint64_t v = 0; v < comp.nodeCount(); ++v)
            {
                scc(node_t(v));
            }
            cleanup();
        }

        SCC(const Impl& pImpl, const vector<node_t>& pSubcompByNodes)
            : mImpl(pImpl)
        {
            init();
            for (node_t v: pSubcompByNodes)
            {
               scc(v);
            }
            cleanup();
        }
    };

    string mName;
    StringFileFactory mStringFac;
    Graph& mGraph;
    Logger& mLog;
    ostream& mOut;
    uint64_t mMinLength;
    uint64_t mMinRhomers;
    uint64_t mMappableReads;

    vector<SmallBaseVector> mContigs;
#ifdef RETAIN_UNMAPPED_READS
    deque< pair<SmallBaseVector,SmallBaseVector> > mUnmappedPairs;
#endif
    ptr_vector<MappedRead> mReads;
    ptr_vector<VerifiedRead> mVReads;

    typedef unordered_multimap<rank_type,uint64_t> read_kmer_index_t;
    read_kmer_index_t mReadKmerIndex;

    vector<uint32_t> mReadKmerCount;

    std::shared_ptr<Build> mBuild;

    std::shared_ptr<Component> mComponent;

    vector< vector<node_t> > sccs() const;

    vector< vector<node_t> > componentsByVertex() const;

    vector< vector<edge_t> > componentsByEdge() const;

    void indexReadsByKmer()
    {
        for (uint64_t i = 0; i < mVReads.size(); ++i)
        {
            const VerifiedRead& r = mVReads[i];
            mReadKmerIndex.insert(make_pair(r.mEdges[0], i));
        }
    }

    void commitEdgeRemove();

    ptr_vector<Transcript> mTranscripts;

    Impl(const string& pName, Graph& pGraph, Logger& pLog, ostream& pOut,
         uint64_t pMinLength, uint64_t pMappableReads)
        : mName(pName), mStringFac(), mGraph(pGraph), mLog(pLog), mOut(pOut),
          mMinLength(pMinLength), mMappableReads(pMappableReads)
    {
        const uint64_t K = mGraph.K();
        mMinRhomers = mMinLength < K ? 0 : mMinLength - K + 1;
        mBuild = std::make_shared<Build>(mGraph.count());
    }

    uint64_t readMaps(const SmallBaseVector& pRead) const
    {
        uint64_t rho = mGraph.K() + 1;
        uint64_t size = pRead.size();
        if (size < rho)
        {
            return 0;
        }
        size -= rho;

        uint64_t hits = 0;
        dynamic_bitset<uint64_t>& contigEdges = mBuild->mContigEdges;

        for (uint64_t i = 0; i < size; ++i)
        {
            Graph::Edge e(pRead.kmer(rho, i));
            uint64_t rnk = 0;
            bool maps = mGraph.accessAndRank(e, rnk);
            if (maps && contigEdges[rnk])
            {
                ++hits;
            }
        }

        return hits;
    }

    uint64_t addRead(const SmallBaseVector& pRead)
    {
        uint64_t rho = mGraph.K() + 1;
        uint64_t size = pRead.size() - mGraph.K() - 1;
        uint64_t readId = mReads.size();
        Build& build = *mBuild;

        vector<uint64_t> edgeRank;
        edgeRank.reserve(size-1);
        vector<bool> edgeMaps;
        edgeMaps.reserve(size-1);

        for (uint64_t i = 0; i < size; ++i)
        {
            Graph::Edge e(pRead.kmer(rho, i));
            uint64_t rnk = 0;
            bool maps = mGraph.accessAndRank(e, rnk);
            edgeRank.push_back(rnk);
            edgeMaps.push_back(maps);
        }

        for (uint64_t i = 0; i < edgeRank.size(); ++i)
        {
            if (!edgeMaps[i])
            {
                continue;
            }
            uint64_t rnk = edgeRank[i];
            ++build.mReadCoverage[rnk];
            build.mGraphEdges[rnk] = true;
        }

        Gossamer::ensureCapacity(mReads);
        mReads.push_back(new MappedRead(edgeRank, edgeMaps));
        return readId;
    }

    void addContig(const SmallBaseVector& pVec);

    void processComponent();

    void constructGraph();

    void clampExtremelyHighEdgeCounts();

    bool trimLowCoverageEdges();

    bool linearPaths();

    void dumpGraphByComponents(const std::string& pStage, ostream& pOut) const;

    void dumpGraph(ostream& pOut);

    void cullComponents();

    void breakCycles();

    struct BreakCyclesContext
    {
        const vector<node_t>& mComponent;
        bool mInvariantsPotentiallyBroken;

        BreakCyclesContext(const vector<node_t>& pComponent)
            : mComponent(pComponent), mInvariantsPotentiallyBroken(false)
        {
        }
    };

    bool breakCircularComponent(const vector<node_t>& pComponent);
    bool breakCyclesComponent(BreakCyclesContext& pCtx);
    bool breakCyclesSubcomponent(BreakCyclesContext& pCtx);

    void verifyReads();

    void extractTranscripts();

    void extractTranscriptsComponent(const vector<node_t>& pComponent);

    void extractTranscriptsLinear(const vector<node_t>& pComponent);

    void extractTranscriptsYshapeIn(const vector<node_t>& pComponent);

    void extractTranscriptsYshapeOut(const vector<node_t>& pComponent);

    void extractTranscriptsSimpleBubble(const vector<node_t>& pComponent);

    void extractTranscriptsComplex(const vector<node_t>& pComponent);

    void makeTranscriptFromPath(ptr_deque<Transcript>& pTranscripts,
                const vector<rank_type>& pPath);

    void makeTranscriptsFromPathBundle(ptr_deque<Transcript>& pTranscripts,
                const PathBundle& pBundle);

    uint64_t trimPathBundle(PathBundle& pBundle,
                            ptr_deque<Transcript>& pTranscripts);

    void quantifyTranscripts();

    void outputTranscripts();
};


template<typename It>
ResolveTranscripts::Impl::Component::Component(Graph& pGraph,
        FileFactory& pFac, It pCovBegin, It pCovEnd)
    : mGraph(pGraph), mFac(pFac), mKmers(sComponentKmersName, pFac),
      mCoverage(pCovBegin, pCovEnd)
{
    mToRemove.resize(mKmers.count());
    buildNodes();
#ifdef FULL_SANITY_CHECK
    sanityCheckGraph();
#endif
}


void
ResolveTranscripts::Impl::Component::buildNodes()
{
    vector<Graph::Node> extraordinaryNodes;

    {
        dynamic_bitset<uint64_t> ordinaryNodes(mKmers.count());

        pair<uint64_t,uint64_t> granks;
        pair<uint64_t,uint64_t> eranks;

        for (DenseArray::Iterator it(mKmers); it.valid(); ++it)
        {
            Graph::Edge e = mGraph.select(*it);
            Graph::Node f = mGraph.from(e);
            Graph::Node t = mGraph.to(e);

            granks = mGraph.beginEndRank(f);
            eranks = mKmers.rank(granks.first, granks.second);

            if (granks.first < granks.second
                && eranks.first < eranks.second)
            {
                ordinaryNodes[eranks.first] = true;
            }
            else
            {
                Gossamer::ensureCapacity(extraordinaryNodes);
                extraordinaryNodes.push_back(f);
            }

            granks = mGraph.beginEndRank(t);
            eranks = mKmers.rank(granks.first, granks.second);
    
            if (granks.first < granks.second
                && eranks.first < eranks.second)
            {
                ordinaryNodes[eranks.first] = true;
            }
            else
            {
                Gossamer::ensureCapacity(extraordinaryNodes);
                extraordinaryNodes.push_back(t);
            }
        }

        denseArrayFromBitmap(sComponentNodesName, mFac, ordinaryNodes);
    }

    Gossamer::sortAndUnique(extraordinaryNodes);

    mNodes = std::make_shared<Nodes>(*this, extraordinaryNodes);
}


void
ResolveTranscripts::Impl::Component::dumpByEdge(
    const vector<edge_t>& pComponent, ostream& pOut) const
{
    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    set<Graph::Node> ns;
    pOut << "digraph G {\n";
    for (edge_t e: pComponent)
    {
        BOOST_ASSERT(edgeRank(e) < edgeCount());
        uint64_t i = mKmers.select(edgeRank(e));
        uint32_t cov = coverage(e);
        Graph::Edge edge(mGraph.select(i));
        Graph::Node f = mGraph.from(edge);
        Graph::Node t = mGraph.to(edge);
        ns.insert(f);
        ns.insert(t);
        SmallBaseVector nf, nt;
        mGraph.seq(f, nf);
        mGraph.seq(t, nt);
        pOut << "  " << nf << " -> " << nt
             << " [label=\"" << cov << "\"] // " << i << '\n';
#ifdef FULL_SANITY_CHECK
        pOut.flush();
#endif
    }

    for (const Graph::Node& n: ns)
    {
        SmallBaseVector vec;
        mGraph.seq(n, vec);
        pOut << "  " << vec << " [label=\"" << nodeRank(lookupNode(n)) << "\"];\n";
#ifdef FULL_SANITY_CHECK
        pOut.flush();
#endif
    }

    pOut << "}\n";
    pOut.flush();
}

void
ResolveTranscripts::Impl::Component::dumpByVert(
    const vector<node_t>& pComponent, ostream& pOut) const
{
    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    set<edge_t> es;
    pOut << "digraph G {\n";
    for (node_t n: pComponent)
    {
        Graph::Node nn = underlyingNode(n);
        SmallBaseVector vec;
        mGraph.seq(nn, vec);
        pOut << "  " << vec << " [label=\"" << nodeRank(n) << "\"];\n";
#ifdef FULL_SANITY_CHECK
        pOut.flush();
#endif

        fetchInEdges(n, inEdges);
        fetchOutEdges(n, outEdges);

        for (edge_t e: inEdges)
        {
            es.insert(e);
        }

        for (edge_t e: outEdges)
        {
            es.insert(e);
        }
    }

    for (edge_t e: es)
    {
        BOOST_ASSERT(edgeRank(e) < mKmers.count());
        uint64_t i = mKmers.select(edgeRank(e));
        uint32_t cov = coverage(e);
        Graph::Edge edge(mGraph.select(i));
        Graph::Node f = mGraph.from(edge);
        Graph::Node t = mGraph.to(edge);
        SmallBaseVector nf, nt;
        mGraph.seq(f, nf);
        mGraph.seq(t, nt);
        pOut << "  " << nf << " -> " << nt
             << " [label=\"" << cov << "\"] // " << i << '\n';
#ifdef FULL_SANITY_CHECK
        pOut.flush();
#endif
    }

    pOut << "}\n";
    pOut.flush();
}


void
ResolveTranscripts::Impl::dumpGraphByComponents(
    const std::string& pStage, ostream& pOut) const
{
    pOut << "# BEGIN Graph at stage " << pStage << '\n';
    for (auto& component: componentsByEdge())
    {
        mComponent->dumpByEdge(component, pOut);
    }
    pOut << "# END Graph at stage " << pStage << '\n';
    pOut.flush();
}


void
ResolveTranscripts::Impl::commitEdgeRemove()
{
    dynamic_bitset<uint64_t> newEdges(mGraph.count());
    deque<uint32_t> newCounts;

    {
        Component& comp = *mComponent;
        for (uint64_t i = 0; i < comp.edgeCount(); ++i)
        {
            edge_t e = comp.edgeSelect(i);
            uint64_t ernk = comp.subsetSelect(e);
            if (!comp.scheduledForRemove(e))
            {
                newEdges[ernk] = true;
                newCounts.push_back(comp.coverage(e));
            }
        }
    }

    mComponent = std::shared_ptr<Component>();
    denseArrayFromBitmap(sComponentKmersName, mStringFac, newEdges);
    mComponent = std::make_shared<Component>(mGraph, mStringFac, newCounts.begin(), newCounts.end());
}



optional<ResolveTranscripts::Impl::ShortestPaths::distance_type>
ResolveTranscripts::Impl::ShortestPaths::distance(node_t pFrom, node_t pTo)
{
    optional<distance_type> retval;

    cache_map_t::const_iterator it = mCache.find(pFrom);
    if (it == mCache.end())
    {
        shortestPathsFrom(pFrom);
        it = mCache.find(pFrom);
    }
    distance_map_t::const_iterator toIt = it->second.find(pTo);
    if (toIt != it->second.end())
    {
        retval = toIt->second;
    }
    return retval;
}


void
ResolveTranscripts::Impl::ShortestPaths::shortestPathsFrom(node_t pFrom)
{
    typedef FibHeap<distance_type,node_t> heap_t;
    Component& comp = *mImpl.mComponent;

    distance_map_t distMap;
    heap_t heap;

    unordered_map<node_t,heap_t::iterator> inHeap;

    vector<edge_t> outEdges;
    outEdges.reserve(sMaxEdgesPerNode);
    comp.fetchOutEdges(pFrom, outEdges);
    for (edge_t e: outEdges)
    {
        node_t node = comp.to(e);
        if (mSubcomponent[comp.nodeRank(node)])
        {
            inHeap[node] = heap.insert(distance_type(1), node);
            distMap[node] = 1;
        }
    }

    dynamic_bitset<uint64_t> seen(comp.nodeCount());

    while (!heap.empty())
    {
        distance_type d = heap.minimum()->mKey;
        node_t v = heap.minimum()->mValue;
        inHeap.erase(v);
        heap.removeMinimum();
        if (seen[comp.nodeRank(v)])
        {
            continue;
        }
        seen[comp.nodeRank(v)] = true;

        comp.fetchOutEdges(v, outEdges);
        for (edge_t e: outEdges)
        {
            node_t v2 = comp.to(e);
            if (seen[comp.nodeRank(v2)] || !mSubcomponent[comp.nodeRank(v2)])
            {
                continue;
            }
            // ComponentGraph::edge_id_t e = edge.second;
            distance_type newDist = d + 1;
            distance_map_t::iterator dit = distMap.find(v2);
            if (dit == distMap.end() || dit->second > newDist)
            {
                unordered_map<node_t,heap_t::iterator>::iterator
                    ihit = inHeap.find(v2);
                distMap[v2] = newDist;
                if (ihit == inHeap.end())
                {
                    heap.insert(newDist, v2);
                }
                else
                {
                    heap.decreaseKey(ihit->second, newDist);
                }
            }
        }
    }

    if (mCache.size() >= sCacheSize)
    {
        ejectElementFromCache();
    }
    mCache[pFrom] = distMap;
}


void
ResolveTranscripts::Impl::ShortestPaths::shortestPath(
        node_t pFrom, node_t pTo, set<edge_t>& pPath)
{
    // cerr << "Extracting shortestPath from " << pFrom << " to " << pTo << '\n';
    optional<distance_type> dist = distance(pFrom, pTo);
    if (!dist)
    {
        return;
    }
    // cerr << "Distance is " << dist << '\n';

    Component& comp = *mImpl.mComponent;

    FibHeap<distance_type, node_t> pq;
    unordered_map<node_t, edge_t> path;

    vector<edge_t> outEdges;
    outEdges.reserve(sMaxEdgesPerNode);
    comp.fetchOutEdges(pFrom, outEdges);
    for (edge_t e: outEdges)
    {
        node_t v = comp.to(e);
        if (mSubcomponent[comp.nodeRank(v)])
        {
            pq.insert(distance_type(1), v);
            path.insert(make_pair(v, e));
        }
    }
    dynamic_bitset<uint64_t> seen(comp.nodeCount());
    unordered_map<node_t,distance_type> distMap;
    bool found = false;

    while (!pq.empty())
    {
        distance_type distance = pq.minimum()->mKey;
        node_t v = pq.minimum()->mValue;
        pq.removeMinimum();
        if (v == pTo)
        {
            found = true;
            break;
        }
        if (seen[comp.nodeRank(v)])
        {
            continue;
        }
        seen[comp.nodeRank(v)] = true;

        comp.fetchOutEdges(v, outEdges);

        for (edge_t e: outEdges)
        {
            node_t v2 = comp.to(e);
            if (!mSubcomponent[comp.nodeRank(v2)])
            {
                continue;
            }
            distance_type newDist = distance + 1;
            unordered_map<node_t,distance_type>::iterator it = distMap.find(v2);
            // cerr << "Edge " << v << " -> " << v2 << '\n';
            if (!seen[comp.nodeRank(v2)] && (it == distMap.end() || newDist < it->second))
            {
                distMap[v2] = newDist;
                // cerr << "   ...which is a new best path.\n";
                path.insert(pair<node_t,edge_t>(v2, e));
                pq.insert(newDist, v2);
            }
        }
    }
    if (!found)
    {
        // cerr << "Not found!\n";
        return;
    }
    // cerr << "Found!\n";

    node_t v = pTo;
    do
    {
        edge_t edge = path.find(v)->second;
        pPath.insert(edge);
        v = comp.from(edge);
    } while (v != pFrom);
}


vector< vector<node_t> >
ResolveTranscripts::Impl::sccs() const
{
    vector< vector<node_t> > retval;
    SCC scc(*this);
    retval.reserve(scc.mSCCs.size());
    retval.assign(scc.mSCCs.begin(), scc.mSCCs.end());
    return retval;
}


vector< vector<node_t> >
ResolveTranscripts::Impl::componentsByVertex() const
{
    vector< vector<node_t> > components;

    const Component& comp = *mComponent;

#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "    Calculating components by vertex");
    mLog(info, "        " + lexical_cast<string>(comp.edgeCount()) + " edges");
    mLog(info, "        " + lexical_cast<string>(comp.nodeCount()) + " nodes");
#endif

    if (comp.empty())
    {
        return components;
    }

    vector_property_map<uint32_t> rank_map(comp.nodeCount());
    vector_property_map<uint32_t> parent_map(comp.nodeCount());

    disjoint_sets< vector_property_map<uint32_t>,
                   vector_property_map<uint32_t> >
        sets(rank_map, parent_map);

    for (uint64_t i = 0; i < comp.nodeCount(); ++i)
    {
        sets.make_set(i);
    }

    for (uint64_t i = 0; i < comp.edgeCount(); ++i)
    {
        edge_t e = comp.edgeSelect(i);
        node_t from = comp.from(e);
        node_t to = comp.to(e);
        sets.union_set(comp.nodeRank(from), comp.nodeRank(to));
    }

    sets.compress_sets(
        make_counting_iterator<uint32_t>(0),
        make_counting_iterator<uint32_t>(comp.nodeCount()));

    vector<size_t> cs;
    cs.reserve(comp.nodeCount());

    for (uint32_t i = 0; i < comp.nodeCount(); ++i)
    {
        cs.push_back(get(parent_map,i));
    }

    Gossamer::sortAndUnique(cs);

    uint32_t numComponents = cs.size();

    components.resize(numComponents);
#ifdef MORE_INTERNAL_PROGRESS
    vector<uint64_t> sizes(numComponents);
#endif

    for (uint64_t i = 0; i < comp.nodeCount(); ++i)
    {
        vector<size_t>::const_iterator
            ii = lower_bound(cs.begin(), cs.end(), get(parent_map, i));
        components[ii - cs.begin()].push_back(comp.nodeSelect(i));
#ifdef MORE_INTERNAL_PROGRESS
        ++sizes[ii - cs.begin()];
#endif
    }
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "    Component sizes:");
    typedef map<uint64_t,uint64_t> histo_map_t;
    histo_map_t histo;
    for (auto size: sizes)
    {
        ++histo[size];
    }
    for (auto& h: histo)
    {
        mLog(info, "      " + lexical_cast<string>(h.first) + "\t"
            + lexical_cast<string>(h.second));
    }
#endif

    return components;
}


vector< vector<edge_t> >
ResolveTranscripts::Impl::componentsByEdge() const
{
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "    Calculating components by edge");
#endif
    vector< vector<edge_t> > components;

    const Component& comp = *mComponent;

    if (comp.empty())
    {
        return components;
    }

    uint64_t numEdges = comp.edgeCount();

    vector_property_map<uint32_t> rank_map(numEdges);
    vector_property_map<uint32_t> parent_map(numEdges);

    disjoint_sets< vector_property_map<uint32_t>,
                   vector_property_map<uint32_t> >
        sets(rank_map, parent_map);

    for (uint64_t i = 0; i < numEdges; ++i)
    {
        sets.make_set(i);
    }

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    for (uint64_t i = 0; i < comp.nodeCount(); ++i)
    {
        node_t n = comp.nodeSelect(i);
        comp.fetchInEdges(n, inEdges);
        comp.fetchOutEdges(n, outEdges);

        for (size_t i = 1; i < inEdges.size(); ++i)
        {
            sets.union_set(comp.edgeRank(inEdges[i-1]),
                           comp.edgeRank(inEdges[i]));
        }

        for (size_t i = 1; i < outEdges.size(); ++i)
        {
            sets.union_set(comp.edgeRank(outEdges[i-1]),
                           comp.edgeRank(outEdges[i]));
        }

        if (!inEdges.empty() && !outEdges.empty())
        {
            sets.union_set(comp.edgeRank(inEdges[0]),
                           comp.edgeRank(outEdges[0]));
        }
    }

    sets.compress_sets(
        make_counting_iterator<uint32_t>(0),
        make_counting_iterator<uint32_t>(numEdges));
    BOOST_ASSERT(components.size() == 0);

    vector<size_t> cs;
    cs.reserve(comp.nodeCount());

    for (uint32_t i = 0; i < comp.edgeCount(); ++i)
    {
        cs.push_back(get(parent_map,i));
    }

    Gossamer::sortAndUnique(cs);

    uint32_t numComponents = cs.size();

    components.resize(numComponents);
#ifdef MORE_INTERNAL_PROGRESS
    vector<uint64_t> sizes(numComponents);
#endif

    for (uint64_t i = 0; i < comp.edgeCount(); ++i)
    {
        vector<size_t>::const_iterator
            ii = lower_bound(cs.begin(), cs.end(), get(parent_map, i));
        components[ii - cs.begin()].push_back(comp.edgeSelect(i));
#ifdef MORE_INTERNAL_PROGRESS
        ++sizes[ii - cs.begin()];
#endif
    }
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "    Component sizes:");
    typedef map<uint64_t,uint64_t> histo_map_t;
    histo_map_t histo;
    for (auto size: sizes)
    {
        ++histo[size];
    }
    for (auto& h: histo)
    {
        mLog(info, "      " + lexical_cast<string>(h.first) + "\t"
            + lexical_cast<string>(h.second));
    }
#endif

    return components;
}


// The read pairs may contain edges which don't cleanly map to the
// cleaned-up component.  The purpose of this function is to reprocess
// the reads so that they align perfectly, and then compact the reads
// so that duplicates are handled together.
//
// At the moment, we do the simplest possible thing: If there are edges
// which are not in the component, we split the read at that point,
// discarding any fragments which are pointlessly small.
//
// There is a lot of scope for improvement here, though. We could try
// to correct reads.  We could also try the ALLPATHS thing: turn the
// pair into one long read if there is a unique path between them.
//
void
ResolveTranscripts::Impl::verifyReads()
{
    const int64_t sMinEdges = 2;

    Component& comp = *mComponent;

    mVReads.reserve(mReads.size());
    mReadKmerCount.clear();
    mReadKmerCount.resize(comp.edgeCount());

    for (auto& r: mReads)
    {
        vector<rank_type>::const_iterator beg = r.mEdges.begin();
        vector<rank_type>::const_iterator end = r.mEdges.begin();
        
        for (uint64_t i = 0; i < r.mEdges.size(); ++i)
        {
            vector<rank_type>::const_iterator ii = r.mEdges.begin();
            std::advance(ii, i);

            bool edgeMaps = r.mEdgeMaps[i] && comp.subsetAccess(r.mEdges[i]);
            if (edgeMaps)
            {
                ++mReadKmerCount[comp.edgeRank(comp.subsetRank(r.mEdges[i]))];
                if (beg == end)
                {
                    beg = end = ii;
                }
                ++end;
            }
            else
            {
                if (beg != end)
                {
                    if (std::distance(beg,end) >= sMinEdges)
                    {
                        Gossamer::ensureCapacity(mVReads);
                        mVReads.push_back(new VerifiedRead(beg, end));
                    }
                }
                beg = end;
            }
        }
        if (beg != end)
        {
            if (std::distance(beg,end) >= sMinEdges)
            {
                Gossamer::ensureCapacity(mVReads);
                mVReads.push_back(new VerifiedRead(beg, end));
            }
        }
    }
    mReads.clear();

#ifdef MORE_INTERNAL_PROGRESS
    std::cerr << "    Before compaction: " << mVReads.size() << " reads.\n";
#endif
    if (mVReads.size() > 1)
    {
        sort(mVReads.begin(), mVReads.end());
        uint64_t i = 0;
        uint64_t j = 1;
        while (j < mVReads.size())
        {
            if (mVReads[i] == mVReads[j])
            {
                mVReads[i].mCount += mVReads[j].mCount;
                ++j;
            }
            else
            {
                ++i;
                if (i != j)
                {
                    std::swap(mVReads[i], mVReads[j]);
                }
                ++j;
            }
        }
        ptr_vector<VerifiedRead>::iterator ii = mVReads.begin();
        std::advance(ii, i);
        mVReads.erase(ii, mVReads.end());
    }
#ifdef MORE_INTERNAL_PROGRESS
    std::cerr << "    After compaction: " << mVReads.size() << " reads.\n";
#endif
}


void
ResolveTranscripts::Impl::extractTranscripts()
{
    indexReadsByKmer();

    vector< vector<node_t> > components = componentsByVertex();

#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "  Looping over " + lexical_cast<string>(components.size()) + " components");
#endif
    for (auto& component: components)
    {
        if (component.size() < 2 || component.size() + 1 < mMinRhomers)
        {
            continue;
        }
#ifdef MORE_INTERNAL_PROGRESS
        mLog(info, "  Component of size "
                   + lexical_cast<string>(component.size()));
#endif

        extractTranscriptsComponent(component);
    }

    mReadKmerIndex.clear();
}


void
ResolveTranscripts::Impl::extractTranscriptsComponent(
        const vector<node_t>& pComponent)
{
#ifndef DONT_SPECIAL_CASE_PATH_TRACING

    // Count the degrees of every node.  Note that we don't care
    // about indegree or outdegree 1, since these are linear paths.

    uint64_t inDegreeZero = 0;
    uint64_t outDegreeZero = 0;
    uint64_t inDegreeTwo = 0;
    uint64_t outDegreeTwo = 0;
    uint64_t inDegreeThreeOrMore = 0;
    uint64_t outDegreeThreeOrMore = 0;
#ifdef FULL_SANITY_CHECK
    uint64_t inDegreeSum = 0;
    uint64_t outDegreeSum = 0;
#endif

    {
        Component& comp = *mComponent;

        for (node_t v: pComponent)
        {
            uint64_t inDegree = comp.countInEdges(v);
#ifdef FULL_SANITY_CHECK
            inDegreeSum += inDegree;
#endif
            switch (inDegree)
            {
                case 0: ++inDegreeZero; break;
                case 1: break;
                case 2: ++inDegreeTwo; break;
                default: ++inDegreeThreeOrMore; break;
            }
            uint64_t outDegree = comp.countOutEdges(v);
#ifdef FULL_SANITY_CHECK
            outDegreeSum += outDegree;
#endif
            switch (outDegree)
            {
                case 0: ++outDegreeZero; break;
                case 1: break;
                case 2: ++outDegreeTwo; break;
                default: ++outDegreeThreeOrMore; break;
            }
#ifdef DEBUG_EXTRACTION
            SmallBaseVector vec;
            mGraph.seq(comp.underlyingNode(v), vec);
            std::cerr << "Node " << vec << " in " << inDegree << " out " << outDegree << '\n';
#endif
        }
    }
#ifdef DEBUG_EXTRACTION
    std::cerr << "  Degree counts:\n";
    std::cerr << "      0: " << inDegreeZero << ", " << outDegreeZero << '\n';
    std::cerr << "      2: " << inDegreeTwo << ", " << outDegreeTwo << '\n';
    std::cerr << "      3+: " << inDegreeThreeOrMore << ", " << outDegreeThreeOrMore << '\n';
#endif

#ifdef FULL_SANITY_CHECK
    BOOST_ASSERT(inDegreeSum == outDegreeSum);
#endif

    if (inDegreeZero == 1 && outDegreeZero == 1
        && inDegreeTwo == 0 && outDegreeTwo == 0
        && inDegreeThreeOrMore == 0 && outDegreeThreeOrMore == 0)
    {
#ifdef DEBUG_EXTRACTION
        std::cerr << "  Linear case\n";
#endif
        extractTranscriptsLinear(pComponent);
        return;
    }

    if (inDegreeZero == 1 && outDegreeZero == 2
        && inDegreeTwo == 0 && outDegreeTwo == 1
        && inDegreeThreeOrMore == 0 && outDegreeThreeOrMore == 0)
    {
#ifdef DEBUG_EXTRACTION
        std::cerr << "  Y-shape with linear path in\n";
#endif
        extractTranscriptsYshapeIn(pComponent);
        return;
    }

    if (inDegreeZero == 2 && outDegreeZero == 1
        && inDegreeTwo == 1 && outDegreeTwo == 0
        && inDegreeThreeOrMore == 0 && outDegreeThreeOrMore == 0)
    {
#ifdef DEBUG_EXTRACTION
        std::cerr << "  Y-shape with linear path out\n";
#endif
        extractTranscriptsYshapeOut(pComponent);
        return;
    }

    if (inDegreeZero == 1 && outDegreeZero == 1
        && inDegreeTwo == 1 && outDegreeTwo == 1
        && inDegreeThreeOrMore == 0 && outDegreeThreeOrMore == 0)
    {
#ifdef DEBUG_EXTRACTION
        std::cerr << "  Simple bubble\n";
#endif
        extractTranscriptsSimpleBubble(pComponent);
        return;
    }
#endif

#ifdef DEBUG_EXTRACTION
    std::cerr << "  Complex case\n";
    std::cerr << "Subcomponent contains " << pComponent.size() << " nodes\n";
#endif
    extractTranscriptsComplex(pComponent);
}


void
ResolveTranscripts::Impl::extractTranscriptsLinear(
        const vector<node_t>& pComponent)
{
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    node_t n;
    bool startNodeFound = false;

    for (node_t v: pComponent)
    {
        if (!comp.countInEdges(v))
        {
            n = v;
            startNodeFound = true;
            break;
        }
    }

    BOOST_ASSERT(startNodeFound);

    vector<rank_type> rpath;
    rpath.reserve(pComponent.size());

    for (;;)
    {
        comp.fetchOutEdges(n, outEdges);
        BOOST_ASSERT(outEdges.size() < 2);
        if (!outEdges.size())
        {
            break;
        }
        edge_t edge = outEdges.front();
        rpath.push_back(comp.subsetSelect(edge));
        n = comp.to(edge);
    }

    const uint64_t K = mGraph.K();
    const uint64_t minLength = mMinLength < K ? 0 : mMinLength - K;
    if (rpath.size() < minLength)
    {
        return;
    }

    mTranscripts.push_back(new Transcript(rpath.begin(), rpath.end()));
}


void
ResolveTranscripts::Impl::extractTranscriptsYshapeIn(
        const vector<node_t>& pComponent)
{
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    node_t n;
    bool startNodeFound = false;

    for (node_t v: pComponent)
    {
        comp.fetchOutEdges(v, outEdges);
        if (outEdges.size() == 2)
        {
            n = v;
            startNodeFound = true;
            break;
        }
    }

    BOOST_ASSERT(startNodeFound);

    deque<rank_type> rpathUpper;
    deque<rank_type> rpathLower;

    // Go back along the common path first.
    node_t nn = n;
    for (;;)
    {
        comp.fetchInEdges(nn, inEdges);
        BOOST_ASSERT(inEdges.size() < 2);
        if (!inEdges.size())
        {
            break;
        }
        edge_t edge = inEdges.front();
        rank_type e = comp.subsetSelect(edge);
        rpathUpper.push_front(e);
        rpathLower.push_front(e);
        nn = comp.from(edge);
    }

    // Extend upper path forward.
    nn = n;
    for (;;)
    {
        comp.fetchOutEdges(nn, outEdges);
        if (!outEdges.size())
        {
            break;
        }
        edge_t edge = outEdges.front();
        rpathUpper.push_back(comp.subsetSelect(edge));
        nn = comp.to(edge);
    }

    // Extend lower path forward.
    nn = n;
    for (;;)
    {
        comp.fetchOutEdges(nn, outEdges);
        if (!outEdges.size())
        {
            break;
        }
        edge_t edge = outEdges.back();
        rpathLower.push_back(comp.subsetSelect(edge));
        nn = comp.to(edge);
    }

    const uint64_t K = mGraph.K();
    const uint64_t minLength = mMinLength < K ? 0 : mMinLength - K;
    if (rpathUpper.size() >= minLength)
    {
        mTranscripts.push_back(
            new Transcript(rpathUpper.begin(), rpathUpper.end())
        );
    }
    if (rpathLower.size() >= minLength)
    {
        mTranscripts.push_back(
            new Transcript(rpathLower.begin(), rpathLower.end())
        );
    }
}


void
ResolveTranscripts::Impl::extractTranscriptsYshapeOut(
        const vector<node_t>& pComponent)
{
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    node_t n;
    bool startNodeFound = false;

    for (node_t v: pComponent)
    {
        if (comp.countInEdges(v) == 2)
        {
            n = v;
            startNodeFound = true;
            break;
        }
    }

    BOOST_ASSERT(startNodeFound);

    deque<rank_type> rpathUpper;
    deque<rank_type> rpathLower;

    // Go along the common path first.
    node_t nn = n;
    for (;;)
    {
        comp.fetchOutEdges(nn, outEdges);
        BOOST_ASSERT(outEdges.size() < 2);
        if (!outEdges.size())
        {
            break;
        }
        edge_t edge = outEdges.front();
        rank_type e = comp.subsetSelect(edge);
        rpathUpper.push_back(e);
        rpathLower.push_back(e);
        nn = comp.to(edge);
    }

    // Extend upper path backwards.
    nn = n;
    for (;;)
    {
        comp.fetchInEdges(nn, inEdges);
        if (!inEdges.size())
        {
            break;
        }
        edge_t edge = inEdges.front();
        rpathUpper.push_front(comp.subsetSelect(edge));
        nn = comp.from(edge);
    }

    // Extend lower path backwards.
    nn = n;
    for (;;)
    {
        comp.fetchInEdges(nn, inEdges);
        if (!inEdges.size())
        {
            break;
        }
        edge_t edge = inEdges.back();
        rpathLower.push_front(comp.subsetSelect(edge));
        nn = comp.from(edge);
    }

    const uint64_t K = mGraph.K();
    const uint64_t minLength = mMinLength < K ? 0 : mMinLength - K;
    if (rpathUpper.size() >= minLength)
    {
        mTranscripts.push_back(
            new Transcript(rpathUpper.begin(), rpathUpper.end())
        );
    }
    if (rpathLower.size() >= minLength)
    {
        mTranscripts.push_back(
            new Transcript(rpathLower.begin(), rpathLower.end())
        );
    }
}


void
ResolveTranscripts::Impl::extractTranscriptsSimpleBubble(
        const vector<node_t>& pComponent)
{
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    node_t n;
    bool startNodeFound = false;

    for (node_t v: pComponent)
    {
        if (comp.countOutEdges(v) == 2)
        {
            n = v;
            startNodeFound = true;
            break;
        }
    }

    BOOST_ASSERT(startNodeFound);

    deque<rank_type> rpathUpper;
    deque<rank_type> rpathLower;

    // Go back along the common path first.
    node_t nn = n;
    for (;;)
    {
        comp.fetchInEdges(nn, inEdges);
        BOOST_ASSERT(inEdges.size() < 2);
        if (!inEdges.size())
        {
            break;
        }
        edge_t edge = inEdges.front();
        rank_type e = comp.subsetSelect(edge);
        rpathUpper.push_front(e);
        rpathLower.push_front(e);
        nn = comp.from(edge);
    }

    // Extend upper path forward.
    nn = n;
    for (;;)
    {
        comp.fetchOutEdges(nn, outEdges);
        if (!outEdges.size())
        {
            break;
        }
        edge_t edge = outEdges.front();
        rpathUpper.push_back(comp.subsetSelect(edge));
        nn = comp.to(edge);
    }

    // Extend lower path forward.
    nn = n;
    for (;;)
    {
        comp.fetchOutEdges(nn, outEdges);
        if (!outEdges.size())
        {
            break;
        }
        edge_t edge = outEdges.back();
        rpathLower.push_back(comp.subsetSelect(edge));
        nn = comp.to(edge);
    }

    const uint64_t K = mGraph.K();
    const uint64_t minLength = mMinLength < K ? 0 : mMinLength - K;
    if (rpathUpper.size() >= minLength)
    {
        mTranscripts.push_back(
            new Transcript(rpathUpper.begin(), rpathUpper.end())
        );
    }
    if (rpathLower.size() >= minLength)
    {
        mTranscripts.push_back(
            new Transcript(rpathLower.begin(), rpathLower.end())
        );
    }
}


void
ResolveTranscripts::Impl::makeTranscriptFromPath(
    ptr_deque<Transcript>& pTranscripts, const vector<rank_type>& pPath)
{
    const uint64_t K = mGraph.K();
    const uint64_t minLength = mMinLength < K ? 1 : mMinLength - K;
    if (pPath.size() >= minLength)
    {
        pTranscripts.push_back(
            new Transcript(pPath.begin(), pPath.end())
        );
#ifdef DEBUG_EXTRACTION
        std::cerr << "Extracting transcript\n";
#endif
#ifdef VERBOSE_DEBUG_EXTRACTION
        SmallBaseVector vec;
        pTranscripts.back().seq(mGraph, vec);
        vec.print(cerr, 72);
#endif
    }
}


void
ResolveTranscripts::Impl::makeTranscriptsFromPathBundle(
    ptr_deque<Transcript>& pTranscripts, const PathBundle& pBundle)
{
    for (auto& path: pBundle.mPaths)
    {
        makeTranscriptFromPath(pTranscripts, path);
    }
}


uint64_t
ResolveTranscripts::Impl::trimPathBundle(PathBundle& pBundle,
    ptr_deque<Transcript>& pTranscripts)
{
    // Need to cull paths which reach this point.

    uint64_t numPrevPaths = pBundle.mPaths.size();

    vector<PathSuppEntry> pathSupport(numPrevPaths);
    for (uint64_t i = 0; i < numPrevPaths; ++i)
    {
        pathSupport[i].mSupport = 0;
        pathSupport[i].mLength = pBundle.mPaths[i].size();
        pathSupport[i].mPath = i;
    }

    uint64_t totalReadSupp = 0;
    for (auto& rs: pBundle.mReadSupport)
    {
        uint64_t thisReadSupp = mVReads[rs.mRead].mCount;
        pathSupport[rs.mPath].mSupport += thisReadSupp;
        totalReadSupp += thisReadSupp;
    }
    uint64_t minReadSupport
        = std::max<uint64_t>(sMinReadSupportThresh,
                             sMinReadSupportRel * totalReadSupp);

    // Sort PathSuppEntry by decreasing order of goodness.
    std::sort(pathSupport.begin(), pathSupport.end());

    boost::dynamic_bitset<uint64_t> pathsToKeep(numPrevPaths);
    uint64_t numPathsToKeep = 0;
    for (uint64_t i = 0; i < pathSupport.size(); ++i)
    {
        // Keep this path if we don't have too many paths already,
        // and this path has support of at least minReadSupport.
        if (numPathsToKeep < sMaxPathsPerNode
            && pathSupport[i].mSupport >= minReadSupport)
        {
            pathsToKeep[pathSupport[i].mPath] = true;
            ++numPathsToKeep;
        }
    }

    PathBundle newBundle;
    newBundle.mPaths.resize(numPathsToKeep);
    for (uint64_t i = 0, j = 0; i < numPrevPaths; ++i)
    {
        if (pathsToKeep[i])
        {
            BOOST_ASSERT(j < numPathsToKeep);
            std::swap(newBundle.mPaths[j], pBundle.mPaths[i]);
            pathSupport[i].mPath = j++;
        }
#if 0
        // Uncomment this to report low-quality paths which have been culled.
        else
        {
            makeTranscriptFromPath(pTranscripts, pBundle.mPaths[i]);
        }
#endif
    }
    for (auto& rs: pBundle.mReadSupport)
    {
        if (pathsToKeep[rs.mPath])
        {
            Gossamer::ensureCapacity(newBundle.mReadSupport);
            newBundle.mReadSupport.push_back(
                ReadSuppEntry(rs.mRead, rs.mPos, pathSupport[rs.mPath].mPath)
            );
        }
    }
    if (pBundle.hasSingletonPath() && pathsToKeep[pBundle.mSingletonPath])
    {
        newBundle.mSingletonPath = pathSupport[pBundle.mSingletonPath].mPath;
    }
    std::swap(newBundle, pBundle);
    return pBundle.mPaths.size();
}


void
ResolveTranscripts::Impl::extractTranscriptsComplex(
        const vector<node_t>& pComponent)
{
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges, outEdgesU;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);
    outEdgesU.reserve(sMaxEdgesPerNode);
    vector<rank_type> outEdgesMapped, outEdgesMappedU;
    outEdgesMapped.reserve(sMaxEdgesPerNode);
    outEdgesMappedU.reserve(sMaxEdgesPerNode);
    vector<node_t> toNodes;
    toNodes.reserve(sMaxEdgesPerNode);
    vector<PathsReachingNode> toPaths;
    toPaths.reserve(sMaxEdgesPerNode);

    dynamic_bitset<uint64_t> interestingNodes(comp.nodeCount());
    dynamic_bitset<uint64_t> seenNodes(comp.nodeCount());
    dynamic_bitset<uint64_t> queuedNodes(comp.nodeCount());

    deque<node_t> q;

    typedef unordered_map<node_t, PathsReachingNode> paths_t;
    paths_t paths;

    // Add edges from the start/end nodes.
    for (node_t v: pComponent)
    {
        comp.fetchInEdges(v, inEdges);
        comp.fetchOutEdges(v, outEdges);

        if (inEdges.empty())
        {
#ifdef DEBUG_EXTRACTION
            std::cerr << "Vertex " << comp.nodeRank(v) << " is a source node\n";
#endif
            q.push_back(v);
            queuedNodes[comp.nodeRank(v)] = true;

            // Seed the graph with paths which start here.
            PathsReachingNode pathsReachingV;

            pathsReachingV.mBundles.resize(outEdges.size());
            for (uint64_t i = 0; i < outEdges.size(); ++i)
            { 
                rank_type outEdge = comp.subsetSelect(outEdges[i]);
                PathBundle& bundle = pathsReachingV.mBundles[i];

                uint64_t path = bundle.ensureSingletonPath(outEdge);
                pair<read_kmer_index_t::const_iterator,
                     read_kmer_index_t::const_iterator>
                    readsRng = mReadKmerIndex.equal_range(outEdge);
                for (read_kmer_index_t::const_iterator ii = readsRng.first;
                     ii != readsRng.second; ++ii)
                {
                    Gossamer::ensureCapacity(bundle.mReadSupport);
                    bundle.mReadSupport.push_back(
                        ReadSuppEntry(ii->second, 0, path)
                    );

#ifdef VERBOSE_DEBUG_EXTRACTION
                   std::cerr << "Read " << ii->second
                       << " starts at source node edge " << outEdge << '\n';
                   {
                       const VerifiedRead& r = mVReads[ii->second];
                       SmallBaseVector sbv;
                       vector<rank_type> es(&r.mEdges[0], &r.mEdges[1]);
                       seqPath(mGraph, es, sbv);
                       sbv.print(cerr);
                   }
#endif
               }
            }
            paths[v].swap(pathsReachingV);
        }
        if (inEdges.size() != 1 || outEdges.size() != 1)
        {
            interestingNodes[comp.nodeRank(v)] = true;
        }
    }

    ptr_deque<Transcript> newTranscripts;

    uint64_t workDone = 0;

    // Find the paths which reach v.

    while (!q.empty())
    {
        node_t v = q.front();
        q.pop_front();
        queuedNodes[comp.nodeRank(v)] = false;
        if (seenNodes[comp.nodeRank(v)])
        {
            continue;
        }

#ifdef DEBUG_EXTRACTION
        std::cerr << "At vertex " << comp.nodeRank(v) << '\n';
#endif

        PathsReachingNode pathsReachingV;

        {
            paths_t::iterator pathsIt = paths.find(v);

            if (pathsIt != paths.end())
            {
#ifdef DEBUG_EXTRACTION
                std::cerr << "Some paths reach here.\n";
                for (auto& bundle: pathsIt->second.mBundles)
                {
                    std::cerr << "    " << bundle.mPaths.size() << '\n';
                }
#endif
                // Capture intermediate paths.
                std::swap(pathsReachingV, pathsIt->second);
                paths.erase(pathsIt);
            }
        }

        bool linearPath = false;

        // This loop is over linear paths, so that we don't hit the
        // priority queue when it isn't needed.  At the entry to this
        // loop, here's what needs to be set up correctly:
        //
        //     - v is the node of interest.
        //     - pathsReachingV are the paths which have been traced as
        //       far as v.
        //     - The pathsReachingV have been removed from paths.
        //
        // If these assumptions ever change, fix this comment.

        do
        {
            seenNodes[comp.nodeRank(v)] = true;
            if (++workDone > pComponent.size() + 200)
            {
                // If this ever happens, it is almost certainly a bug.

                mLog(error, "Possible infinite loop detected in path tracing.  "
                    "Abandoning this subcomponent.");
                mLog(error, string("(") + lexical_cast<string>(workDone) +
                    + " vertices processed in a component of size "
                    + lexical_cast<string>(pComponent.size()) + ")");
#ifdef DUMP_GRAPH_ON_POTENTIAL_INFINITE_LOOP
                dumpComponentByVert(pComponent, cerr);
                cerr << "# BEGIN LEFT READS\n";
                for (uint64_t i = 0; i < mUnmappedPairs.size(); ++i)
                {
                    cerr << ">r" << i << "/1\n";
                    mUnmappedPairs[i].first.print(cerr);
                }
                cerr << "# END LEFT READS\n";
                cerr << "# BEGIN RIGHT READS\n";
                for (uint64_t i = 0; i < mUnmappedPairs.size(); ++i)
                {
                    cerr << ">r" << i << "/2\n";
                    mUnmappedPairs[i].second.print(cerr);
                }
                cerr << "# END RIGHT READS\n";
#endif
                return;
            }
#ifdef DEBUG_EXTRACTION
            std::cerr << "Now at vertex " << comp.nodeRank(v) << '\n';
#endif

            if (interestingNodes[comp.nodeRank(v)])
            {
#ifdef DEBUG_EXTRACTION
                std::cerr << "This is an interesting node, so dumping transcripts.\n";
#endif
                for (auto& bundle: pathsReachingV.mBundles)
                {
                    makeTranscriptsFromPathBundle(newTranscripts, bundle);
                }
            }

            comp.fetchOutEdges(v, outEdges);
#ifdef DEBUG_EXTRACTION
            std::cerr << "Vertex has " << outEdges.size() << " out edges.\n";
#endif

            if (!pathsReachingV.mBundles.size())
            {
                pathsReachingV.mBundles.resize(outEdges.size());
            }

            // Map the edges and find to nodes.
            toNodes.clear();
            outEdgesMapped.clear();

            // This is a linear path if V has only one out edge.
            linearPath = outEdges.size() == 1
                    && !interestingNodes[comp.nodeRank(v)];
            for (edge_t e: outEdges)
            {
                rank_type mappedE = comp.subsetSelect(e);
                node_t u = comp.to(e);
                outEdgesMapped.push_back(mappedE);
                toNodes.push_back(u);
                if (interestingNodes[comp.nodeRank(u)])
                {
                    linearPath = false;
                }
            }

#ifdef DEBUG_EXTRACTION
            if (linearPath)
            {
                std::cerr << "Linear path\n";
            }
#endif

            toPaths.clear();
            toPaths.resize(outEdgesMapped.size());

            for (uint64_t outEdge = 0; outEdge < outEdges.size(); ++outEdge)
            {
                node_t u = toNodes[outEdge];
#ifdef VERBOSE_DEBUG_EXTRACTION
                edge_t e = outEdges[outEdge];
                std::cerr << "Edge " << e << " (" << outEdgesMapped[outEdge] << ") -> "
                          << u << '\n';
#endif
                PathsReachingNode& pathsReachingU = toPaths[outEdge];
                paths_t::iterator oldPathsReachingU = paths.find(u);
                if (oldPathsReachingU != paths.end())
                {
#ifdef VERBOSE_DEBUG_EXTRACTION
                    std::cerr << "Merging with old paths\n";
#endif
                    swap(pathsReachingU, oldPathsReachingU->second);
                }
                else
                {
                    // Otherwise, add any singleton paths which start here.
                    for (uint64_t i = 0; i < outEdgesMappedU.size(); ++i)
                    {
                        rank_type outEdgeU = outEdgesMappedU[i];
                        PathBundle& bundle = pathsReachingU.mBundles[i];
                        if (bundle.hasSingletonPath())
                        {
                            continue;
                        }

                        uint64_t path = bundle.ensureSingletonPath(outEdgeU);
                        pair<read_kmer_index_t::const_iterator,
                             read_kmer_index_t::const_iterator>
                            readsRng = mReadKmerIndex.equal_range(outEdgeU);
                        for (read_kmer_index_t::const_iterator ii = readsRng.first;
                             ii != readsRng.second; ++ii)
                        {
                            Gossamer::ensureCapacity(bundle.mReadSupport);
                            bundle.mReadSupport.push_back(
                                ReadSuppEntry(ii->second, 0, path)
                            );
#ifdef VERBOSE_DEBUG_EXTRACTION
                            std::cerr << "Read " << ii->second
                                << " starts at edge " << outEdgeU << '\n';
                            {
                                const VerifiedRead& r = mVReads[ii->second];
                                SmallBaseVector sbv;
                                vector<rank_type> es(&r.mEdges[0], &r.mEdges[1]);
                                seqPath(mGraph, es, sbv);
                                sbv.print(cerr);
                            }
#endif
                        }
                    }
                }

                PathBundle& prevBundle = pathsReachingV.mBundles[outEdge];
                uint64_t numPrevPaths
                    = trimPathBundle(prevBundle, newTranscripts);

                typedef ArrayMap<rank_type,uint64_t> next_kmer_map_t;
                next_kmer_map_t nextKmer;
                comp.fetchOutEdges(u, outEdgesU);
                pathsReachingU.mBundles.resize(outEdgesU.size());

                // Map graph edges back to the structure of the component.
                for (uint64_t i = 0; i < outEdgesU.size(); ++i)
                {
                    rank_type outUMapped = comp.subsetSelect(outEdgesU[i]);
#ifdef VERBOSE_DEBUG_EXTRACTION
                    std::cerr << "Edge " << i << " is " << outEdgesU[i] << " ("
                        << outUMapped << ")\n";
#endif
                    nextKmer.insert(make_pair(outUMapped, i));
                }

                typedef ArrayMap<uint64_t, uint64_t> fwd_map_t;
                fwd_map_t forwardMap[sMaxEdgesPerNode];

                vector<uint64_t> readSupport(numPrevPaths);
                BOOST_ASSERT(readSupport.size() == numPrevPaths);

                for (auto& rs: prevBundle.mReadSupport)
                {
                    uint64_t readNo = rs.mRead;
                    uint32_t readPos = rs.mPos;
                    uint32_t pathNo = rs.mPath;
                    uint32_t readPosNext = readPos + 1;
#ifdef VERBOSE_DEBUG_EXTRACTION
                    cerr << "Read (" << readNo << ", " << readPos
                        << ") supports the path ";
                    {
                        SmallBaseVector sbv;
                        seqPath(mGraph, prevBundle.mPaths[pathNo], sbv);
                        cerr << sbv;
                    }
                    cerr << '\n';
#endif

                    const VerifiedRead& r = mVReads[readNo];
                    BOOST_ASSERT(r.mEdges[readPos] == outEdgesMapped[outEdge]);

                    if (readPosNext >= r.mEdges.size())
                    {
                        continue;
                    }

                    rank_type nexte = r.mEdges[readPosNext];
                    next_kmer_map_t::iterator ii = nextKmer.find(nexte);
                    if (ii == nextKmer.end())
                    {
                        continue;
                    }
                    uint64_t i = ii->second;

                    BOOST_ASSERT(i < sMaxEdgesPerNode);
                    BOOST_ASSERT(pathNo < numPrevPaths);

                    vector<rank_type>& prevPath = prevBundle.mPaths[pathNo];
                    PathBundle& nextBundle = pathsReachingU.mBundles[i];
                    fwd_map_t& fwdMap = forwardMap[i];
                    fwd_map_t::iterator pathit = fwdMap.find(pathNo);
                    if (pathit == fwdMap.end())
                    {
                        pathit = fwdMap.insert(make_pair(pathNo,
                            nextBundle.mPaths.size())).first;
                        Gossamer::ensureCapacity(nextBundle.mPaths);
                        nextBundle.mPaths.push_back(vector<rank_type>());
                        vector<rank_type>& newPath
                            = nextBundle.mPaths.back();
                        newPath.reserve(prevPath.size() + 1);
                        copy(prevPath.begin(), prevPath.end(),
                            back_inserter(newPath));
                        newPath.push_back(nexte);

                        const vector<rank_type>& p
                            = nextBundle.mPaths[pathit->second];
                        pair<read_kmer_index_t::const_iterator,
                             read_kmer_index_t::const_iterator>
                            readsRng = mReadKmerIndex.equal_range(p.back());
                        for (read_kmer_index_t::const_iterator ii = readsRng.first;
                             ii != readsRng.second; ++ii)
                        {
                            Gossamer::ensureCapacity(nextBundle.mReadSupport);
                            nextBundle.mReadSupport.push_back(
                                ReadSuppEntry(ii->second, 0, pathit->second)
                            );
                        }
                    }

                    Gossamer::ensureCapacity(nextBundle.mReadSupport);
                    nextBundle.mReadSupport.push_back(
                        ReadSuppEntry(readNo, readPosNext, pathit->second)
                    );
                }
            }

            if (linearPath)
            {
                v = toNodes[0];
                std::swap(pathsReachingV, toPaths[0]);
                continue;
            }
            else
            {
                for (uint64_t i = 0; i < toNodes.size(); ++i)
                {
                    node_t u = toNodes[i];

                    swap(paths[u], toPaths[i]);

                    bool addU = true;
                    comp.fetchInEdges(u, inEdges);
                    for (edge_t e: inEdges)
                    {
                        node_t fromNode = comp.from(e);

                        if (!seenNodes[comp.nodeRank(fromNode)])
                        {
                            addU = false;
                            break;
                        }
                    }
                    if (addU)
                    {
                        q.push_front(u);
                        queuedNodes[comp.nodeRank(u)] = true;
                    }
                }
            }
        } while (linearPath);
    }

#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "  Transitive reduction");
#endif

    // Now do transitive reduction on the draft transcripts.  Keep any
    // which are not wholly contained within some other transcript.

    std::sort(newTranscripts.begin(), newTranscripts.end(),
              Transcript::CompareByLength());

    dynamic_bitset<uint64_t> entailed(newTranscripts.size());
    {
        const uint64_t K = mGraph.K();
        const uint64_t minLength = mMinLength < K ? 1 : mMinLength - K;

        typedef std::unordered_multimap<rank_type,uint64_t> init_kmer_map_t;
        init_kmer_map_t initialKmerMap;
        deque<init_kmer_map_t::iterator> entriesToRemove;

        for (uint64_t j = 0; j < newTranscripts.size(); ++j)
        {
            const vector<rank_type>& edgesj = newTranscripts[j].mEdges;

            for (size_t p = 0; p < edgesj.size() - minLength + 1; ++p)
            {
                pair<init_kmer_map_t::iterator,
                     init_kmer_map_t::iterator>
                    transRng = initialKmerMap.equal_range(edgesj[p]);
                for (init_kmer_map_t::iterator ii = transRng.first;
                     ii != transRng.second; ++ii)
                {
                    uint64_t i = ii->second;
                    const vector<rank_type>& edgesi = newTranscripts[i].mEdges;
                    if (edgesj.size() < edgesi.size() + p)
                    {
                        continue;
                    }

                    entailed[i] = true;
                    for (size_t q = edgesi.size() - 1; q > 0; --q)
                    {
                        if (edgesi[q] != edgesj[p+q])
                        {
                            entailed[i] = false;
                            break;
                        }
                    }
                    if (entailed[i])
                    {
                        entriesToRemove.push_back(ii);
                    }
                }
            }

            if (!entriesToRemove.empty())
            {
                for (auto& ii: entriesToRemove)
                {
                    initialKmerMap.erase(ii);
                }
                entriesToRemove.clear();
            }
            initialKmerMap.insert(make_pair(edgesj[0], j));
        }
    }

    // Keep non-entailed transcripts.
    mTranscripts.reserve(mTranscripts.size()
                         + newTranscripts.size()
                         - entailed.count());
    for (uint64_t i = 0; !newTranscripts.empty(); ++i)
    {
        ptr_deque<Transcript>::auto_type trans = newTranscripts.pop_front();
        if (!entailed[i])
        {
            mTranscripts.push_back(trans.release());
        }
    }
}


void
ResolveTranscripts::Impl::quantifyTranscripts()
{
    Component& comp = *mComponent;

    {
        vector<uint64_t> countsInTranscripts(comp.edgeCount());

        // Count kmers in transcripts.
        for (auto& transcript: mTranscripts)
        {
            for (rank_type edge: transcript.mEdges)
            {
                ++countsInTranscripts[comp.edgeRank(comp.subsetRank(edge))];
            }
        }

        // Compute FPKM for transcript
        uint64_t k = mGraph.K();
        for (auto& transcript: mTranscripts)
        {
            double mappedReadFragments = 0;
            for (rank_type edge: transcript.mEdges)
            {
                uint64_t rnk = comp.edgeRank(comp.subsetRank(edge));
                mappedReadFragments
                    += (double)mReadKmerCount[rnk] / countsInTranscripts[rnk];
            }
            uint64_t length = transcript.mEdges.size() + k;

            transcript.mFPKM
                = mappedReadFragments * 1e9 / (length * mMappableReads);
        }
    }
}



void
ResolveTranscripts::Impl::outputTranscripts()
{
    const uint64_t K = mGraph.K();
    const uint64_t minLength = mMinLength < K ? 0 : mMinLength - K;

    for (uint64_t i = 0; i < mTranscripts.size(); ++i)
    {
        const Transcript& transcript = mTranscripts[i];

        SmallBaseVector vec;
        transcript.seq(mGraph, vec);

        if (transcript.mEdges.size() >= minLength)
        {
            mOut << ">" << mName << "--" << i
                << " length=" << vec.size()
                << " ~FPKM=" << transcript.mFPKM
                << '\n';
            vec.print(mOut);
        }
    }
#if 1
    mOut.flush();
#endif
}


void
ResolveTranscripts::Impl::clampExtremelyHighEdgeCounts()
{
    const double sExtremeEdgeFlowFactor = 200;
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    for (uint64_t i = 0; i < comp.edgeCount(); ++i)
    {
        edge_t e = comp.edgeSelect(i);
        node_t from = comp.from(e);
        node_t to = comp.to(e);

        comp.fetchInEdges(from, inEdges);
        comp.fetchOutEdges(to, outEdges);

        uint64_t inFlow = 0;
        for (edge_t& edge: inEdges)
        {
            inFlow += comp.coverage(edge);
        }

        uint64_t outFlow = 0;
        for (edge_t& edge: outEdges)
        {
            outFlow += comp.coverage(edge);
        }

        uint32_t count = comp.coverage(e);
        if (inFlow != 0 && outFlow != 0
            && count > sExtremeEdgeFlowFactor * inFlow
            && count > sExtremeEdgeFlowFactor * outFlow)
        {
            comp.setCoverage(e, max(inFlow, outFlow));
        }
    }
}


bool
ResolveTranscripts::Impl::trimLowCoverageEdges()
{
    const double sFlowThreshold = 0.05;
    const double sEdgeThreshold = 0.05;
    const uint32_t sAbsoluteThreshold = 2;

    bool modified = false;
    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    for (uint64_t i = 0; i < comp.nodeCount(); ++i)
    {
        node_t n = comp.nodeSelect(i);
        comp.fetchInEdges(n, inEdges);
        comp.fetchOutEdges(n, outEdges);

        if (inEdges.empty() || outEdges.empty())
        {
            continue;
        }

        uint64_t inFlow = 0;
        for (auto& edge: inEdges)
        {
            inFlow += comp.coverage(edge);
        }

        uint64_t outFlow = 0;
        for (auto& edge: outEdges)
        {
            outFlow += comp.coverage(edge);
        }

        for (auto& in: inEdges)
        {
            if (comp.scheduledForRemove(in))
            {
                continue;
            }
            uint32_t count = comp.coverage(in);
            if (count < outFlow * sFlowThreshold
                || count < inFlow * sEdgeThreshold
                || count <= sAbsoluteThreshold)
            {
                comp.scheduleForRemove(in);
                modified = true;
            }
        }

        for (auto& out: outEdges)
        {
            if (comp.scheduledForRemove(out))
            {
                continue;
            }
            uint32_t count = comp.coverage(out);
            if (count < inFlow * sFlowThreshold
                || count < outFlow * sEdgeThreshold
                || count <= sAbsoluteThreshold)
            {
                comp.scheduleForRemove(out);
                modified = true;
            }
        }
    }

    if (modified)
    {
        commitEdgeRemove();
    }

    return modified;
}


void
ResolveTranscripts::Impl::cullComponents()
{
    bool changed = false;
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "  Before size: " + lexical_cast<string>(mComponent->edgeCount()));
#endif
    for (auto& component: componentsByEdge())
    {
        if (component.size() < mMinRhomers)
        {
            for (auto& e: component)
            {
                mComponent->scheduleForRemove(e);
                changed = true;
            }
        }
    }
    if (changed)
    {
        commitEdgeRemove();
    }
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "  After size: " + lexical_cast<string>(mComponent->edgeCount()));
#endif
}


void
ResolveTranscripts::Impl::breakCycles()
{
    bool changed = false;

    {
#ifdef MORE_INTERNAL_PROGRESS
        mLog(info, "  Trivial self-cycles");
#endif
        vector<edge_t> inEdges;
        vector<edge_t> outEdges;
        inEdges.reserve(sMaxEdgesPerNode);
        outEdges.reserve(sMaxEdgesPerNode);

        Component& comp = *mComponent;

        for (uint64_t i = 0; i < comp.nodeCount(); ++i)
        {
            node_t n = comp.nodeSelect(i);
            comp.fetchOutEdges(n, outEdges);
            for (auto& e: outEdges)
            {
                if (n == comp.to(e))
                {
                    comp.scheduleForRemove(e);
                    changed = true;
                }
            }
        }

        if (changed)
        {
            commitEdgeRemove();
        }
    }

    bool doCycles = true;

    uint64_t pass = 1;
    do
    {
        changed = false;
#ifdef MORE_INTERNAL_PROGRESS
        mLog(info, "  Pass " + lexical_cast<string>(pass));
#endif

        if (pass > 5)
        {
            mLog(info, "  Cycle breaking in large subcomponent: "
               + lexical_cast<string>(pass) + " passes");
        }

        bool invariantsBroken = false;

        for (auto& component: sccs())
        {
            if (component.size() <= 1)
            {
                // SCCs of size 1 are a very common case, thanks to
                // long linear paths. Self-loops have already been
                // removed, so no work to be done here.
                continue;
            }

            // If this is the first pass, or if invariants were
            // broken, check for circles.
            if (doCycles && breakCircularComponent(component))
            {
                changed = true;
                continue;
            }

#ifdef MORE_INTERNAL_PROGRESS
            mLog(info, "  SCC of size " + lexical_cast<string>(component.size()));
#endif

            BreakCyclesContext ctx(component);

            bool thisChanged = breakCyclesComponent(ctx);
#ifdef MORE_INTERNAL_PROGRESS
            if (thisChanged)
            {
                mLog(info, "    ...which changed");
            }
#endif
            changed |= thisChanged;
            invariantsBroken |= ctx.mInvariantsPotentiallyBroken;
        }

        if (changed)
        {
            commitEdgeRemove();
        }
        doCycles = invariantsBroken;
        ++pass;
    } while (changed);
}


bool
ResolveTranscripts::Impl::breakCircularComponent(
        const vector<node_t>& pComponent)
{
    vector<edge_t> outEdges;
    outEdges.reserve(sMaxEdgesPerNode);

    edge_t minEdge;
    uint32_t minEdgeCov = ~(uint32_t)0;

    Component& comp = *mComponent;

    for (auto& v: pComponent)
    {
        comp.fetchOutEdges(v, outEdges);
        if (outEdges.size() != 1)
        {
            return false;
        }

        if (comp.countInEdges(v) != 1)
        {
            return false;
        }

        edge_t e = outEdges.front();
        uint32_t cov = comp.coverage(e);
        if (minEdgeCov > cov)
        {
            minEdge = e;
            minEdgeCov = cov;
        }
    }

    BOOST_ASSERT(minEdgeCov < ~(uint32_t)0);
    comp.scheduleForRemove(minEdge);
    return true;
}


bool
ResolveTranscripts::Impl::breakCyclesComponent(BreakCyclesContext& pCtx)
{
    const uint64_t sSmallComponent = 2000;

    Component& comp = *mComponent;

#ifdef MORE_INTERNAL_PROGRESS
    std::cerr << "Processing component of size " << pCtx.mComponent.size() << '\n';
#endif
    // Gather statistics.
    uint64_t linearPaths = 0;
    std::unordered_set<edge_t> edges;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    dynamic_bitset<uint64_t> componentAsSet(comp.nodeCount());
    for (auto& v: pCtx.mComponent)
    {
        componentAsSet[comp.nodeRank(v)] = true;
    }

    deque<node_t> joinNodes;
    for (auto& v: pCtx.mComponent)
    {
        comp.fetchInEdges(v, inEdges);
        comp.fetchOutEdges(v, outEdges);

        for (auto& e: inEdges)
        {
            edges.insert(e);
        }

        for (auto& e: outEdges)
        {
            edges.insert(e);
        }

        if (inEdges.size() != 1 || outEdges.size() != 1)
        {
            joinNodes.push_back(v);
            for (auto& e: outEdges)
            {
                node_t toNode = comp.to(e);
                if (componentAsSet[comp.nodeRank(toNode)])
                {
                    ++linearPaths;
                }
            }
        }
    }
#ifdef MORE_INTERNAL_PROGRESS
    std::cerr << joinNodes.size() << " join points.\n";
    std::cerr << linearPaths << " linear paths.\n";
    std::cerr.flush();
#endif

    if (joinNodes.size() == 1)
    {
        // Simple cycle.
#ifdef MORE_INTERNAL_PROGRESS
        std::cerr << "Simple cycle\n";
#endif

        node_t node = joinNodes.front();
        comp.fetchInEdges(node, inEdges);
        comp.fetchOutEdges(node, outEdges);

        if (inEdges.size() == 1)
        {
            comp.scheduleForRemove(inEdges.front());
            return true;
        }
        else if (outEdges.size() == 1)
        {
            comp.scheduleForRemove(outEdges.front());
            return true;
        }

        // Still a simple cycle, but there are multiple edges both in and
        // out of this node.  Pick the one with the lowest coverage which
        // will break the cycle.

        edge_t minEdge;
        uint32_t minEdgeCov = ~(uint32_t)0;

        for (auto& e: inEdges)
        {
            uint32_t cov = comp.coverage(e);
            node_t n = comp.from(e);
            if (componentAsSet[comp.nodeRank(n)] && cov < minEdgeCov)
            {
                minEdge = e;
                minEdgeCov = cov;
            }
        }

        for (auto& e: outEdges)
        {
            uint32_t cov = comp.coverage(e);
            node_t n = comp.to(e);
            if (componentAsSet[comp.nodeRank(n)] && cov < minEdgeCov)
            {
                minEdge = e;
                minEdgeCov = cov;
            }
        }

        BOOST_ASSERT(minEdgeCov < ~(uint32_t)0);
        comp.scheduleForRemove(minEdge);
        return true;
    }

    if (joinNodes.size() < sSmallComponent || linearPaths < sSmallComponent)
    {
        // The subcomponent is small enough that we can use the expensive
        // algorithm.
#ifdef MORE_INTERNAL_PROGRESS
        std::cerr << "Complex but tractable subcomponent\n";
#endif
        return breakCyclesSubcomponent(pCtx);
    }

    // Potential tangle.

#ifdef MORE_INTERNAL_PROGRESS
    std::cerr << "Potentially intractable subcomponent\n";
    std::cerr.flush();
#endif

    // Find all edges with lowest coverage which are wholly contained
    // within this SCC and remove them.

    uint32_t bestcov = ~(uint32_t)0;

    dynamic_bitset<uint64_t> edgesToRemove(comp.edgeCount());

    for (auto& v: joinNodes)
    {
        comp.fetchOutEdges(v, outEdges);
        for (auto& e: outEdges)
        {
            node_t w = comp.to(e);
            if (componentAsSet[comp.nodeRank(w)])
            {
                uint32_t ecov = comp.coverage(e);
                if (ecov < bestcov)
                {
                    edgesToRemove.clear();
                    bestcov = ecov;
                }
                if (ecov == bestcov)
                {
                    edgesToRemove[comp.edgeRank(e)] = true;
                }
            }
        }
    }

#ifdef MORE_INTERNAL_PROGRESS
    std::cerr << "Removing " << edgesToRemove.size() << " edge(s) with coverage " << bestcov << '\n';
    std::cerr.flush();
#endif
    comp.scheduleForRemove(edgesToRemove);

    // This strategy may have broken some of the invariants expected
    // of other cycle-breaking strategies.
    pCtx.mInvariantsPotentiallyBroken = true;

    return true; // Graph changed.
}


bool
ResolveTranscripts::Impl::breakCyclesSubcomponent(BreakCyclesContext& pCtx)
{
    bool changed = false;

    ShortestPaths shortestPaths(*this, pCtx.mComponent);

    typedef std::set< std::set<edge_t> > loops_t;
    loops_t loops;
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "  breakCyclesSubcomponent");
    mLog(info, "    Component size = " + lexical_cast<string>(pCtx.mComponent.size()));
#endif

    Component& comp = *mComponent;

    vector<edge_t> inEdges, outEdges;
    inEdges.reserve(sMaxEdgesPerNode);
    outEdges.reserve(sMaxEdgesPerNode);

    uint64_t joinPoints = 0;
    for (auto& v: pCtx.mComponent)
    {
        if (comp.countInEdges(v) <= 1)
        {
            continue;
        }
        ++joinPoints;

        comp.fetchOutEdges(v, outEdges);
        for (auto& edge: outEdges)
        {
            node_t v2 = comp.to(edge);
            if (!shortestPaths.distance(v2, v))
            {
                // No path from v2 to v.
                continue;
            }
            // cerr << "Path found between " << v2 << " and " << v << '\n';

            std::set<edge_t> loopPath;
            shortestPaths.shortestPath(v2, v, loopPath);
            loops.insert(loopPath);
        }
    }
#ifdef MORE_INTERNAL_PROGRESS
    mLog(info, "    " + lexical_cast<string>(joinPoints) + " join points");
    mLog(info, "    " + lexical_cast<string>(loops.size()) + " loops");
#endif

    if (loops.empty())
    {
        return changed;
    }

    typedef std::unordered_map<edge_t,uint64_t> num_loops_t;
    num_loops_t numLoops;

    for (auto& loopPath: loops)
    {
        for (auto& e: loopPath)
        {
            ++numLoops[e];
        }
    }

    // Break simple loops first. Or we would, but it's disabled for now.
    vector< loops_t::iterator > loopsToRemove;
    loopsToRemove.reserve(loops.size());

    if (loops.empty())
    {
        return changed;
    }

#ifdef DEBUG
    mLog(info, lexical_cast<string>(loops.size()) + " complex loops remaining");
#endif

    // Visit edges in descending order of number of participating loops.
    typedef deque< pair<uint64_t,edge_t> > pq_t;
    pq_t pq;
    for (auto& nl: numLoops)
    {
        pq.push_back(make_pair(nl.second, nl.first));
    }
    sort(pq.begin(), pq.end());

    while (!loops.empty() && !pq.empty())
    {
        edge_t e = pq.back().second;
        pq.pop_back();

        loopsToRemove.clear();

        comp.scheduleForRemove(e);

        for (loops_t::iterator ii = loops.begin(); ii != loops.end(); ++ii)
        {
            bool loopRelevant = false;

            for (auto& edge: *ii)
            {
                if (edge == e)
                {
                    loopRelevant = true;
                    break;
                }
            }
            if (!loopRelevant)
            {
                continue;
            }
            changed = true;

            loopsToRemove.push_back(ii);

            for (auto& edge: *ii)
            {
                --numLoops[edge];
            }
        }

        for (auto it: loopsToRemove)
        {
            loops.erase(it);
        }
        loopsToRemove.clear();

        pq_t pqtemp;
        pqtemp.swap(pq);
        for (auto& ce: pqtemp)
        {
            uint64_t nl = numLoops[ce.second];
            if (nl > 0)
            {
                pq.push_back(make_pair(nl, ce.second));
            }
        }
        sort(pq.begin(), pq.end());
    }

    return changed;
}


void
ResolveTranscripts::Impl::dumpGraph(ostream& pOut)
{
    const Component& comp = *mComponent;
    pOut << "digraph G {\n";
    for (uint64_t i = 0; i < comp.nodeCount(); ++i)
    {
        SmallBaseVector vec;
        mGraph.seq(comp.underlyingNode(comp.nodeSelect(i)), vec);
        pOut << "  " << vec << " [label=\"" << i << "\"];\n";
    }
    for (uint64_t i = 0; i < comp.edgeCount(); ++i)
    {
        Graph::Edge e(comp.underlyingEdge(comp.edgeSelect(i)));
        SmallBaseVector nf, nt;
        mGraph.seq(mGraph.from(e), nf);
        mGraph.seq(mGraph.to(e), nt);
        pOut << "  " << nf << " -> " << nt << '\n';
    }
    pOut << "}\n";
    pOut.flush();
}


void
ResolveTranscripts::Impl::addContig(const SmallBaseVector& pVec)
{
    mContigs.push_back(pVec);
    dynamic_bitset<uint64_t>& contigEdges = mBuild->mContigEdges;

    uint64_t rho = mGraph.K() + 1;
    uint64_t upper = pVec.size() - rho;
    for (uint64_t i = 0; i < upper; ++i)
    {
        Graph::Edge e(pVec.kmer(rho, i));
        uint64_t rnk = mGraph.rank(e);
        contigEdges[rnk] = true;
    }
}


void
ResolveTranscripts::Impl::constructGraph()
{
    uint64_t numEdges = mBuild->mGraphEdges.count();
#ifdef DEBUG
    mLog(info, " graph edges = " + lexical_cast<string>(numEdges));
    mLog(info, " numContigs = " + lexical_cast<string>(mContigs.size()));
    mLog(info, " numReads = " + lexical_cast<string>(mReads.size()));
#endif

    vector<uint32_t> coverage;
    coverage.reserve(numEdges);

    {
        Build& build = *mBuild;
        subset_t::Builder builder(sComponentKmersName, mStringFac);

        // cerr << "Edges to add: " << edges.size() << '\n';
        for (rank_type i = 0; i < mGraph.count(); ++i)
        {
            if (build.mGraphEdges[i])
            {
                const uint64_t* cov = build.mReadCoverage.find(i);
                if (cov)
                {
                    builder.push_back(i);
                    coverage.push_back(*cov);
                }
            }
        }
        builder.end(mGraph.count());
    }

    mBuild = std::shared_ptr<Build>();
    mComponent = std::make_shared<Component>(std::ref(mGraph), std::ref(mStringFac), coverage.begin(), coverage.end());
}


void
ResolveTranscripts::Impl::processComponent()
{
    mLog(info, "Processing component " + mName);

    if (mReads.size() < sMinReads || mBuild->mGraphEdges.count() < mMinRhomers)
    {
        return;
    }

    if (mReads.size() > ~uint32_t(0))
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Component has more than "
                + lexical_cast<string>(~uint32_t(0)) + "reads"));
    }

    constructGraph();

#ifdef DUMP_INTERMEDIATES
    dumpGraphByComponents("begin", mOut);
#endif

#ifdef INTERNAL_PROGRESS
    mLog(info, "Clamping high edge counts...");
#endif
    clampExtremelyHighEdgeCounts();
#ifdef DUMP_INTERMEDIATES
    dumpGraphByComponents("clamp", mOut);
#endif

#ifdef INTERNAL_PROGRESS
    mLog(info, "Trimming low coverage edges...");
#endif
    trimLowCoverageEdges();
#ifdef DUMP_INTERMEDIATES
    dumpGraphByComponents("trim", mOut);
#endif

#ifdef INTERNAL_PROGRESS
    mLog(info, "Culling small subcomponents...");
#endif
    cullComponents();
#ifdef DUMP_INTERMEDIATES
    dumpGraphByComponents("cull", mOut);
#endif

    // That may have left no components left. If so, we're done.
    if (mComponent->empty())
    {
#ifdef INTERNAL_PROGRESS
        mLog(info, "All subcomponents culled.");
#endif
        return;
    }

#ifdef INTERNAL_PROGRESS
    mLog(info, "Breaking cycles...");
#endif
    breakCycles();
#ifdef DUMP_INTERMEDIATES
    dumpGraphByComponents("cycles", mOut);
#endif

#ifdef INTERNAL_PROGRESS
    mLog(info, "Verifying reads...");
#endif
    verifyReads();

#ifdef INTERNAL_PROGRESS
    mLog(info, "Extracting transcripts...");
#endif
    extractTranscripts();

#ifdef INTERNAL_PROGRESS
    mLog(info, "Quantifying transcripts...");
#endif
    quantifyTranscripts();

#ifdef INTERNAL_PROGRESS
    mLog(info, "Outputting transcripts...");
#endif
    outputTranscripts();

#ifdef INTERNAL_PROGRESS
    mLog(info, "Done.");
#endif
}


ResolveTranscripts::ResolveTranscripts(const string& pName,
            Graph& pGraph, Logger& pLog, std::ostream& pOut,
            uint64_t pMinLength, uint64_t pBasesInReads)
    : mPImpl(new Impl(pName, pGraph, pLog, pOut, pMinLength, pBasesInReads))
{
}


ResolveTranscripts::~ResolveTranscripts()
{
}


void
ResolveTranscripts::addContig(const SmallBaseVector& pVec)
{
    mPImpl->addContig(pVec);
}


void
ResolveTranscripts::addReadPair(const ReadInfo& pLhs, const ReadInfo& pRhs)
{
    SmallBaseVector lhsRc;
    pLhs.mRead.reverseComplement(lhsRc);

    SmallBaseVector rhsRc;
    pRhs.mRead.reverseComplement(rhsRc);

    uint64_t lhsFwHits = mPImpl->readMaps(pLhs.mRead);
    uint64_t rhsFwHits = mPImpl->readMaps(pRhs.mRead);
    uint64_t lhsRcHits = mPImpl->readMaps(lhsRc);
    uint64_t rhsRcHits = mPImpl->readMaps(rhsRc);

    if ((lhsFwHits == 0 && lhsRcHits == 0)
        || (rhsFwHits == 0 && rhsRcHits == 0))
    {
        return;
    }

    uint64_t lfrr = lhsFwHits + rhsRcHits;
    uint64_t lrrf = lhsRcHits + rhsFwHits;
#ifdef RETAIN_UNMAPPED_READS
    mPImpl->mUnmappedPairs.push_back(make_pair(pLhs.mRead, pRhs.mRead));
#endif

    if (lfrr >= lrrf)
    {
        mPImpl->addRead(pLhs.mRead);
        mPImpl->addRead(rhsRc);
    }
    else
    {
        mPImpl->addRead(pRhs.mRead);
        mPImpl->addRead(lhsRc);
    }
}


void
ResolveTranscripts::processComponent()
{
    mPImpl->processComponent();
}


