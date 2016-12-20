// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "BackgroundMultiConsumer.hh"
#include "IndexedBinaryHeap.hh"
#include "ProgressMonitor.hh"
#include "SuperGraph.hh"

using namespace std;
using namespace boost;

constexpr uint64_t SuperGraph::version;

namespace // anonymous
{
    void print(ostream& pOut, const string& pStr, uint64_t pCols)
    {
        for (uint64_t i = 0; i < pStr.size(); i += pCols)
        {
            uint64_t upper = std::min(i + pCols, uint64_t(pStr.size()));
            for (uint64_t j = i; j < upper; ++j)
            {
                pOut << pStr[j];
            }
            pOut << endl;
        }
    }

    class ContigVisitor
    {
    public:

        // Skip the first k bases.
        bool operator()(const Graph::Edge& e, const uint64_t& pRank)
        {

            // cerr << "vis " << Gossamer::kmerToString(mGraph.K() + 1, e.value()) << '\n';

            const uint32_t c = mGraph.multiplicity(pRank);
            mMin = std::min(mMin, c);
            mMax = std::max(mMax, c);
            mSum += c;
            mSum2 += c * c;
            ++mNumEdges;

            Gossamer::position_type p(e.value());
            if (mStart || mRestart)
            {
                // First edge.
                Graph::Node n = mGraph.from(e);
                if (mRestart || mGraph.inDegree(n) == 0 || mGraph.canonical(n))
                {
                    mStart = false;
                    const uint64_t K = mGraph.K();
                    const uint64_t rho = K + 1;
                    uint64_t skip = mRestart ? K - mRestartNum : 0;
                    // cerr << "skip " << skip << '\n';
                    mRestart = false;
                    p.reverse();
                    p >>= std::numeric_limits<Gossamer::position_type>::digits - 2 * rho;
                    for (uint64_t i = 0; i < rho; ++i)
                    {
                        if (skip)
                        {
                            skip -= 1;
                        }
                        else
                        {
                            mContig << base(p & 3);
                        }
                        p >>= 2;
                    }
                }
            }
            else
            {
                mContig << base(p & 3);
            }
            mLast = e;
            return true;
        }

        void addGap(int64_t pNum)
        {
            // cerr << "addGap " << pNum << '\n';
            if (pNum > 0)
            {
                mRestart = true;
                mContig << string(pNum, 'N');
                mRestartNum = mGraph.K();
            }
            else if (pNum <= 0)
            {
                mRestart = true;
                mRestartNum = mGraph.K() + pNum;
            }
        }

        // The accumulated vector without the last k bases.
        string getTruncatedContig() const
        {
            string str = mContig.str();
            if (str.size() == 0)
            {
                return str;
            }
            Graph::Node n = mGraph.to(mLast);
            if (mGraph.outDegree(n) == 0 || mGraph.antiCanonical(n))
            {
                return str;
            }
            if (str.size() < mGraph.K())
            {
                return "";
            }
            return string(str, 0, str.size() - mGraph.K());
        }

        uint32_t min() const
        {
            return mMin;
        }

        uint32_t max() const
        {
            return mMax;
        }

        double mean() const
        {
            return double(mSum) / double(mNumEdges);
        }

        double stdDev() const
        {
            return sqrt( (double)(mNumEdges * mSum2 - mSum * mSum) ) / mNumEdges;
        }

        ContigVisitor(const Graph& pGraph)
            : mGraph(pGraph), mStart(true), mRestart(false), mRestartNum(0),
              mContig(), mLast(Gossamer::position_type(0)),
              mMin(numeric_limits<uint32_t>::max()), mMax(numeric_limits<uint32_t>::min()),
              mSum(0), mSum2(0), mNumEdges(0)
        {
        }

    private:

        char base(uint16_t pB)
        {
            static const char bases[] = {'A', 'C', 'G', 'T'};
            BOOST_ASSERT(pB <= 3);
            return bases[pB];
        }

        const Graph& mGraph;
        bool mStart;
        bool mRestart;
        uint32_t mRestartNum;
        stringstream mContig;
        Graph::Edge mLast;

        uint32_t mMin;
        uint32_t mMax;
        uint64_t mSum;
        uint64_t mSum2;
        uint64_t mNumEdges;
    };

    class ContigPrinter
    {
    public:

