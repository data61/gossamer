// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdScaffold.hh"

#include "LineSource.hh"
#include "Debug.hh"
#include "EdgeIndex.hh"
#include "EntryEdgeSet.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "ScaffoldGraph.hh"
#include "SuperGraph.hh"
#include "SuperPathId.hh"
#include "Timer.hh"

#include <iostream>
#include <string>
#include <unordered_map>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace   // anonymous
{
    class PrefixVis
    {
    public:
 
        const uint64_t stepsLeft() const
        {
            return mStepsLeft;
        }

        const SmallBaseVector& getVector() const
        {
            return mVec;
        }

        void restart(uint64_t pExtraBases)
        {
            mExtraBases = pExtraBases;
        }

        uint64_t operator()(Graph::Edge pEdge, const Gossamer::rank_type& pRank)
        {
            if (mStepsLeft == 0)
            {
                return false;
            }

            // cerr << kmerToString(mGraph.K() + 1, pEdge.value()) << '\n';

            Gossamer::position_type p(pEdge.value());
            if (mExtraBases)
            {
                const uint64_t K = mGraph.K();
                const uint64_t rho = K + 1;
                p.reverse();
                // p >>= std::numeric_limits<Gossamer::position_type>::digits - 2 * rho;
                uint64_t shift = std::numeric_limits<Gossamer::position_type>::digits - 2 * rho;
                shift += 2 * (K - mExtraBases);
                p >>= shift;
                for (uint64_t i = 0; i < mExtraBases && mStepsLeft; ++i)
                {
                    mVec.push_back(p & 3);
                    p >>= 2;
                    --mStepsLeft;
                }
                mExtraBases = 0;
                if (mStepsLeft == 0)
                {
                    return false;
                }
            }
            
            mVec.push_back(p & 3);
            return --mStepsLeft;
        }

        PrefixVis(const Graph& pGraph, uint64_t pMaxSteps)
            : mGraph(pGraph), mStepsLeft(pMaxSteps), mExtraBases(mGraph.K()), mVec()
        {
        }

    private:
    
        const Graph& mGraph;
        uint64_t mStepsLeft;
        uint64_t mExtraBases;
        SmallBaseVector mVec;
    };

    void getPrefix(const Graph& pG, const SuperGraph& pSG, SuperPathId pId, 
                   uint64_t pBases, SmallBaseVector& pVec)
    {
        PrefixVis vis(pG, pBases);
        const SuperPath& p(pSG[pId]);
        const SuperPath::Segments& segs(p.segments());
        for (uint64_t i = 0; i < segs.size() && vis.stepsLeft(); ++i)
        {
            const SuperPath::Segment seg(segs[i]);
            if (seg.isGap())
            {
                vis.restart(seg.gap());
                continue;
            }
            EntryEdgeSet::Edge e(pSG.entries().select(seg));
            pG.linearPath(Graph::Edge(e.value()), vis);
        }

        pVec = vis.getVector();
    }

    void getSuffix(const Graph& pG, const SuperGraph& pSG, SuperPathId pId, 
                   uint64_t pBases, SmallBaseVector& pVec)
    {
        SuperPathId rcId(pSG.reverseComplement(pId));
        SmallBaseVector vecRc;
        pVec.clear();
        getPrefix(pG, pSG, rcId, pBases, vecRc);
        vecRc.reverseComplement(pVec);
    }

    // Find the best alignment of the end of pA with the start of pB.
    bool alignEnds(const SmallBaseVector& pA, const SmallBaseVector& pB, int64_t pEst, int64_t& pAln)
    {
        typedef std::unordered_map<Gossamer::position_type, vector<int64_t> > OfsMap;
        const int64_t lenA = pA.size();
        OfsMap ofs;
        const int64_t K = 7;
        for (int64_t i = 0; i < lenA - K + 1; ++i)
        {
            int64_t of = i - lenA;
            Gossamer::position_type kmer(pA.kmer(K, i));
            ofs[kmer].push_back(of);
            // cerr << kmerToString(K, kmer) << '\t' << of << '\n';
        }

        std::unordered_map<int64_t, uint64_t> alns;
        const int64_t lenB = pB.size();
        for (int64_t i = 0; i < lenB - K + 1; ++i)
        {
            Gossamer::position_type kmer(pB.kmer(K, i));
            OfsMap::iterator it = ofs.find(kmer);
            if (it == ofs.end())
            {
                continue;
            }
            const vector<int64_t>& of(it->second);
            for (vector<int64_t>::const_iterator 
                 j = of.begin(); j != of.end(); ++j)
            {
                const int64_t aln = *j - int64_t(i);
                ++alns[aln];
            }
        }

        // Eliminate poorly-supported alignments.
        // For alignment -a with we require at least 
        //      (a - K + 1) / 2 
        // hits. i.e. at least half of the kmers in the match must be present.
        for (std::unordered_map<int64_t, uint64_t>::iterator
             i = alns.begin(); i != alns.end(); )
        {
            int64_t a = -i->first;
            int64_t h = i->second;
            if (h >= (a - int64_t(K) + 1) / 2)
            {
                ++i;
            }
            else
            {
                i = alns.erase(i);
            }
        }

        if (alns.empty())
        {
            return false;
        }

        // Pick the remaining alignment that's closest to the estimate.
        std::unordered_map<int64_t, uint64_t>::const_iterator i = alns.begin();
        int64_t aln = i->first;
        int64_t minDiff = llabs(aln - pEst);
        ++i;
        for (; i != alns.end(); ++i)
        {
            int64_t diff = llabs(i->first - pEst);
            if (diff < minDiff)
            {
                aln = i->first;
                minDiff = diff;
            }
        }

        pAln = aln;
        return true;
    }

    typedef std::unordered_map<SuperPathId, int64_t> DistMap;
    typedef std::multimap<int64_t, SuperPathId> InvDistMap;

    typedef boost::tuple<double, SuperPathId, int64_t> QueueEntry;

    struct QueueEntryLt
    {
        bool operator()(const QueueEntry& pX, const QueueEntry pY) const
        {
            if (pX.get<0>() < pY.get<0>())
            {
                return true;
            }
            else if (pX.get<0>() > pY.get<0>())
            {
                return false;
            }

            if (pX.get<1>() < pY.get<1>())
            {
                return true;
            }
            else if (pX.get<1>() > pY.get<1>())
            {
                return false;
            }

            if (pX.get<2>() < pY.get<2>())
            {
                return true;
            }
            else if (pX.get<2>() > pY.get<2>())
            {
                return false;
            }

            return false;
        }

    };


    typedef priority_queue<QueueEntry, vector<QueueEntry>, QueueEntryLt > Queue;
 
    // TODO: Priority should be a function of:
    //  - count - favour more links
    //  - distance - favour closer contigs
    //  - size - favour longer contigs
    // currently: p = c
    // idea: p = c^2 + s - d
    void enqueue(const SuperGraph& pSg, const ScaffoldGraph& pScaf, const DistMap& pSeen, 
                 Queue& pQueue, SuperPathId pNode, int64_t pPos)
    {
        const ScaffoldGraph::Edges& froms(pScaf.getFroms(pNode));
        for (ScaffoldGraph::Edges::const_iterator i = froms.begin(); i != froms.end(); ++i)
        {
            SuperPathId n = i->get<0>();
            if (!pSeen.count(n))
            {
                int64_t size = pSg.baseSize(n);
                int64_t gap = i->get<1>();
                int64_t pos = pPos - (gap + size);
                int64_t count = i->get<2>();
                // double prio = count*count + size - gap;
                double prio = count;
                pQueue.push(QueueEntry(prio, n, pos));
            }
        }

        const ScaffoldGraph::Edges& tos(pScaf.getTos(pNode));
        const int64_t endPos = pPos + pSg.baseSize(pNode);
        for (ScaffoldGraph::Edges::const_iterator i = tos.begin(); i != tos.end(); ++i)
        {
            SuperPathId n = i->get<0>();
            if (!pSeen.count(n))
            {
                // int64_t size = pSg.baseSize(n);
                int64_t gap = i->get<1>();
                int64_t pos = endPos + gap;
                int64_t count = i->get<2>();
                // double prio = count*count + size - gap;
                double prio = count;
                pQueue.push(QueueEntry(prio, n, pos));
            }
        }
    }

    // True if x is in the range [x1, x2].
    bool between(int64_t x, int64_t x1, int64_t x2)
    {
        return x >= x1 && x <= x2;
    }

    // TODO: Extend this so that we can optionally ignore the n least supported links, as
    // a kind of ad hoc error correction.
    bool calculateBounds(const SuperGraph& pSg, const ScaffoldGraph& pScaf, const DistMap& pDist,
                         SuperPathId pNode, int64_t& pMinPos, int64_t& pMaxPos)
    {
        const int64_t nodeSize = pSg.baseSize(pNode);
        const ScaffoldGraph::Edges& froms(pScaf.getFroms(pNode));
        const ScaffoldGraph::Edges& tos(pScaf.getTos(pNode));

        // Calculate bounds for pos.
        int64_t posMin = numeric_limits<int64_t>::min();
        int64_t posMax = numeric_limits<int64_t>::max();
        bool constrained = false;
        for (ScaffoldGraph::Edges::const_iterator i = froms.begin(); i != froms.end(); ++i)
        {
            SuperPathId n(i->get<0>());
            DistMap::const_iterator j = pDist.find(n);
            if (j != pDist.end())
            {
                constrained = true;
                int64_t halfRange = i->get<3>() / 2;
                const int64_t edgePos = j->second + pSg.baseSize(n) + i->get<1>();
                posMin = max(posMin, edgePos - halfRange);
                posMax = min(posMax, edgePos + halfRange);
            }
        }

        for (ScaffoldGraph::Edges::const_iterator i = tos.begin(); i != tos.end(); ++i)
        {
            SuperPathId n(i->get<0>());
            DistMap::const_iterator j = pDist.find(n);
            if (j != pDist.end())
            {
                constrained = true;
                int64_t halfRange = i->get<3>() / 2;
                const int64_t edgePos = j->second - (i->get<1>() + nodeSize);
                posMin = max(posMin, edgePos - halfRange);
                posMax = min(posMax, edgePos + halfRange);
            }
        }

        pMinPos = posMin;
        pMaxPos = posMax;
        return constrained;
    }

    enum Placement { Unconstrained, Unplaced, Placed };

    // Set pPlace to the position nearest to pTarget, at which pNode can be placed subject 
    // to the edges in pScaf, and the placements in pDist.
    // Returns false if pNode cannot be placed without violating any constraints.
    Placement placeNear(const SuperGraph& pSg, const ScaffoldGraph& pScaf, const DistMap& pDist,
                        SuperPathId pNode, int64_t pTarget, int64_t& pPlace)
    {
        int64_t posMin;
        int64_t posMax;
        bool constrained = calculateBounds(pSg, pScaf, pDist, pNode, posMin, posMax);
        if (!constrained)
        {
            return Unconstrained;
        }

        if (posMin > posMax)
        {
            // cerr << "[" << posMin << ", " << posMax << "]\t" << pTarget << "\n";
            return Unplaced;
        }

        pPlace =   pTarget < posMin ? posMin 
                 : pTarget > posMax ? posMax
                 : pTarget;
        return Placed;
    }

    Placement placeFar(const SuperGraph& pSg, const ScaffoldGraph& pScaf, const DistMap& pDist,
                       SuperPathId pNode, int64_t& pPlace)
    {
        int64_t posMin, posMax;
        if (!calculateBounds(pSg, pScaf, pDist, pNode, posMin, posMax))
        {
            return Unconstrained;
        }

        pPlace = posMax;
        return Placed;
    }

    // Set pPlace to the midpoint allowed by the edge constraints and the placements in pDist.
    // Returns false if pNode cannot be placed without violating any constraints.
    Placement placeMid(const SuperGraph& pSg, const ScaffoldGraph& pScaf, const DistMap& pDist,
                       SuperPathId pNode, int64_t& pPlace)
    {
        int64_t posMin, posMax;
        if (!calculateBounds(pSg, pScaf, pDist, pNode, posMin, posMax))
        {
            return Unconstrained;
        }

        pPlace = (posMax + posMin) / 2;
        return Placed;
    }

    void dumpInvDistMap(const SuperGraph& pSg, ostream& pOut, const InvDistMap& pMap)
    {
        for (InvDistMap::const_iterator i = pMap.begin(); i != pMap.end(); ++i)
        {
            const int64_t sz(pSg.baseSize(i->second));
            const int64_t a(i->first);
            const int64_t b(a + sz);
            SuperPathId n(i->second);
            SuperPathId nRc(pSg.reverseComplement(i->second));
            pOut << a << '\t' << b << '\t' << n.value() << " (" << nRc.value() << ")\t" << sz << '\n';
        }
    }


    void invertDistanceMap(const DistMap& pDistMap, InvDistMap& pInvDistMap)
    {
        pInvDistMap.clear();
        for (DistMap::const_iterator i = pDistMap.begin(); i != pDistMap.end(); ++i)
        {
            pInvDistMap.insert(make_pair(i->second, i->first));
        }
    }


    // Extracts a linear sequence of nodes from the scaffold graph, given the set of
    // available nodes.
    // If false is returned, no viable linear sequence could be found.
    bool linearise(const Graph& pG, const SuperGraph& pSg, const ScaffoldGraph& pScaf, 
                   const std::unordered_set<SuperPathId>& pAvail, InvDistMap& pInvDistMap)
    {
        // Find a starting terminal. 
        SuperPathId start(0);
        bool foundStart = false;
        for (std::unordered_set<SuperPathId>::const_iterator i = pAvail.begin(); i != pAvail.end(); ++i)
        {
            const ScaffoldGraph::Edges& tos(pScaf.getTos(*i));
            const ScaffoldGraph::Edges& froms(pScaf.getFroms(*i));
            bool outs = false;
            for (ScaffoldGraph::Edges::const_iterator j = tos.begin(); j != tos.end(); ++j)
            {
                if (pAvail.count(j->get<0>()))
                {
                    outs = true;
                    break;
                }
            }
            if (outs)
            {
                bool ins = false;
                for (ScaffoldGraph::Edges::const_iterator j = froms.begin(); j != froms.end(); ++j)
                {
                    if (pAvail.count(j->get<0>()))
                    {
                        ins = true;
                        break;
                    }
                }
                if (!ins)
                {
                    foundStart = true;
                    start = *i;
                    break;
                }
            }
        }
        if (!foundStart)
        {
            // No starting terminal nodes remaining (could be due to a cycle.)
            return false;
        }

        DistMap ord;
        ord.insert(make_pair(start, 0));

        Queue q;
        enqueue(pSg, pScaf, ord, q, start, 0);
        while (!q.empty())
        {
            QueueEntry qe = q.top();
            q.pop();
            const SuperPathId n = qe.get<1>();
            const SuperPathId nRc = pSg.reverseComplement(n);
            const int64_t d = qe.get<2>();
            if (!ord.count(n) && !ord.count(nRc) && pAvail.count(n))
            {
                ord.insert(make_pair(n, d));
                enqueue(pSg, pScaf, ord, q, n, d);
            }
        }

        // Algorithm
        //  1. Consider contigs in distance order.
        //  2. Place each contig as near to its (linear) predecessor as possible
        //      a) start from the estimated position
        //      b) calculate the distance from each (graph) neighbour that's already been placed
        //      c) place the contig such that the above distances are minimised.
        //      d) if no placement is possible, skip it
        //  3. Relax the edges towards their expected lengths, expanding them, by moving 
        //     apart contigs, in turn, from the end back to the begining.
        
        // Invert the distance map
        InvDistMap ids;
        for (DistMap::const_iterator i = ord.begin(); i != ord.end(); ++i)
        {
            ids.insert(make_pair(i->second, i->first));
        }
        // cerr << "\nInitial ordering\n";
        // dumpInvDistMap(pSg, cerr, ids);
        {
            DistMap ds;
            InvDistMap::const_iterator i = ids.begin();
            int64_t x = i->first;
            SuperPathId n = i->second;
            int64_t nSize = pSg.baseSize(n);
            int64_t end = x + nSize;
            ds.insert(make_pair(n, x));

            for (++i; i != ids.end(); ++i)
            {
                x = i->first;
                n = i->second;
                nSize = pSg.baseSize(n);
  
                int64_t pos = 0;
                Placement p = placeNear(pSg, pScaf, ds, n, end, pos);
                // cerr << x << '\t' << n.value() << '\t' << p << '\t' << pos << '\n';
                if (p == Placed)
                {
                    ds.insert(make_pair(n, pos));
                    end = pos + nSize;
                }
                else if (p == Unconstrained)
                {
                    // TODO: we ought to have some way to back fill these guys once a graph neighbour is added!
                }
                else
                {
                    // Skip this node.
                }
            }

            // Relax distances.
            for (uint64_t j = 0; j < 5; ++j)
            {
                for (DistMap::iterator i = ds.begin(); i != ds.end(); ++i)
                {
                    int64_t pos = 0;
                    Placement p = placeMid(pSg, pScaf, ds, i->first, pos);
                    (void)(&p);
                    BOOST_ASSERT(p == Placed);
                    i->second = pos;
                }
            }

            invertDistanceMap(ds, ids);
            // cerr << "\nPre-aligned placement\n";
            // dumpInvDistMap(pSg, cerr, ids);

            // Check for alignment between overlapping contigs.
            ds.clear();
            InvDistMap::iterator cur = ids.begin();
            InvDistMap::iterator next = cur;
            ++next;
            const int64_t K(pG.K());
            int64_t move = 0;
            for (; next != ids.end(); ++cur, ++next)
            {
                ds.insert(make_pair(cur->second, cur->first + move));
                SmallBaseVector curVec, nextVec;
                getSuffix(pG, pSg, cur->second, K, curVec);
                getPrefix(pG, pSg, next->second, K, nextVec);
                const int64_t curEnd = cur->first + pSg.baseSize(cur->second);
                const int64_t estGap = next->first - curEnd;
                int64_t aln = 0;
                if (estGap < 0)
                {
                    if (!alignEnds(curVec, nextVec, estGap, aln) || aln < -K)
                    {
                        // No plausible alignment - have contigs abut.
                        move += -estGap;
                    }
                    else
                    {
                        move += aln - estGap;
                    }
                }
            }
            ds.insert(make_pair(cur->second, cur->first + move));
            invertDistanceMap(ds, ids);

            // cerr << "\nFinal placement\n";
            // dumpInvDistMap(pSg, cerr, ids);
        }

        pInvDistMap.swap(ids);
        return true;
    }

}   // namespace anonymous

