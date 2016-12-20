// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SCAFFOLDGRAPH_HH
#define SCAFFOLDGRAPH_HH

#ifndef GOSSCMDCONTEXT_HH
#include "GossCmdContext.hh"
#endif

#ifndef PAIRLINKER_HH
#include "PairLinker.hh"
#endif

#ifndef BOOST_TUPLE
#include <boost/tuple/tuple.hpp>
#define BOOST_TUPLE
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_UNORDERED_MAP
#include <unordered_map>
#define STD_UNORDERED_MAP
#endif

#ifndef STD_UNORDERED_SET
#include <unordered_set>
#define STD_UNORDERED_SET
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class ScaffoldGraph
{
public:
    static constexpr uint64_t version = 2012032701ULL;
    // Version history
    //  2012032701  - introduce version tracking

    typedef PairLinker::Orientation Orientation;

    struct Header
    {
        uint64_t version;
        uint64_t insertSize;
        uint64_t insertRange;
        Orientation orientation;

        Header()
        {
        }

        Header(uint64_t pInsertSize, uint64_t pInsertRange, Orientation pOrientation)
            : version(ScaffoldGraph::version), insertSize(pInsertSize), 
              insertRange(pInsertRange), orientation(pOrientation)
        {
        }
    };

    class Builder
    {
    public:
        void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                       const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                       const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2);

        void end();

        Builder(const std::string& pBaseName, FileFactory& pFactory, 
                const SuperGraph& pSuperGraph,
                const uint64_t pInsertSize, const uint64_t pInsertRange, 
                const Orientation pOrientation);

    private:
        const std::string mBaseName;
        FileFactory& mFactory;
        const SuperGraph& mSuperGraph;
        Header mHeader;
        FileFactory::OutHolderPtr mScafFilePtr;
        std::ostream& mScafFile;
    };

    typedef int64_t Dist;
    typedef uint64_t Count;
    typedef int64_t Range;
    typedef boost::tuple<SuperPathId, Dist, Count, Range> Edge;
    typedef std::vector<Edge> Edges;

    struct Node 
    {
        Edges mFrom;
        Edges mTo;
    };

    typedef std::map<SuperPathId, Node> NodeMap;

    bool hasNode(SuperPathId pNode) const
    {
        return mNodes.count(pNode);
    }

    void dumpNodes(std::ostream& pOut) const;

    void dumpDot(std::ostream& pOut, const SuperGraph& pSg) const;

    void getNodes(std::vector<SuperPathId>& pNodes) const;

    void getNodes(std::unordered_set<SuperPathId>& pNodes) const;

    void getNodesAndRcs(const SuperGraph& pSg, std::unordered_set<SuperPathId>& pNodes) const;

    // Collects the nodes reachable via forward and backward edges from pNode.
    void getConnectedNodes(SuperPathId pNode, std::unordered_set<SuperPathId>& pNodes) const;

    // Collects the nodes reachable via forward edges from pNode.
    void getNodesFrom(SuperPathId pNode, std::unordered_set<SuperPathId>& pNodes) const;

    // Collects the nodes which can reach pNode via forward edges.
    void getNodesTo(SuperPathId pNode, std::unordered_set<SuperPathId>& pNodes) const;

    const Edges& getFroms(SuperPathId pTo) const;

    const Edges& getTos(SuperPathId pFrom) const;

    void add(SuperPathId pFrom, SuperPathId pTo, int64_t pGap, uint64_t pCount, int64_t pRange);

    void mergeEdge(SuperPathId pFrom, SuperPathId pTo, int64_t pGap, uint64_t pCount, int64_t pRange);

    // Removes an edge.
    void remove(SuperPathId pFrom, SuperPathId pTo);

    // Removes a node.
    void remove(SuperPathId pA);

    void removeTo(SuperPathId pFrom, SuperPathId pTo);

    void removeFrom(SuperPathId pFrom, SuperPathId pTo);

    // Returns the next scaffold file basename given a supergraph basename.
    static std::string nextScafBaseName(const GossCmdContext& pCxt, const std::string& pSgBaseName);

    // Deletes all of the scaffold files associated with the given supergraph basename.
    static void removeScafFiles(const GossCmdContext& pCxt, const std::string& pSgBaseName);

    // True if there are any scaffold files associated with the supergraph basename.
    static bool existScafFiles(const GossCmdContext& pCxt, const std::string& pSgBaseName);

    static std::unique_ptr<ScaffoldGraph> read(const std::string& pName, FileFactory& pFactory,
                                             uint64_t pMinLinkCount);

    void mergeRcs(const SuperGraph& pSg);

    NodeMap mNodes;

private:

    ScaffoldGraph();

    void addDummyRcNodes(const SuperGraph& pSg);
};


#endif      // SCAFFOLDGRAPH_HH