        void push_back(uint64_t pId)
        {
            SuperPathId id(pId);
            string idStr = lexical_cast<string>(pId);
            string segLens;
            string segStarts;
            const EntryEdgeSet& entries(mSg.entries());
            const vector<SuperPath::Segment>& segs(mSg[id].segments());
            ContigVisitor vis(mG);
            for (vector<SuperPath::Segment>::const_iterator
                 j = segs.begin(); j != segs.end(); ++j)
            {
                // Extend contig
                const SuperPath::Segment s = *j;
                const int64_t l = SuperPath::segLength(entries, s);
                const bool isGap = SuperPath::isGap(entries, s);
                if (isGap)
                {
                    vis.addGap(l);
                }
                else
                {
                    EntryEdgeSet::Edge e(entries.select(s.linearPath()));
                    mG.linearPath(Graph::Edge(e.value()), vis);
                }

                // Header info
                if (j != segs.begin())
                {
                    segLens += ":";
                    segStarts += ":";
                }
                segLens += lexical_cast<string>(l);
                segStarts += isGap ? (lexical_cast<string>(l) + "g") : lexical_cast<string>(s);
            }
            string v = vis.getTruncatedContig();

            SuperPathId rc(mSg.reverseComplement(id));
            SuperGraph::SuperPathIds succs;
            mSg.successors(mSg.end(id), succs);
            string succIds = "";
            if (succs.size()) 
            {
                succIds = lexical_cast<string>(succs[0].value());
                for (uint64_t j = 1; j < succs.size(); ++j)
                {
                    succIds += ":" + lexical_cast<string>((succs[j]).value());
                }
            }

            if (v.size() >= mMinLength)
            {
                unique_lock<mutex> lk(mMut);
                if (mOmitSequence)
                {
                    mOut << idStr << '\t' << v.size() << "\t[" << segLens << "]\t[" << segStarts << "]\t"
                         << rc.value() << "\t[" << succIds << "]\t"
                         << vis.min() << '\t' << vis.max() << '\t' << vis.mean() << '\t' << vis.stdDev() << endl;
                }
                else
                {
                    mOut << '>' << idStr;
                    if (mVerboseHeaders)
                    {
                        mOut << ' ' << v.size() << ",[" << segLens << "]" << ",[" << segStarts << "],"
                             << rc.value() << ",[" << succIds << "],"
                             << vis.min() << ',' << vis.max() << ',' << vis.mean() << ',' << vis.stdDev();
                    }
                    mOut << endl;
                    print(mOut, v, mCols);
                }
            }
        }

        ContigPrinter(const Graph& pG, const SuperGraph& pSg, mutex& pMut, ostream& pOut, 
                      uint64_t pMinLength, uint64_t pCols, bool pOmitSequence, bool pVerboseHeaders)
            : mG(pG), mSg(pSg), mMut(pMut), mOut(pOut), mMinLength(pMinLength), mCols(pCols),
              mOmitSequence(pOmitSequence), mVerboseHeaders(pVerboseHeaders)
        {
        }

    private:

        const Graph& mG;
        const SuperGraph& mSg;
        mutex& mMut;
        ostream& mOut;
        const uint64_t mMinLength;
        const uint64_t mCols;
        const bool mOmitSequence;
        const bool mVerboseHeaders;
    };

    typedef std::shared_ptr<ContigPrinter> ContigPrinterPtr;

    // TODO: Fix in light of gap segments!
    bool entails(const SuperPath::Segments& pLhs, const SuperPath::Segments& pRhs)
    {
        BOOST_ASSERT(pLhs.size() > 0);
        BOOST_ASSERT(pRhs.size() > 0);

        if (pRhs.size() > pLhs.size())
        {
            return false;
        }
        for (uint64_t i = 0; i < pLhs.size() - pRhs.size(); ++i)
        {
            bool matches = true;
            for (uint64_t j = 0; j < pRhs.size(); ++j)
            {
                if (pLhs[i + j] != pRhs[j])
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
            {
                return true;
            }
        }
        return false;
    }

}
// namespace anonymous


struct NodePathDir
{
    bool operator<(const NodePathDir& pOther) const
    {
        return    mNode != pOther.mNode 
               && mDir.mDist < pOther.mDir.mDist;
    }

    bool operator==(const NodePathDir& pOther) const
    {
        return mNode == pOther.mNode;
    }

    const SuperGraph::Node& key() const
    {
        return mNode;
    }

    uint32_t priority() const
    {
        return mDir.mDist;
    }

    NodePathDir(SuperGraph::Node pNode, uint32_t pDist, SuperPathId pEdge)
        : mNode(pNode), mDir(pEdge, pDist)
    {
    }