void
GossCmdScaffold::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    auto sgP = SuperGraph::read(mIn, fac);
    SuperGraph& sg(*sgP);

    auto scafP = ScaffoldGraph::read(mIn, fac, mMinLinkCount);
    ScaffoldGraph& scaf(*scafP);

/*
    log(info, "dropping mirrored components");
    dropMirrored(sg, scaf, log);
*/

    log(info, "combining reverse complements");
    scaf.mergeRcs(sg);

/*
    log(info, "eliminating structurally duplicated nodes");
    elimNonUnique(sg, scaf, log);
*/

    // scaf.write("scaf.out", fac);

#ifdef EXTRACT
    if (mExtract != uint64_t(-1))
    {
        std::unordered_set<SuperPathId> cmp;
        scaf.getConnectedNodes(SuperPathId(mExtract), cmp);
        cerr << "component containing " << mExtract << ':';
        for (std::unordered_set<SuperPathId>::const_iterator i = cmp.begin(); i != cmp.end(); ++i)
        {
            cerr << ' ' << (*i).value() << "(" << sg.reverseComplement(*i).value() << ")";
        }
        cerr << '\n';

        set<pair<SuperPathId, SuperPathId> > seen;
        for (std::unordered_set<SuperPathId>::const_iterator i = cmp.begin(); i != cmp.end(); ++i)
        {
            SuperPathId n(*i);
            ScaffoldGraph::Node& m(scaf.mNodes[n]);
            for (ScaffoldGraph::Edges::const_iterator j = m.mFrom.begin(); j != m.mFrom.end(); ++j)
            {
                SuperPathId f = j->get<0>();
                int64_t g = j->get<1>();
                uint64_t c = j->get<2>();
                if (seen.insert(make_pair(f, n)).second)
                {
                    cerr << f.value() << " -> " << n.value() 
                         << " [label = \"" << g << " (" << c << ")\"];\n";
                }
            }
            for (ScaffoldGraph::Edges::const_iterator j = m.mTo.begin(); j != m.mTo.end(); ++j)
            {
                SuperPathId t = j->get<0>();
                int64_t g = j->get<1>();
                uint64_t c = j->get<2>();
                if (seen.insert(make_pair(n, t)).second)
                {
                    cerr << n.value() << " -> " << t.value() 
                         << " [label = \"" << g << " (" << c << ")\"];\n";
                }
            }
        }
        for (std::unordered_set<SuperPathId>::const_iterator i = cmp.begin(); i != cmp.end(); ++i)
        {
            SuperPathId n(*i);
            uint64_t sz(sg.baseSize(n));
            cerr << n.value() << " [label = \"" << n.value() << " (" << sz << ")\"];\n";
        }

        return;
    }
