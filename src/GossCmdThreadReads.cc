// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdThreadReads.hh"

#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "Debug.hh"
#include "EdgeIndex.hh"
#include "EntryEdgeSet.hh"
#include "EstimateGraphStatistics.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "KmerAligner.hh"
#include "LineParser.hh"
#include "PairLinker.hh"
#include "ReadSequenceFileSequence.hh"
#include "ReverseComplementAdapter.hh"
#include "ScaffoldGraph.hh"
#include "SuperGraph.hh"
#include "SuperPathId.hh"
#include "Timer.hh"

#include <algorithm>
#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <thread>
#include <unordered_set>
#include <math.h>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef pair<SuperPathId,SuperPathId> Link;

// std::hash
namespace std {
    template<>
    struct hash<Link>
    {
        std::size_t operator()(const Link& pValue) const
        {
            BigInteger<2> l(pValue.first.value());
            BigInteger<2> r(pValue.second.value());
            l <<= 64;
            l += r;
            return l.hash();
        }
    };
}

namespace // anonymous
{

Debug discardUpdates("drop-updated-supergraph", "after read threading, don't save the resulting supergraph.");
Debug showLinkStats("show-links", "show the links used for establishing new paths");
Debug writeLinks("write-links", "write links to a file");
Debug readLinks("read-links", "read links from a file, rather than recalculating");

typedef vector<string> strings;

typedef uint32_t LinkCount;
typedef std::unordered_map<Link, LinkCount> LinkMap;

string label(const GossRead& pRead)
{
    string r = pRead.print();
    size_t n = r.find_first_of('\n');
    return string(r, 0, n);
}

struct BiLinkMap
{
    typedef std::unordered_map<SuperPathId, vector<SuperPathId> > UniLinkMap;
    typedef std::unordered_map<Link, uint32_t> LinkCountMap;
    typedef std::unordered_map<Link, uint32_t> LinkGapMap;

    void add(SuperPathId a, SuperPathId b, uint32_t g = 0, LinkCount c = 1)
    {
        add(make_pair(a, b), g, c);
    }

    void add(const Link& l, uint32_t g = 0, LinkCount c = 1)
    {
        SuperPathId a(l.first);
        SuperPathId b(l.second);
        LinkCountMap::iterator i(mCounts.find(l));
        LinkGapMap::iterator j(mGaps.find(l));
        if (i == mCounts.end())
        {
            mLhs[a].push_back(b);
            mRhs[b].push_back(a);
            mCounts[l] = c;
            mGaps[l] = g;
        }
        else
        {
            i->second += c;
            j->second += g;
        }
    }

    void add(const BiLinkMap& links)
    {
        for (LinkCountMap::const_iterator
             i = links.mCounts.begin(); i != links.mCounts.end(); ++i)
        {
            LinkGapMap::const_iterator j(links.mGaps.find(i->first));
            const uint32_t g = j == links.mGaps.end() ? 0 : j->second;
            add(i->first, g, i->second);
        }
    }

    void swap(BiLinkMap& links)
    {
        mLhs.swap(links.mLhs);
        mRhs.swap(links.mRhs);
        mCounts.swap(links.mCounts);
        mGaps.swap(links.mGaps);
    }

    void removeCounts()
    {
        mCounts.clear();
    }

    LinkCount count(SuperPathId a, SuperPathId b) const
    {
        LinkCountMap::const_iterator i(mCounts.find(Link(a, b)));
        if (i != mCounts.end())
        {
            return i->second;
        }
        return 0;
    }

    uint32_t avgGap(SuperPathId a, SuperPathId b) const
    {
        LinkGapMap::const_iterator i(mGaps.find(Link(a, b)));
        if (i != mGaps.end())
        {
            // cerr << "avgGap\t" << a.value() << '\t' << b.value() << '\t' << i->second << '\t' << count(a, b) << '\t' << (i->second / count(a, b)) << '\n';
            return i->second / count(a, b);
        }
        return 0;
    }