    SuperGraph::Node mNode;
    SuperGraph::PathDir mDir;
};


/**             
 * Find all paths within pRadius steps of pNode, and add them to pPaths. If pRC is true, 
 * the reverse complement paths are added instead.
 */         
void
SuperGraph::findSubgraph(const Node& pNode, set<SuperPathId>& pPaths,
                         uint64_t pRadius, bool pRC)
{
    if (pRadius == 0)
    {
        return; 
    }   
   
    vector<SuperPathId> succs;
    successors(pNode, succs);
    for (uint64_t i = 0; i < succs.size(); ++i)
    {
        SuperPathId id = succs[i];
        SuperPathId recId = id;
        if (pRC)
        {
            const SuperPath p((*this)[id]);
            recId = p.reverseComplement();
        }
        
        if (!pPaths.count(recId))
        {
            pPaths.insert(recId);
            findSubgraph(end(id), pPaths, pRadius - 1, pRC);
        }
    }
}

bool
SuperGraph::shortestPaths(const Node& pSource, const Node& pSink, uint64_t pMaxLength, 
                          const set<SuperPathId>* pValidPaths, map<Node, PathDir>& pPathDirs)
{
    IndexedBinaryHeap<NodePathDir, Node> heap;
    map<Node, PathDir> paths;

    // Find the path from rc sink to rc source, then calculate its rc, to 
    // get the path from source to sink.
    Node source = pSink;
    Node sink = pSource;
    source.value().reverseComplement(mEntries.K());
    sink.value().reverseComplement(mEntries.K());

    // Note: dummy SuperPathId edge
    heap.push(NodePathDir(source, 0, SuperPathId(0)));
    
    bool found = false;
    SuperPathIds succs;

    uint64_t num_nodes(0);
    while (true)
    {
        if (heap.size() == 0)
        {
            // Visited all nodes.
            break;
        }
        
        const NodePathDir& np(heap.pop());
        const Node& n(np.mNode);
        const PathDir& dir(np.mDir);

        if (dir.mDist > pMaxLength)
        {
            // The next nearest node is too far away.
            break;
        }
        if (n == sink)
        {
            found = true;
        }

        // For all nodes one edge away from the current.
        succs.clear();
        successors(n, succs);
        for (SuperPathIds::const_iterator i = succs.begin(); i != succs.end(); ++i)
        {
            if (pValidPaths && !pValidPaths->count(*i))
            {
                continue;
            }

            const SuperPath p((*this)[*i]);
            const Node e(end(p));
            const uint32_t n_len = np.mDir.mDist + size(p);

            if (heap.contains(e))
            {
                NodePathDir& enp(heap.get(e));
                BOOST_ASSERT(e == enp.key());
                if (n_len < enp.mDir.mDist)
                {
                    // The route to e via n is shorter than all other known alternatives.
                    enp.mDir.mDist = n_len;
                    enp.mDir.mEdge = *i;
                    heap.up(enp);
                }
            }
            else if (!paths.count(e))
            {
                // Node was never in the heap - add it.
                heap.push(NodePathDir(e, n_len, *i));
            }
        }

        // Add curr to the path map.
        paths.insert(make_pair(n, dir));
        ++num_nodes;
    }

    if (found)
    {
        // Reverse the results.
        for (map<Node, PathDir>::iterator
             i = paths.begin(); i != paths.end(); ++i)
        {
            PathDir dir(i->second);
            // Skip the initial dummy edge.
            if (dir.mDist)
            {
                SuperPath p = (*this)[dir.mEdge];
                dir.mEdge = p.reverseComplement();
                Node n = i->first;
                n.value().reverseComplement(mEntries.K());
                pPathDirs.insert(make_pair(n, dir));
            }
        }
    }
    else
    {
        pPathDirs.clear();
    }

    return found;
}

SuperGraph::ShortestPathIterator::ShortestPathIterator(
        SuperGraph& pSGraph, const Node& pSource, const Node& pSink, 
        uint64_t pMaxLength, uint64_t pSearchRadius)
    : mSource(pSource), mSink(pSink), mMaxLength(pMaxLength), mSGraph(pSGraph),
      mValid(true)
{
    set<SuperPathId> valid;
    set<SuperPathId>* validPtr = 0;
    if (pSearchRadius)
    {
        pSGraph.findSubgraph(pSource, valid, pSearchRadius, true);
        validPtr = &valid;
    }
    if (mSGraph.shortestPaths(mSource, mSink, mMaxLength, validPtr, mNodeShortestPaths))
    {
        uint32_t shortest = mNodeShortestPaths.find(mSource)->second.mDist;
        mPaths.push(DevPath(shortest));
    }

    next(mCurPath);
}

bool
SuperGraph::ShortestPathIterator::next(vector<SuperPathId>& pPath)
{
    if (!mPaths.size())
    {
        mValid = false;
        return false;
    }

    // The next shortest path.
    const DevPath dp = mPaths.top();
    mPaths.pop();
    uint32_t init_len = 0;
    bool extend = true;
    map<Node, PathDir>::const_iterator sp_itr;
    
    Node cur = mSource;
    if (!dp.mDevs.empty())
    {
        SuperPathId pid = dp.mDevs.back();
        cur = mSGraph.end(pid);

        // Calculate the length of the path to just after the last deviation.
        // i.e. path length - length of shortest path from that deviation.
        
        sp_itr = mNodeShortestPaths.find(cur);
        if (sp_itr != mNodeShortestPaths.end())
        {
            const uint32_t last_dev_len = mNodeShortestPaths.find(cur)->second.mDist;
            init_len = dp.mLength - last_dev_len;
        }
        else
        {
            // The target node was too far from the sink to include in the shortest path map!
            // There won't be any shorter paths.
            extend = false;
        }
    }

    //cerr << "extend: " << extend << '\n';
    // Add all single edge deviations from p, following the last present deviation.
    if (extend)
    {
        SuperPathIds succs;
        while (cur != mSink)
        {
            //cerr << "cur: " << kmerToString(27, cur.value()) << '\n';
            succs.clear();
            mSGraph.successors(cur, succs);
            const PathDir min_dir(mNodeShortestPaths.find(cur)->second);
            for (SuperPathIds::const_iterator
                 i = succs.begin(); i != succs.end(); ++i)
            {
                //cerr << "next: " << (*i).value();
                if (*i != min_dir.mEdge)
                {
                    //cerr << " alt ";
                    DevPath ddp(dp);
                    ddp.mDevs.push_back(*i);
                    Node dev_node = mSGraph.end(*i);
                    sp_itr = mNodeShortestPaths.find(dev_node);
                    if (   sp_itr != mNodeShortestPaths.end()
                        || dev_node == mSink)
                    {
                        //cerr << "follow\n";
                        uint32_t dev_len = mSGraph.size(*i);
                        if (sp_itr != mNodeShortestPaths.end())
                        {
                            dev_len += sp_itr->second.mDist;
                        }
                        ddp.mLength = init_len + dev_len;
                        mPaths.push(ddp);
                    }
                    else
                    {
                        //cerr << "unknown\n";
                    }
                }
                //cerr << " skip\n";
            }
            SuperPath p = mSGraph[min_dir.mEdge];
            cur = mSGraph.end(p);
            init_len += mSGraph.size(p);
        }
    }
    
    // Generate the sequence of SuperPathIds.
    bool ok = true;
    pPath.clear();
    cur = mSource;
    vector<SuperPathId>::const_iterator itr_dev(dp.mDevs.begin());

    // cerr << "path:";
    while (cur != mSink)
    {
        // cerr << ' ' << kmerToString(mSGraph.entries().K(), cur.value());
        SuperPathId next_path(0);
        if (   itr_dev != dp.mDevs.end()
            && cur == mSGraph.start(mSGraph[*itr_dev]))
        {
            // Deviate.
            next_path = *itr_dev;
            ++itr_dev;
        }
        else
        {
            // Follow shortest path.
            sp_itr = mNodeShortestPaths.find(cur);
            if (sp_itr == mNodeShortestPaths.end())
            {
                // The target node was not in range - the path is too long.
                ok = false;
                break;
            }
            next_path = sp_itr->second.mEdge;
        }
        // cerr << ' ' << next_path.value();
        pPath.push_back(next_path);
        cur = mSGraph.end(mSGraph[next_path]);
    }
    // cerr << '\n';

    return ok;
}

/**
 * Returns the reverse complement of the given path.
 */
SuperPathId
SuperGraph::reverseComplement(const SuperPathId& pId) const
{
    return SuperPathId(mRCs[pId.value()]);
}

void
SuperGraph::nodes(vector<Node>& pNodes) const
{
    pNodes.reserve(mSucc.size());
    for (unordered_map<Node, SuperPathIds>::const_iterator
         i = mSucc.begin(); i != mSucc.end(); ++i)
    {
        pNodes.push_back(i->first);
    }
}

/**
 * Populate pSucc with the SuperPathIds for the successors of the
 * given node.
 */
void 
SuperGraph::successors(const Node& pNode, SuperPathIds& pSucc) const
{
    pSucc.clear();
    unordered_map<Node, SuperPathIds>::const_iterator i(mSucc.find(pNode));
    if (i != mSucc.end())
    {
        pSucc.insert(pSucc.end(), i->second.begin(), i->second.end());
    }
}

/**
 * Determines whether the nominated SuperPath is unique wrt the expected coverage.
 */
// TODO: Make the thresholds configurable.
bool
SuperGraph::unique(const SuperPath& pPath, double pExpectedCoverage) const
{
    if (isGap(pPath.id()))
    {
        return false;
    }

    if (size(pPath) + mEntries.K() < 50)
    {
        return false;
    }

    double n = 0;
    double c = 0;
    const vector<SuperPath::Segment>& segs(pPath.segments());
    for (uint64_t i = 0; i < segs.size(); ++i)
    {
        SuperPath::Segment s(segs[i]);
        if (!SuperPath::isGap(mEntries, s))
        {
            double l = mEntries.length(s);
            n += l;
            c += l * mEntries.multiplicity(s);
        }
    }
    c /= n;

    // See: Zerbino, D., et al. "Pebble and Rock Band: Heuristic Resolution of 
    //      Repeats and Scaffolding in the Velvet Short-Read de novo Assembler", 2009
    const double rho = pExpectedCoverage;
    const double k = log(2.0) / 2.0;
    double f = k + (n / (2 * rho)) * (rho * rho - (c*c) / 2.0);
    return f >= 5.0;
}

void 
SuperGraph::contigInfo(const Graph& pG, const SuperPathId& pId, string& pSeq, 
                       SuperPathId& pRc, double& pCovMean) const
{
    ContigVisitor vis(pG);
    const vector<SuperPath::Segment>& segs(mSegs[pId.value()]);
    for (vector<SuperPath::Segment>::const_iterator
         j = segs.begin(); j != segs.end(); ++j)
    {
        const SuperPath::Segment s = *j;
        const int64_t l = SuperPath::segLength(mEntries, s);
        const bool isGap = SuperPath::isGap(mEntries, s);
        if (isGap)
        {
            vis.addGap(l);
        }
        else
        {
            EntryEdgeSet::Edge e(mEntries.select(s.linearPath()));
            pG.linearPath(Graph::Edge(e.value()), vis);
        }
    }
    pSeq = vis.getTruncatedContig();
    pRc = reverseComplement(pId);
    pCovMean = vis.mean();
}

void 
SuperGraph::printContigs(const std::string& pBaseName, FileFactory& pFactory,
                         Logger& pLogger, std::ostream& pOut, 
                         uint64_t pMinLength, bool pOmitSequence,
                         bool pVerboseHeaders, bool pNoLineBreaks, 
                         bool pPrintEntailed, bool pPrintRcs, uint64_t pNumThreads)
{
    GraphPtr gPtr = Graph::open(pBaseName, pFactory);
    Graph& g(*gPtr);

    unordered_set<SuperPathId> entailed;

    // Calculate entailed SuperPaths.
    if (!pPrintEntailed)
    {
        unordered_map<uint64_t,SuperPathIds> segIndex;
        {
            dynamic_bitset<> seen(mEntries.count());
     
            // log(info, "finding repeated segments");
            for (PathIterator i(*this); i.valid(); ++i)
            {
                const SuperPath path((*this)[*i]);
                const vector<SuperPath::Segment>& segs(path.segments());
                for (uint64_t j = 0; j < segs.size(); ++j)
                {
                    const uint64_t seg = segs[j];
                    if (SuperPath::isGap(mEntries, seg))
                    {
                        continue;
                    }
                    if (seen[seg])
                    {
                        if (!segIndex.count(seg))
                        {
                            segIndex[seg] = SuperPathIds();
                        }
                    }
                    else
                    {
                        seen[seg] = true;
                    }
                }
            }
        }

        // log(info, "populating segment index");
        for (PathIterator i(*this); i.valid(); ++i)
        {
            const SuperPath path((*this)[*i]);
            const vector<SuperPath::Segment>& segs(path.segments());
            for (uint64_t j = 0; j < segs.size(); ++j)
            {
                const uint64_t seg = segs[j];
                unordered_map<uint64_t, SuperPathIds>::iterator itr = segIndex.find(seg);
                if (itr != segIndex.end())
                {
                    itr->second.push_back(*i);
                }
            }
        }

        // Traverse the index, and look for entailed paths.
        for (unordered_map<uint64_t,SuperPathIds>::iterator i = segIndex.begin(); i != segIndex.end(); ++i)
        {
            SuperPathIds& ids = i->second;
            sort(ids.begin(), ids.end());
            ids.erase(::unique(ids.begin(), ids.end()), ids.end());
            for (uint64_t j = 0; j < ids.size(); ++j)
            {
                const SuperPath p_j((*this)[ids[j]]);
                const SuperPath::Segments& u(p_j.segments());
                for (uint64_t k = j + 1; k < ids.size(); ++k)
                {
                    const SuperPath p_k((*this)[ids[k]]);
                    const SuperPath::Segments& v(p_k.segments());
                    if (entails(u, v))
                    {
                        entailed.insert(ids[k]);
                    }
                    else if (entails(v, u))
                    {
                        entailed.insert(ids[j]);
                    }
                }
            }
        }
    }

    if (pOmitSequence)
    {
        pOut << "Id\tLength\tSegmentLengths\tSegmentStarts\tRevCompId\tSuccessorIds\tMinCov\tMaxCov\tMeanCov\tStdDevCov" << endl;
    }

    mutex mut;
    BackgroundMultiConsumer<uint64_t> grp(128);
    vector<ContigPrinterPtr> printers;
    const uint64_t cols = pNoLineBreaks ? -1 : 60;
    for (uint64_t i = 0; i < pNumThreads; ++i)
    {
        // printers.push_back();
        printers.push_back(ContigPrinterPtr(new ContigPrinter(g, *this, mut, pOut, pMinLength, cols, pOmitSequence, pVerboseHeaders)));
        grp.add(*printers.back());
    }

    uint64_t j = 0;
    ProgressMonitor mon(pLogger, count(), 100);
    for (PathIterator i(*this); i.valid(); ++i)
    {
        SuperPathId id = *i;
        mon.tick(j++);

        if (entailed.count(id))
        {
            continue;
        }

        if (!pPrintRcs && id.value() > reverseComplement(id).value())
        {
            continue;
        }
        
        grp.push_back(id.value());
    }
    grp.wait();
}


void
SuperGraph::dump(std::ostream& pOut) const
{
    pOut << "elements\n";
    for (uint64_t i = 0; i < mRCs.size(); ++i)
    {
        pOut << i << " [";
        for (uint64_t j = 0; j < mSegs[i].size(); ++j)
        {
            pOut << " " << mSegs[i][j];
        }
        pOut << "] " << mRCs[i] << '\n';
    }

    pOut << "succs\n";
    for (unordered_map<Node, SuperPathIds>::const_iterator
         i = mSucc.begin(); i != mSucc.end(); ++i)
    {
        pOut << i->first.value() << ":";
        const SuperPathIds& ids(i->second);
        for (SuperPathIds::const_iterator j = ids.begin(); j != ids.end(); ++j)
        {
            pOut << " " << (*j).value();
        }
        pOut << '\n';
    }
    pOut << "paths\n";
    for (PathIterator i(*this); i.valid(); ++i)
    {
        SuperPath p((*this)[*i]);
        pOut << lexical_cast<string>(p) << '\n';
    }
}

void 
SuperGraph::write(const string& pBaseName, FileFactory& pFactory) const
{
    string name = pBaseName + "-supergraph";

    // mHeader
    {
        FileFactory::OutHolderPtr op(pFactory.out(name + ".header"));
        ostream& o(**op);
        Header h;
        h.version = version;
        o.write(reinterpret_cast<const char*>(&h), sizeof(h));
    }

    // mNextId
    {
        FileFactory::OutHolderPtr op(pFactory.out(name + ".next-id"));
        ostream& o(**op);
        o.write(reinterpret_cast<const char*>(&mNextId), sizeof(mNextId));
    }

    // mCount
    {
        FileFactory::OutHolderPtr op(pFactory.out(name + ".count"));
        ostream& o(**op);
        o.write(reinterpret_cast<const char*>(&mCount), sizeof(mCount));
    }

    // mSucc
    {
        MappedArray<Node>::Builder nodeArr(name + ".succ.nodes", pFactory);
        MappedArray<uint32_t>::Builder numPathIdsArr(name + ".succ.num-path-ids", pFactory);
        MappedArray<SuperPathId>::Builder pathIdArr(name + ".succ.path-ids", pFactory);

        for (unordered_map<Node, SuperPathIds>::const_iterator
             i = mSucc.begin(); i != mSucc.end(); ++i)
        {
            nodeArr.push_back(i->first);
            numPathIdsArr.push_back(i->second.size());
            for (uint64_t j = 0; j < i->second.size(); ++j)
            {
                pathIdArr.push_back(i->second[j]);
            }
        }
        nodeArr.end();
        numPathIdsArr.end();
        pathIdArr.end();
    }

    // mSegs
    {
        MappedArray<uint32_t>::Builder numSegmentsArr(name + ".segs.num-segments", pFactory);
        MappedArray<uint64_t>::Builder segmentArr(name + ".segs.segments", pFactory);
        for (vector<SuperPath::Segments>::const_iterator
             i = mSegs.begin(); i != mSegs.end(); ++i)
        {
            const SuperPath::Segments& segs(*i);
            numSegmentsArr.push_back(segs.size());
            for (SuperPath::Segments::const_iterator
                 j = segs.begin(); j != segs.end(); ++j)
            {
                segmentArr.push_back(*j);
            }
        }
        numSegmentsArr.end();
        segmentArr.end();
    }

    // mRCs
    {
        MappedArray<uint64_t>::Builder rcPathIdArr(name + ".rcs.rc-path-ids", pFactory);
        for (vector<uint64_t>::const_iterator 
             i = mRCs.begin(); i != mRCs.end(); ++i)
        {
            rcPathIdArr.push_back(*i);
        }
        rcPathIdArr.end();
    }
}

unique_ptr<SuperGraph>
SuperGraph::read(const string& pBaseName, FileFactory& pFactory)
{
    string name = pBaseName + "-supergraph";
    unique_ptr<SuperGraph> sg(new SuperGraph(pBaseName, pFactory));
    
    // mHeader
    {
        FileFactory::InHolderPtr ip;
        try 
        {
            ip = pFactory.in(name + ".header");
        }
        catch (...)
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("No supergraph header file found.")
                    << Gossamer::version_mismatch_info(make_pair(SuperGraph::version, 0)));
        }
        Header h;
        (**ip).read(reinterpret_cast<char*>(&h), sizeof(h));
        if (h.version != SuperGraph::version)
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(name + ".header")
                    << Gossamer::version_mismatch_info(make_pair(SuperGraph::version, h.version)));
        }
    }

    // mNextId
    {
        FileFactory::InHolderPtr ip(pFactory.in(name + ".next-id"));
        istream& i(**ip);
        i.read(reinterpret_cast<char*>(&sg->mNextId), sizeof(sg->mNextId));
    }

    // mCount
    {
        FileFactory::InHolderPtr ip(pFactory.in(name + ".count"));
        istream& i(**ip);
        i.read(reinterpret_cast<char*>(&sg->mCount), sizeof(sg->mCount));
    }

    // mSucc
    {
        MappedArray<Node>::LazyIterator nodeItr(name + ".succ.nodes", pFactory);
        MappedArray<uint32_t>::LazyIterator numPathIdsItr(name + ".succ.num-path-ids", pFactory);
        MappedArray<SuperPathId>::LazyIterator pathIdItr(name + ".succ.path-ids", pFactory);
        for (; nodeItr.valid(); ++nodeItr, ++numPathIdsItr)
        {
            vector<SuperPathId>& pathIds(sg->mSucc[*nodeItr]);
            pathIds.reserve(*numPathIdsItr);
            for (uint32_t i = 0; i < *numPathIdsItr; ++i, ++pathIdItr)
            {
                pathIds.push_back(*pathIdItr);
            }
        }
    }

    // mSegs
    {
        MappedArray<uint32_t>::LazyIterator numSegmentsItr(name + ".segs.num-segments", pFactory);
        MappedArray<uint64_t>::LazyIterator segmentItr(name + ".segs.segments", pFactory);

        for (; numSegmentsItr.valid(); ++numSegmentsItr)
        {
            sg->mSegs.resize(sg->mSegs.size() + 1);
            SuperPath::Segments& segs(sg->mSegs.back());
            segs.reserve(*numSegmentsItr);
            for (uint32_t i = 0; i < *numSegmentsItr; ++i, ++segmentItr)
            {
                segs.push_back(*segmentItr);
            }
        }
    }

    // mRCs
    {
        MappedArray<uint64_t>::LazyIterator rcPathIdItr(name + ".rcs.rc-path-ids", pFactory);
        uint64_t i = 0;
        for (; rcPathIdItr.valid(); ++rcPathIdItr, ++i)
        {
            // cerr << i << " -> " << *rcPathIdItr << '\n';
            sg->mRCs.push_back(*rcPathIdItr);
        }
    }

    return sg;
}