#endif

    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    
    // Consider breaking the scaffold graph into individual components:
    // these can then be processed concurrently.
#ifdef DO_DOT
    scaf.dumpDot(cout, sg);
#endif
    std::unordered_set<SuperPathId> left;
    scaf.getNodes(left);
    InvDistMap ids;
    while (!left.empty())
    {
        if (!linearise(g, sg, scaf, left, ids))
        {
            // Nothing extracted.
            break;
        }

        for (InvDistMap::const_iterator i = ids.begin(); i != ids.end(); ++i)
        {
            left.erase(i->second);
            left.erase(sg.reverseComplement(i->second));
        }

#ifdef DO_DOT
        {
            for (InvDistMap::const_iterator i = ids.begin(); i != ids.end(); ++i)
            {
                cout << i->second.value() << " [fillcolor = \"grey\" style = \"filled\"];\n";
                cout << sg.reverseComplement(i->second).value() << " [fillcolor = \"grey\" style = \"filled\"];\n";
            }
            InvDistMap::const_iterator i = ids.begin();
            SuperPathId a = i->second;
            ++i;
            SuperPathId b(0);
            while (i != ids.end())
            {
                b = i->second;
                cout << a.value() << " -> " << b.value()
                     << " [color=\"red\" style=\"dotted\"];\n";
                ++i;
                a = b;
            }
        }
#endif

        if (ids.size() < 2)
        {
            continue;
        }

        InvDistMap::const_iterator i = ids.begin();
        SuperPathId cur(i->second);
        int64_t curEnd(i->first + sg.baseSize(cur));
        uint64_t len = 1;
        for (++i; i != ids.end(); ++i)
        {
            const SuperPathId next(i->second);
            BOOST_ASSERT(!sg.isGap(cur));
            BOOST_ASSERT(!sg.isGap(next));
            const int64_t nextPos(i->first);
            // const int64_t curEnd = curPos + sg.baseSize(cur);
            int64_t gap = nextPos - curEnd;
            curEnd = nextPos + sg.baseSize(next);
            /*
            if (gap >= 1000)
            {
                LOG(log, info) << "built " << len << " contig scaffold of " << sg.baseSize(cur) << " bases";
                cur = next;
                len = 1;
                continue;
            }
            */

            ++len;
            vector<SuperPathId> p;
            p.reserve(3);
            p.push_back(cur);
            p.push_back(sg.gapPath(gap));
            p.push_back(next);
            pair<SuperPathId, SuperPathId> ns = sg.link(p);
            sg.erase(p[0]);
            sg.erase(p[1]);
            sg.erase(p[2]);
            cur = ns.first;
        }
        LOG(log, info) << "built " << len << " contig scaffold of " << sg.baseSize(cur) << " bases";
    }
    sg.write(mIn, fac);

    log(info, "cleaning up scaffold files");
    ScaffoldGraph::removeScafFiles(pCxt, mIn);

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryScaffold::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    uint64_t c = 10;
    chk.getOptional("min-link-count", c);

    uint64_t x = -1;
#ifdef EXTRACT
    chk.getOptional("extract", x);
#endif

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdScaffold(in, c, x));
}

GossCmdFactoryScaffold::GossCmdFactoryScaffold()
    : GossCmdFactory("apply a scaffold to a supergraph")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("min-link-count");
 
#ifdef EXTRACT
    mSpecificOptions.addOpt<uint64_t>("extract", "", "");
#endif
}