    void dump(ostream& pOut) const
    {
        for (UniLinkMap::const_iterator i = mLhs.begin(); i != mLhs.end(); ++i)
        {
            SuperPathId a(i->first);
            for (vector<SuperPathId>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
            {
                SuperPathId b(*j);
                uint32_t c = mCounts.find(Link(a,b))->second;
                uint32_t g = mGaps.find(Link(a,b))->second;
                pOut << a.value() << '\t' << b.value() << '\t' << c << '\t' << g << '\n';
            }
        }
    }

    BiLinkMap() 
        : mLhs(), mRhs(), mCounts()
    {
    }

    void removeCount(SuperPathId a, SuperPathId b)
    {
        mCounts.erase(make_pair(a, b));
    }

    UniLinkMap mLhs;
    UniLinkMap mRhs;
    LinkCountMap mCounts;
    LinkGapMap mGaps;
};

struct SimpleBiLinkMap
{
    typedef std::unordered_map<SuperPathId, SuperPathId> UniLinkMap;

    void add(SuperPathId a, SuperPathId b)
    {
        mLhs.insert(Link(a, b));
        mRhs.insert(Link(b, a));
    }

    void add(SuperPathId a, SuperPathId b, uint32_t g)
    {
        add(a, b);
        mGaps[Link(a,b)] = g;
    }

    // Substitute n for a as an lhs.
    
    // TODO: Update mGaps, too!

    void substLhs(SuperPathId n, SuperPathId a)
    {
//        cerr << "lhs [" << n.value() << "/" << a.value() << "]\n";
        UniLinkMap::iterator i(mLhs.find(a));
        if (i != mLhs.end())
        {
            SuperPathId b = i->second;
            UniLinkMap::iterator j(mRhs.find(b));
            BOOST_ASSERT(j != mRhs.end());
            BOOST_ASSERT(j->second == a);
            mLhs.erase(i);
            mRhs.erase(j);
            mLhs.insert(make_pair(n, b));
            mRhs.insert(make_pair(b, n));

            std::unordered_map<Link, uint32_t>::iterator k(mGaps.find(Link(a, b)));
            BOOST_ASSERT(k != mGaps.end());
            uint32_t g = k->second;
            mGaps.erase(k);
            mGaps.insert(make_pair(Link(n, b), g));
        }
    }

    // Substitute n for b as an rhs.

    // TODO: Update mGaps, too!

    void substRhs(SuperPathId n, SuperPathId b)
    {
        // cerr << "rhs [" << n.value() << "/" << b.value() << "]\n";
        UniLinkMap::iterator j(mRhs.find(b));
        if (j != mRhs.end())
        {
            SuperPathId a = j->second;
            UniLinkMap::iterator i(mLhs.find(a));
            BOOST_ASSERT(i != mLhs.end());
            BOOST_ASSERT(i->second == b);
            mLhs.erase(i);
            mRhs.erase(j);
            mLhs.insert(make_pair(a, n));
            mRhs.insert(make_pair(n, a));

            std::unordered_map<Link, uint32_t>::iterator k(mGaps.find(Link(a, b)));
            BOOST_ASSERT(k != mGaps.end());
            uint32_t g = k->second;
            mGaps.erase(k);
            mGaps.insert(make_pair(Link(a, n), g));
        }
    }

    void eraseLhs(SuperPathId a)
    {
        // cerr << "eraseLhs " << a.value() << '\n';
        UniLinkMap::iterator i(mLhs.find(a));
        if (i != mLhs.end())
        {
            SuperPathId b(i->second);
            UniLinkMap::iterator j(mRhs.find(b));
            BOOST_ASSERT(j != mRhs.end());
            BOOST_ASSERT(j->second == a);
            mLhs.erase(i);
            mRhs.erase(j);

            std::unordered_map<Link, uint32_t>::iterator k(mGaps.find(Link(a, b)));
            BOOST_ASSERT(k != mGaps.end());
            mGaps.erase(k);
        }
    }

