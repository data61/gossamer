// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SUPERGRAPH_HH
#define SUPERGRAPH_HH

#ifndef ENTRYEDGESET_HH
#include "EntryEdgeSet.hh"
#endif

#ifndef SUPERPATH_HH
#include "SuperPath.hh"
#endif

#ifndef STD_UNORDERED_MAP
#include <unordered_map>
#define STD_UNORDERED_MAP
#endif

#ifndef STD_SET
#include <set>
#define STD_SET
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class SuperGraph
{

    friend class PathIterator;

public:
    static constexpr uint64_t version = 2011082301ULL;
    // Version history
    // 2011062101   - introduce version tracking
    // 2011082301   - simplified structures and API

    struct Header
    {
        uint64_t version;
    };

    typedef EntryEdgeSet::Edge Edge;
    typedef EntryEdgeSet::Node Node;
    typedef uint64_t ReadNum;

    typedef std::vector<SuperPathId> SuperPathIds;

    static const uint64_t invalidSuperPathId = -1ULL;

    class NodeIterator
    {
    public:
        bool valid() const
        {
            return mCurr != mEnd;
        }

        Node operator*() const
        {
            return mCurr->first;
        }

        void operator++()
        {
            ++mCurr;
        }

        NodeIterator(const std::unordered_map<Node,SuperPathIds>::const_iterator& pCurr,
                     const std::unordered_map<Node,SuperPathIds>::const_iterator& pEnd)
            : mCurr(pCurr), mEnd(pEnd)
        {
        }

    private:
        std::unordered_map<Node,SuperPathIds>::const_iterator mCurr;
        std::unordered_map<Node,SuperPathIds>::const_iterator mEnd;
    };

    class PathIterator
    {
    public:
        bool valid() const
        {
            return mCurr != mEnd;
        }

        SuperPathId operator*() const
        {
            return *mCurr;
        }

        void operator++()
        {
            ++mCurr;
            next();
        }

        PathIterator(const SuperGraph& pSuperGraph)
            : mSG(pSuperGraph), mNodes(pSuperGraph.mSucc.begin(), pSuperGraph.mSucc.end()),
              mIds(), mCurr(mIds.begin()), mEnd(mIds.end())
        {
            next();
        }

    private:
        void next()
        {
            while (mCurr == mEnd && mNodes.valid())
            {
                mSG.successors(*mNodes, mIds);
                mCurr = mIds.begin();
                mEnd = mIds.end();
                ++mNodes;
            }
        }

        const SuperGraph& mSG;
        NodeIterator mNodes;
        SuperPathIds mIds;
        SuperPathIds::const_iterator mCurr;
        SuperPathIds::const_iterator mEnd;
    };


    /**
     * A path direction and distance - relative to some node and sink, respectively.
     * The edge is the successor along the shortest path towards the sink.
     */
    struct PathDir
    {
        PathDir(SuperPathId pEdge, uint32_t pDist) 
            : mEdge(pEdge), mDist(pDist)
        {
        }

        SuperPathId mEdge;
        uint32_t mDist;
    };

    /**
     * An iterator which yields all of the paths between two nodes in order of 
     * non-decreasing length, up to some maximum.
     */
    class ShortestPathIterator
    {
        // Path defined by a sequence of deviations from the shortest path.
        struct DevPath
        {
            // We define this in the opposite sense, for use by std::priority_queue, 
            // which implements a max heap (when we actually want a min heap.)
            bool operator<(const DevPath& pOther) const
            {
                return mLength >= pOther.mLength;
            }

            /**
             * Construct the shortest path in the graph.
             */
            DevPath(uint32_t pLength)
                : mLength(pLength), mDevs()
            {
            }

            uint32_t mLength;
            std::vector<SuperPathId> mDevs;
        };

    public:

        bool valid() const
        {
            return mValid;
        }

        void operator++()
        {
            next(mCurPath);
        }

        const std::vector<SuperPathId>& operator*() const
        {
            return mCurPath;
        }

        ShortestPathIterator(SuperGraph& pSGraph,
                             const Node& pSource, const Node& pSink, 
                             uint64_t pMaxLength, uint64_t pSearchRadius);

    private:
    
        /**
         * Sets pPath to the next shortest deviation path, if there is one.
         * Returns true if there was a path, false otherwise.
         */
        bool next(std::vector<SuperPathId>& pPath);

        Node mSource;
        Node mSink;
        uint64_t mMaxLength;
        SuperGraph& mSGraph;
        std::priority_queue<DevPath> mPaths;
        bool mValid;
        std::vector<SuperPathId> mCurPath;
        std::map<Node, PathDir> mNodeShortestPaths;
    };


    /**
     * Retrieve the SuperPath corresponding to the given Id.
     */
    SuperPath operator[](const SuperPathId& pId) const
    {
        return SuperPath(*this, pId, mSegs[pId.value()], SuperPathId(mRCs[pId.value()]));
    }

    /**
     * Returns the reverse complement of the given path.
     */
    SuperPathId reverseComplement(const SuperPathId& pId) const;

    /**
     * Populate pNodes with all the nodes of the supergraph.
     */
    void nodes(std::vector<Node>& pNodes) const;

    /**
     * Clear, then populate pSucc with the SuperPathIds for the successors of the
     * given node.
     */
    void successors(const Node& pNode, SuperPathIds& pSucc) const;

    /**
     * Returns the number of incoming superpaths for the given node.
     */
    uint64_t numIn(const Node& pNode) const
    {
        Node rc(mEntries.reverseComplement(pNode));
        return numOut(rc);
    }

    /**
     * Returns the number of outgoing superpaths for the given node.
     */
    uint64_t numOut(const Node& pNode) const
    {
        std::unordered_map<Node, SuperPathIds>::const_iterator i(mSucc.find(pNode));
        if (i != mSucc.end())
        {
            return i->second.size();
        }
        return 0;
    }

    /**
     * Assuming the given node only has one outgoing superpath, returns its id.
     */
    SuperPathId onlyOut(const Node& pNode) const
    {
        std::unordered_map<Node, SuperPathIds>::const_iterator i(mSucc.find(pNode));
        BOOST_ASSERT(i != mSucc.end());
        BOOST_ASSERT(i->second.size() == 1);
        return i->second.front();
    }

    /**
     * Creates a new SuperPath consisting of the given SuperPaths, in order,
     * and its reverse complement.
     */
    std::pair<SuperPathId, SuperPathId> link(const std::vector<SuperPathId>& pPaths);

    /**
     * Returns the id of a superpath consisting of a gap of the given length.
     */
    SuperPathId gapPath(int64_t pLen);

    /**
     * Erase a superpath and its reverse complement.
     */
    void erase(const SuperPathId& pId);

    /**
     * Return the EntryEdgeSet.
     */
    const EntryEdgeSet& entries() const
    {
        return mEntries;
    }

    /**
     * K of the underlying EntryEdgeSet/de Bruijn graph.
     */
    uint64_t K() const
    {
        return mEntries.K();
    }

    /**
     * Returns the length of the SuperPath (in edges.)
     */ 
    uint64_t size(const SuperPath& pPath) const
    {
        return pPath.size(mEntries);
    }

    /**
     * Returns the length of the SuperPath (in edges.)
     */ 
    uint64_t size(const SuperPathId& pId) const
    {
        return size((*this)[pId]);
    }

    /**
     * Return the length of the superpath in bases.
     */ 
    int64_t baseSize(const SuperPathId& pId) const
    {
        return ((*this)[pId]).baseSize(mEntries);
    }

    /**
     * Returns the start node of the SuperPath.
     */
     Node start(const SuperPath& pPath) const
     {
        return pPath.start(mEntries);
     }

     Node start(const SuperPathId& pId) const
     {
        SuperPath p((*this)[pId]);
        return start(p);
     }

    /**
     * Returns the end node of the SuperPath.
     */ 
    Node end(const SuperPath& pPath) const
    {
        return pPath.end(mEntries);
    }

    Node end(const SuperPathId& pId) const
    {
        SuperPath p((*this)[pId]);
        return end(p);
    }

    Edge firstEdge(const SuperPathId& pId) const
    {
        SuperPath p((*this)[pId]);
        return p.firstEdge(mEntries);
    }

    Edge lastEdge(const SuperPathId& pId) const
    {
        SuperPath p((*this)[pId]);
        return p.lastEdge(mEntries);
    }

    /**
     * True if the given path is an N path.
     */
    bool isGap(const SuperPathId& pId) const
    {
        uint64_t id = pId.value();
        return    mSegs[id].size() == 1 
               && SuperPath::isGap(mEntries, mSegs[id][0]);
    }

    /**
     * Determines whether the nominated SuperPath is unique wrt the expected coverage.
     */
    bool unique(const SuperPath& pPath, double pExpectedCoverage) const;

    /**
     * The number of edges in this graph.
     */
    uint64_t count() const
    {
        return mCount;
    }

    /**
     * An upper bound on the ids of edges in the graph.
     */ 
     uint64_t size() const
     {
        return mRCs.size();
     }

    bool valid(const SuperPathId& pId) const
    {
        const uint64_t n = pId.value();
        if (n >= mSegs.size())
        {
            return false;
        }

        return !mSegs[n].empty();
    }

    /**
     * Returns the base sequence, reverse comp., and mean coverage for a given path id.
     */
    void contigInfo(const Graph& pG, const SuperPathId& pId, std::string& pSeq, 
                    SuperPathId& pRc, double& pCovMean) const;

    /**
     * Prints the contigs to the given stream.
     */
    void printContigs(const std::string& pBaseName, FileFactory& pFactory,
                      Logger& pLogger, std::ostream& pOut, 
                      uint64_t pMinLength, bool pOmitSequence,
                      bool pVerboseHeaders, bool pNoLineBreaks, 
                      bool pPrintEntailed, bool pPrintRcs, uint64_t pNumThreads);

    void dump(std::ostream& pOut) const;

    /**
     * Saves the SuperGraph.
     */
    void write(const std::string& pBaseName, FileFactory& pFactory) const;

    /**
     * Reads a saved SuperGraph.
     */
    static std::unique_ptr<SuperGraph> read(const std::string& pBaseName, FileFactory& pFactory);
    
    /**
     * Returns a new SuperGraph.
     */
    static std::unique_ptr<SuperGraph> create(const std::string& pBaseName, FileFactory& pFactory);

private:

    SuperGraph(const std::string& pBaseName, FileFactory& pFactory);

    SuperGraph(const SuperGraph&);
    SuperGraph& operator=(const SuperGraph&);

    /**
     * Erase a superpath but not its reverse complement.
     */
    void halfErase(const SuperPathId& pId);

    /**
     * Given a source and sink, calculates the distance to the sink of all nodes
     * which lie within some bound of the distance between the source and sink.
     * Returns true if there is a path between the source and the sink, else false.
     */
    bool shortestPaths(const Node& pSource, const Node& pSink, uint64_t pMaxLength, 
                       const std::set<SuperPathId>* pValidPaths, std::map<Node, PathDir>& pPaths);

    /**
     * Find all paths within pRadius steps of pNode, and add them to pPaths. If pRC is true, 
     * the reverse complement paths are added instead.
     */
    void findSubgraph(const Node& pNode, std::set<SuperPathId>& pPaths, uint64_t pRadius, bool pRC);

    /**
     * The next usable SuperPathId.
     */
    SuperPathId allocId();

    /**
     * Allocates a pair of ids for a path and its reverse complement.
     */
    std::pair<SuperPathId, SuperPathId> allocRcIds()
    {
        SuperPathId fd = allocId();
        SuperPathId rc = allocId();
        mRCs[fd.value()] = rc.value();
        mRCs[rc.value()] = fd.value();
        return std::make_pair(fd, rc);
    }

    /**
     * Frees the given SuperPathId for future use.
     */
    void freeId(SuperPathId pId);

    /**
     * Make room for another SuperPathId.
     */
    void grow();

    EntryEdgeSet mEntries;
    uint64_t mNextId;
    uint64_t mCount;
    std::unordered_map<Node, SuperPathIds> mSucc;
    std::vector<SuperPath::Segments> mSegs;
    std::vector<uint64_t> mRCs;
};

#endif      // SUPERGRAPH_HH