std::unique_ptr<SuperGraph>
SuperGraph::create(const std::string& pBaseName, FileFactory& pFactory)
{
    unique_ptr<SuperGraph> sgPtr(new SuperGraph(pBaseName, pFactory));
    const EntryEdgeSet& entries(sgPtr->mEntries);
    const uint64_t n = entries.count();
    sgPtr->mSegs.resize(n+1);
    sgPtr->mRCs.resize(n+1);
    uint64_t i;
    for (i = 0; i < n; ++i)
    {
        Edge e = entries.select(i);
        Node n = entries.from(e);
        sgPtr->mSucc[n].push_back(SuperPathId(i));
        sgPtr->mSegs[i].push_back(i);
        sgPtr->mRCs[i] = entries.endRank(i);
    }
    sgPtr->mRCs[i] = invalidSuperPathId;
    return sgPtr;
}

/**
 * Creates a new SuperPath consisting of the given SuperPaths, in order,
 * and its reverse complement.
 */
pair<SuperPathId, SuperPathId>
SuperGraph::link(const std::vector<SuperPathId>& pPaths)
{
    BOOST_ASSERT(!pPaths.empty());
    pair<SuperPathId, SuperPathId> ids = allocRcIds();
    uint64_t fd = ids.first.value();
    uint64_t rc = ids.second.value();

    SuperPath::Segments& fdSegs(mSegs[fd]);
    SuperPath::Segments& rcSegs(mSegs[rc]);
    BOOST_ASSERT(fdSegs.empty());
    BOOST_ASSERT(rcSegs.empty());

    uint64_t sz = 0;
    for (uint64_t i = 0; i < pPaths.size(); ++i)
    {
        sz += mSegs[pPaths[i].value()].size();
    }

    fdSegs.reserve(sz);
    rcSegs.reserve(sz);
    for (uint64_t i = 0; i < pPaths.size(); ++i)
    {
        uint64_t fd = pPaths[i].value();
        uint64_t rc = reverseComplement(pPaths[i]).value();
        const SuperPath::Segments& segs(mSegs[fd]);
        const SuperPath::Segments& segsRc(mSegs[rc]);
        // cerr << "fd: " << fd << " (" << segs.size() << ")\n";
        // cerr << "rc: " << rc << " (" << segsRc.size() << ")\n";
        BOOST_ASSERT(segs.size());
        BOOST_ASSERT(segsRc.size());
        fdSegs.insert(fdSegs.end(), segs.begin(), segs.end());
        rcSegs.insert(rcSegs.begin(), segsRc.begin(), segsRc.end());
    }

/*
    cerr << "fdSegs";
    for (SuperPath::Segments::const_iterator i = fdSegs.begin(); i != fdSegs.end(); ++i)
    {
        cerr << ' ' << *i;
    }
    cerr << "\nrcSegs";
    for (SuperPath::Segments::const_iterator i = rcSegs.begin(); i != rcSegs.end(); ++i)
    {
        cerr << ' ' << *i;
    }
    cerr << '\n';
*/

    BOOST_ASSERT(fdSegs.size() == rcSegs.size());

    mSucc[start(ids.first)].push_back(ids.first);
    mSucc[start(ids.second)].push_back(ids.second);

    mCount += 2;

/*
    cerr << "ids: " << ids.first.value() << " " << ids.second.value() << '\n';

    if (ids.first != reverseComplement(ids.second))
    {
        cerr << ids.first.value() << " " << reverseComplement(ids.second).value() << '\n';
    }
*/
    BOOST_ASSERT(ids.first == reverseComplement(ids.second));
    BOOST_ASSERT(ids.second == reverseComplement(ids.first));

    return ids;
}