    void eraseRhs(SuperPathId b)
    {
        UniLinkMap::iterator j(mRhs.find(b));
        if (j != mRhs.end())
        {
            SuperPathId a(j->second);
            eraseLhs(a);
        }
    }

    void verify()
    {
        for (UniLinkMap::const_iterator i(mLhs.begin()); i != mLhs.end(); ++i)
        {
            SuperPathId a(i->first);
            SuperPathId b(i->second);
            UniLinkMap::const_iterator j(mRhs.find(b));
            BOOST_ASSERT(j != mRhs.end());
            BOOST_ASSERT(j->second == a);
        }

        for (UniLinkMap::const_iterator i(mRhs.begin()); i != mRhs.end(); ++i)
        {
            SuperPathId b(i->first);
            SuperPathId a(i->second);
            UniLinkMap::const_iterator j(mLhs.find(a));
            BOOST_ASSERT(j != mLhs.end());
            BOOST_ASSERT(j->second == b);
        }
    }

    UniLinkMap mLhs;
    UniLinkMap mRhs;

    std::unordered_map<Link, uint32_t> mGaps;
};

typedef vector<SuperPathId> Path;
typedef vector<Path> Paths;

typedef vector<Gossamer::position_type> Kmers;

class ReadLinker
{
public:

    const BiLinkMap& getLinks() const
    {
        return mLinks;
    }

    void push_back(GossReadPtr pRead)
    {
        bool primed(false);
        bool link(false);
        SuperPathId a(0);
        SuperPathId b(0);
        uint32_t gap = 0;

        stringstream ss;
        string l(label(*pRead));
        ss << l;
        for (GossRead::Iterator i(*pRead, mRho); i.valid(); ++i)
        {
            SuperPathId id(0);
            if (   mAligner(i.kmer(), id))
                // && mUCache.unique(id))
            {
                if (mUCache.unique(id))
                {
                    if (primed && id != b)
                    {
                        ss << " (" << gap << ")";
                    }
                    ss << ' ' << id.value();
                    if (!primed)
                    {
                        b = id;
                        gap = 0;
                        primed = true;
                    }
                    else if (id != b)
                    {
                        a = b;
                        b = id;
                        // ss << l << '\t' << a.value() << '\t' << b.value() << '\n';
                        mLinks.add(a, b, gap);
                        gap = 0;
                        link = true;
                    }
                }
                else
                {
                    gap += 1;
                    ss << " *";
                }
            }
            else
            {
                gap += 1;
                ss << " _";
            }
        }
        if (link)
        {
            // cerr << ss.str() << '\n';
        }
    }

    ReadLinker(const Graph& pGraph, const EntryEdgeSet& pEntryEdges, 
               const EdgeIndex& pIndex, UniquenessCache& pUCache) 
        : mRho(pGraph.K() + 1), mIndex(pIndex), mUCache(pUCache), 
          mAligner(pGraph, pEntryEdges, pIndex), mLinks()
    {
    }

private:

    const uint64_t mRho;
    const EdgeIndex& mIndex;
    UniquenessCache& mUCache;
    KmerAligner mAligner;
    BiLinkMap mLinks;
};

typedef std::shared_ptr<ReadLinker> ReadLinkerPtr;

#if 0
// Find a path from pBegin to pEnd, that does not visit any other
// unique edges, within pRadius edges.
bool
findPath(SuperGraph& pSG, const SuperPathId& pBegin, const SuperPathId& pEnd,
         uint64_t pRadius, Path& pPath)
{
    if (pBegin == pEnd)
    {
        return true;
    }

    if (pRadius == 0)
    {
        return false;
    }

    SuperGraph::Node n(pSG.end(pBegin));
    SuperGraph::SuperPathIds succs;
    pSG.successors(n, succs);
    for (uint64_t i = 0; i < succs.size(); ++i)
    {
        pPath.push_back(succs[i]);
        if (findPath(pSG, succs[i], pEnd, pRadius - 1, pPath))
        {
            return true;
        }
        pPath.pop_back();
    }
    return false;
}
#endif


#if 0
// Find the path from pBegin to pEnd, within pRadius supergraph edges, that has 
// the closest to pGap de Bruijn graph edges.
bool
findPath(SuperGraph& pSG, const SuperPathId& pBegin, const SuperPathId& pEnd,
         uint64_t pRadius, Path& pPath)
{
    if (pBegin == pEnd)
    {
        return true;
    }

    if (pRadius == 0)
    {
        return false;
    }

    SuperGraph::Node n(pSG.end(pBegin));
    SuperGraph::SuperPathIds succs;
    pSG.successors(n, succs);
    for (uint64_t i = 0; i < succs.size(); ++i)
    {
        pPath.push_back(succs[i]);
        if (findPath(pSG, succs[i], pEnd, pRadius - 1, pPath))
        {
            return true;
        }
        pPath.pop_back();
    }
    return false;
}
#endif


void
findPath(SuperGraph& pSG, const SuperPathId& pAt, const SuperPathId& pTo,
         uint64_t pStepsLeft, const uint64_t pGap, Path& pPath, uint64_t pLength, 
         vector<pair<uint64_t, Path> >& pPaths)
{
    // cerr << "findPath\t" << pAt.value() << '\t' << pTo.value() << '\t' << pStepsLeft << '\t' << pGap << '\t' << pLength << '\n';
    
    if (pAt == pTo)
    {
        // cerr << "made it\n";
        // Don't count the length of the target path.
        uint64_t len = pLength - pSG.size(pTo);
        pPaths.push_back(make_pair(len, pPath));
        return;
    }
    
    
    if (pLength > pGap * 1.5)
    {
        // cerr << "too long\n";
        return;
    }

    if (pStepsLeft == 0)
    {
        // cerr << "out of steps\n";
        return;
    }
    
    // cerr << "diving\n";
    SuperGraph::Node n(pSG.end(pAt));
    SuperGraph::SuperPathIds succs;
    pSG.successors(n, succs);
    for (uint64_t i = 0; i < succs.size(); ++i)
    {
        pPath.push_back(succs[i]);
        uint64_t len = pLength + pSG.size(succs[i]);
        findPath(pSG, succs[i], pTo, pStepsLeft - 1, pGap, pPath, len, pPaths);
        pPath.pop_back();
    }
}

bool
findPath(SuperGraph& pSG, const SuperPathId& pBegin, const SuperPathId& pEnd,
         uint32_t pGap, uint64_t pRadius, Path& pPath)
{
    if (pGap == 0)
    {
        pPath.push_back(pEnd);
        return true;
    }

    vector<pair<uint64_t, Path> > paths;
    findPath(pSG, pBegin, pEnd, pRadius, pGap, pPath, 0, paths);
    const Path* bestPath = 0;
    uint64_t bestDiff = numeric_limits<uint64_t>::max();
    for (uint64_t i = 0; i < paths.size(); ++i)
    {
        const pair<uint64_t, Path>& gpath(paths[i]);
        uint64_t diff = llabs(int64_t(pGap) - int64_t(gpath.first));
        if (diff < bestDiff)
        {
            bestDiff = diff;
            bestPath = &gpath.second;
        }
    }
    if (bestPath)
    {
        pPath = *bestPath;
        return true;
    }
    return false;
}

void
dumpLinks(ostream& pOut, const SimpleBiLinkMap& pLinks)
{
    for (SimpleBiLinkMap::UniLinkMap::const_iterator
         i = pLinks.mLhs.begin(); i != pLinks.mLhs.end(); ++i)
    {
        SuperPathId a(i->first);
        SuperPathId b(i->second);
        SimpleBiLinkMap::UniLinkMap::const_iterator j(pLinks.mRhs.find(b));
        BOOST_ASSERT(j != pLinks.mRhs.end());
        BOOST_ASSERT(j->second == a);

        uint32_t g = 0;
        std::unordered_map<Link, uint32_t>::const_iterator k(pLinks.mGaps.find(*i));
        if (k != pLinks.mGaps.end())
        {
            g = k->second;
        }
        pOut << i->first.value() << '\t' << i->second.value() << '\t' << g << '\n';
    }
}

void linearSegment(SuperGraph& pSG, SuperPathId pId, vector<SuperPathId>& pPath)
{
    SuperPathId p = pId;
    set<SuperPathId> seen;
    pPath.push_back(p);
    seen.insert(p);
    SuperGraph::Node n(pSG.end(p));
    while (   pSG.numOut(n) == 1
           && pSG.numIn(n) == 1)
    {
        p = pSG.onlyOut(n);
        if (seen.count(p))
        {
            break;
        }
        seen.insert(p);
        pPath.push_back(p);
    }

}


// Collapse any new linear segments.
uint64_t simplify(SuperGraph& pSG)
{
    uint64_t newPaths = 0;
    set<SuperPathId> seen;
    set<SuperPathId> remd;
    vector<SuperGraph::Node> ns;
    vector<SuperPathId> ids;
    vector<SuperPathId> p;
    pSG.nodes(ns);
    for (uint64_t i = 0; i < ns.size(); ++i)
    {
        ids.clear();
        remd.clear();
        pSG.successors(ns[i], ids);
        for (uint64_t j = 0; j < ids.size(); ++j)
        {
            if (!remd.count(ids[j]))
            {
                p.clear();
                seen.clear();
                linearSegment(pSG, ids[j], p);
                if (p.size() > 1)
                {
                    newPaths++;
                    pSG.link(p);
                    for (uint64_t k = 0; k < p.size(); ++k)
                    {
                        SuperPathId fd(p[k]);
                        if (!remd.count(fd))
                        {
                            SuperPathId rc(pSG.reverseComplement(fd));
                            pSG.erase(fd);
                            remd.insert(fd);
                            remd.insert(rc);
                        }
                    }
                }
            }
        }
    }   
    return newPaths;
}


} // namespace anonymous

void
GossCmdThreadReads::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    Timer t;