/**
 * Returns the id of a superpath consisting of a gap of the given length.
 */
SuperPathId
SuperGraph::gapPath(int64_t pLen)
{
    pair<SuperPathId, SuperPathId> ids = allocRcIds();
    uint64_t fd = ids.first.value();
    uint64_t rc = ids.second.value();
    SuperPath::Segment s = SuperPath::gapSeg(pLen);

    SuperPath::Segments& fdSegs(mSegs[fd]);
    SuperPath::Segments& rcSegs(mSegs[rc]);

    fdSegs = SuperPath::Segments(1, s);
    rcSegs = SuperPath::Segments(1, s);
    
    mCount += 2;

    // cerr << "gapPath " << ids.first.value() << '\n';

    return ids.first;
}

/**
 * Erase a superpath and its reverse complement.
 */
void 
SuperGraph::erase(const SuperPathId& pId)
{
    // cerr << "erase " << pId.value() << '\n';
    // The rc id can only be found before the path is deleted!
    uint64_t rcId = mRCs[pId.value()];
    halfErase(pId);

    if (rcId != pId.value())
    {
        halfErase(SuperPathId(rcId));
    }
}

/**
 * Erase a superpath but not its reverse complement.
 */
void 
SuperGraph::halfErase(const SuperPathId& pId)
{
    // cerr << "halfErase " << pId.value() << "\n";
    uint64_t id = pId.value();
    BOOST_ASSERT(id < mRCs.size());

    // Remove from node->path map
    // NOTE: This must occur before the path's segments are cleared!
    if (!isGap(pId))
    {
        Node n(start(pId));
        unordered_map<Node, SuperPathIds>::iterator i(mSucc.find(n));
        BOOST_ASSERT(i != mSucc.end());
        SuperPathIds& ids(i->second);
        SuperPathIds::iterator j(find(ids.begin(), ids.end(), pId));
        BOOST_ASSERT(j != ids.end());
        ids.erase(j);
    }

    // Clear segments.
    SuperPath::Segments empty;
    mSegs[id].swap(empty);

    // Free the SuperPathId
    freeId(pId);
    mCount--;
}

/**
 * The next usable SuperPathId.
 */
SuperPathId
SuperGraph::allocId()
{
    uint64_t i = mNextId;
    mNextId = mRCs[i];
    if (mNextId == invalidSuperPathId)
    {
        grow();
        mNextId = mRCs.size() - 1;
    }
    
    return SuperPathId(i);
}

/**
 * Frees the given SuperPathId for future use.
 */
void
SuperGraph::freeId(SuperPathId pId)
{
    uint64_t i = pId.value();
    BOOST_ASSERT(i < mRCs.size());

    mRCs[i] = mNextId;
    mNextId = i;
}

/**
 * Make room for another SuperPathId.
 */
void
SuperGraph::grow()
{
    mRCs.resize(mRCs.size() + 1);
    mSegs.resize(mSegs.size() + 1);
    mRCs.back() = invalidSuperPathId;
}


SuperGraph::SuperGraph(const std::string& pBaseName, FileFactory& pFactory)
    : mEntries(pBaseName + "-entries", pFactory),
      mNextId(mEntries.count()),
      mCount(mEntries.count()),
      mSegs(),
      mRCs()
{
      mSegs.reserve(mNextId + 1);
      mRCs.reserve(mNextId + 1);
}