    if (ScaffoldGraph::existScafFiles(pCxt, mIn))
    {
        if (!mRemScaf)
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::usage_info("Cannot execute command: there are scaffold files associated with " 
                                        + mIn + ". Rerun with --delete-scaffold to remove these files and proceed.\n"));
        }
        ScaffoldGraph::removeScafFiles(pCxt, mIn);
    }

    // Infer coverage if we need to.
    uint64_t coverage = mExpectedCoverage;
    if (mInferCoverage)
    {
        log(info, "Inferring coverage");
        Graph::LazyIterator itr(mIn, fac);
        if (itr.asymmetric())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Asymmetric graphs not yet handled")
                << Gossamer::open_graph_name_info(mIn));
        }
        map<uint64_t,uint64_t> h = Graph::hist(mIn, fac);
        EstimateCoverageOnly estimator(log, h);
        if (!estimator.modelFits())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error() << Gossamer::general_error_info("Could not infer coverage."));
        }
        coverage = static_cast<uint64_t>(estimator.estimateRhomerCoverage());
        log(info, "Estimated coverage = " + lexical_cast<string>(coverage));
    }

    auto sgp = SuperGraph::read(mIn, fac);
    SuperGraph& sg(*sgp);
    const EntryEdgeSet& entries(sg.entries());

    SimpleBiLinkMap lnks;

    if (readLinks.on())
    {
        FileFactory::InHolderPtr inp(fac.in(mIn + ".links"));
        istream& in(**inp);
        while (in.good())
        {
            uint64_t l;
            uint64_t r;
            uint32_t g;
            in >> l >> r >> g;
            if (!in.good())
            {
                break;
            }
            lnks.add(SuperPathId(l), SuperPathId(r), g);
        }
    }
    else
    {
        log(info, "constructing edge index");
        GraphPtr gPtr = Graph::open(mIn, fac);
        Graph& g(*gPtr);
        if (g.asymmetric())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Asymmetric graphs not yet handled")
                << Gossamer::open_graph_name_info(mIn));
        }
        auto idxPtr = EdgeIndex::create(g, entries, sg, mCacheRate, mNumThreads, log);
        EdgeIndex& idx(*idxPtr);

        std::deque<GossReadSequence::Item> items;

        {
            GossReadSequenceFactoryPtr seqFac
                = std::make_shared<GossReadSequenceBasesFactory>();

            GossReadParserFactory lineParserFac(LineParser::create);
            for(auto& f: mLines)
            {
                items.push_back(GossReadSequence::Item(f,
                                lineParserFac, seqFac));
            }

            GossReadParserFactory fastaParserFac(FastaParser::create);
            for(auto& f: mFastas)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastaParserFac, seqFac));
            }

            GossReadParserFactory fastqParserFac(FastqParser::create);
            for(auto& f: mFastqs)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastqParserFac, seqFac));
            }
        }

        UnboundedProgressMonitor umon(log, 100000, " reads");
        LineSourceFactory lineSrcFac(BackgroundLineSource::create);
        ReadSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

        {
            BiLinkMap links;
            {
                log(info, "mapping reads.");
                UniquenessCache ucache(sg, coverage);
                BackgroundMultiConsumer<GossReadPtr> grp(128);
                vector<ReadLinkerPtr> linkers;
                for (uint64_t i = 0; i < mNumThreads; ++i)
                {
                    linkers.push_back(ReadLinkerPtr(new ReadLinker(g, entries, idx, ucache)));
                    grp.add(*linkers.back());
                }

                while (reads.valid())
                {
                    grp.push_back((*reads).clone());
                    ++reads;
                }
                grp.wait();

                log(info, "merging results.");
                for (vector<ReadLinkerPtr>::const_iterator
                     i = linkers.begin(); i != linkers.end(); ++i)
                {
                    const BiLinkMap& l((*i)->getLinks());
                    links.add(l);
                }
            }

            for (BiLinkMap::UniLinkMap::const_iterator
                 i = links.mLhs.begin(); i != links.mLhs.end(); ++i)
            {   
                // cerr << i->first.value();
                const vector<SuperPathId>& rs(i->second);
                for (vector<SuperPathId>::const_iterator j = rs.begin(); j != rs.end(); ++j)
                {
                    Link l(i->first, *j);
                    // cerr << '\t' << (*j).value() 
                    //     << " (" << links.mCounts[l] << ", " << links.mGaps[l] << ")";
                }
                // cerr << '\n';
            }

            // Filter link map.
            LOG(log, info) << "found " << links.mLhs.size() << " links";
            // cerr << "mMinLinkCount: " << mMinLinkCount << '\n';
            // cerr << "# lhs: " << links.mLhs.size() << '\n';
            {
                // Eliminate low-count links.
                BiLinkMap goodLinks;
                for (BiLinkMap::UniLinkMap::const_iterator 
                     i = links.mLhs.begin(); i != links.mLhs.end(); ++i)
                {
                    SuperPathId a(i->first);
                    const vector<SuperPathId>& bs(i->second);
                    for (vector<SuperPathId>::const_iterator j = bs.begin(); j != bs.end(); ++j)
                    {
                        SuperPathId b(*j);
                        //cerr << a.value() << '\t' << b.value() << '\t' << links.count(a, b) << '\n';
                        if (uint64_t(links.count(a, b)) < mMinLinkCount)
                        {
                            continue;
                        }
                        goodLinks.add(a, b, links.avgGap(a, b));
                    }
                }
                links.swap(goodLinks);
            }

            //cerr << "# lhs: " << links.mLhs.size() << '\n';
            {
                // Eliminate cases where the lhs doesn't uniquely determine the rhs.
                BiLinkMap goodLinks;
                for (BiLinkMap::UniLinkMap::const_iterator 
                     i = links.mLhs.begin(); i != links.mLhs.end(); ++i)
                {
                    SuperPathId a(i->first);
                    const vector<SuperPathId>& rs(i->second);
                    if (rs.size() == 1)
                    {
                        const SuperPathId b(rs.front());
                        const uint32_t g = links.avgGap(a, b);
                        goodLinks.add(a, b, g);
                    }
                    else
                    {
                        // Go with the most supported alternative.
                        SuperPathId b(0);
                        uint32_t c(0);
                        for (uint64_t j = 0; j < rs.size(); ++j)
                        {
                            SuperPathId x(rs[j]);
                            uint32_t k = links.count(a, x);
                            if (k > c)
                            {
                                c = k;
                                b = x;
                            }
                        }
                        goodLinks.add(a, b, links.avgGap(a, b));
                    }
                }
                links.swap(goodLinks);
            }
            //cerr << "# lhs: " << links.mLhs.size() << '\n';
            {
                // Eliminate cases where the rhs doesn't uniquely determine the lhs.
                BiLinkMap goodLinks;
                for (BiLinkMap::UniLinkMap::const_iterator 
                     i = links.mRhs.begin(); i != links.mRhs.end(); ++i)
                {
                    SuperPathId b(i->first);
                    const vector<SuperPathId>& ls(i->second);
                    if (ls.size() == 1)
                    {
                        const SuperPathId a(ls.front());
                        const uint32_t g = links.avgGap(a, b);
                        goodLinks.add(a, b, g);
                    }
                    else
                    {
                        // Go with the most supported alternative.
                        SuperPathId a(0);
                        uint32_t c(0);
                        for (uint64_t j = 0; j < ls.size(); ++j)
                        {
                            SuperPathId x(ls[j]);
                            uint32_t k = links.count(x, b);
                            if (k > c)
                            {
                                c = k;
                                a = x;
                            }
                        }
                        goodLinks.add(a, b, links.avgGap(a, b));
                    }
                }
                links.swap(goodLinks);
            }

            //cerr << "# lhs: " << links.mLhs.size() << '\n';
            // links.removeCounts();

            for (BiLinkMap::UniLinkMap::const_iterator
                 i = links.mLhs.begin(); i != links.mLhs.end(); ++i)
            {
                BOOST_ASSERT(i->second.size() == 1);
                SuperPathId a = i->first;
                SuperPathId b = i->second.front();
                std::unordered_map<Link, uint32_t>::const_iterator j(links.mGaps.find(Link(a, b)));
                uint32_t g = j == links.mGaps.end() ? 0 : j->second;
                // cerr << "add\t" << a.value() << '\t' << b.value() << '\t' << g << '\n';
                lnks.add(a, b, g);
            }

            //cerr << "lnks.mLhs\t" << lnks.mLhs.size() << '\n';
            //cerr << "lnks.mRhs\t" << lnks.mRhs.size() << '\n';
            //cerr << "lnks.mGaps\t" << lnks.mGaps.size() << '\n';

        }
    }

    LOG(log, info) << "after filtering, " << lnks.mLhs.size() << " links remain";
    
    if (showLinkStats.on())
    {
        dumpLinks(cerr, lnks);
        cerr << "--------------------\n";
    }

    if (writeLinks.on())
    {
        FileFactory::OutHolderPtr outp(fac.out(mIn + ".links"));
        ostream& out(**outp);
        dumpLinks(out, lnks);
    }


    bool extd = false;
    uint64_t rounds = 0;
    uint64_t newPaths = 0;
    do
    {
        extd = false;
        for (SimpleBiLinkMap::UniLinkMap::iterator
             lhsIter  = lnks.mLhs.begin();
             lhsIter != lnks.mLhs.end();
             lhsIter  = lnks.mLhs.begin())
        {
            // lnks.verify();

            if (!(rounds++ % 16384))
            {
                LOG(log, info) << lnks.mLhs.size() << " reads remain, "
                               << newPaths << " new paths";
            }

            SuperPathId a(lhsIter->first);
            SuperPathId b(lhsIter->second);
            SuperPathId aRC = sg.reverseComplement(a);
            SuperPathId bRC = sg.reverseComplement(b);
            uint32_t gap = lnks.mGaps[Link(a, b)];

            // Look for a path from a to b.

            lnks.eraseLhs(a);
            lnks.eraseRhs(b);
            lnks.eraseLhs(bRC);
            lnks.eraseRhs(aRC);

            if (a == b || a == aRC || b == bRC)
            {
                // Found a loop - skip it.
                continue; 
            }   

/*
            LOG(log, info) << "Searching for path from " 
                           << a.value() << " (" << aRC.value() << ") to " 
                           << b.value() << " (" << bRC.value() << ")";
*/

            bool joined = false;
            Path p(1, a);
            if (findPath(sg, a, b, gap, 5, p))
            {
            /*
                cerr << "found path:";
                for (Path::const_iterator i = p.begin(); i != p.end(); ++i)
                {
                    cerr << ' ' << (*i).value();
                }
                cerr << '\n';
            */
                joined = true;
            }
            else
            {
                // TODO: Fill the gap with Ns.
                // cerr << "no path; adding " << gap << " Ns\n";
                // p.push_back(sg.gapPath(gap));
                // p.push_back(b);
                // joined = true;
            }

            if (joined)
            {
                extd = true;
                ++newPaths;
                Link l = sg.link(p);
                SuperPathId n = l.first;
                SuperPathId nRC = l.second;

                // cerr << "n " << n.value() << '\n' << "nRC " << nRC.value() << '\n';

                SimpleBiLinkMap::UniLinkMap::iterator ui;

                // a -> ... -> b
                // \___________/
                //   n

                // Extend the link that ends with a.
                ui = lnks.mRhs.find(a);
                if (ui != lnks.mRhs.end())
                {
                    lnks.substRhs(n, a);
                }

                // Extend the link that begins with b.
                ui = lnks.mLhs.find(b);
                if (ui != lnks.mLhs.end())
                {
                    lnks.substLhs(n, b);
                }

                // Extend the link that begins with a'.
                ui = lnks.mLhs.find(aRC);
                if (ui != lnks.mLhs.end())
                {
                    lnks.substLhs(nRC, aRC);
                }

                // Extend the link that ends with b'
                ui = lnks.mRhs.find(bRC);
                if (ui != lnks.mRhs.end())
                {
                    lnks.substRhs(nRC, bRC);
                }

                sg.erase(a);
                if (b != a && b != aRC)
                {
                    sg.erase(b);
                }
            }
        }
    }
    while (extd);
    LOG(log, info) << "all reads processed, " << newPaths << " new paths";

    LOG(log, info) << "simplifying new linear paths";
    newPaths = simplify(sg);
    LOG(log, info) << "found " << newPaths << " new paths";

    if (discardUpdates.on())
    {
        return;
    }

    log(info, "writing supergraph");
    sg.write(mIn, fac);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

GossCmdPtr
GossCmdFactoryThreadReads::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    strings fastas;
    chk.getRepeating0("fasta-in", fastas, readChk);

    strings fastqs;
    chk.getRepeating0("fastq-in", fastqs, readChk);

    strings lines;
    chk.getRepeating0("line-in", lines, readChk);

    uint64_t expectedCoverage = 0;
    bool inferCoverage = true;
    if (chk.getOptional("expected-coverage", expectedCoverage))
    {
        inferCoverage = false;
    }

    uint64_t c = 10;
    chk.getOptional("min-link-count", c);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    uint64_t cr = 4;
    chk.getOptional("edge-cache-rate", cr);

    bool del;
    chk.getOptional("delete-scaffold", del);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdThreadReads(in, fastas, fastqs, lines, 
                                             c, inferCoverage, expectedCoverage, T, cr, del));
}

GossCmdFactoryThreadReads::GossCmdFactoryThreadReads()
    : GossCmdFactory("thread reads through the supergraph.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("expected-coverage");
    mCommonOptions.insert("edge-cache-rate");
    mCommonOptions.insert("min-link-count");
    mCommonOptions.insert("delete-scaffold");
}
