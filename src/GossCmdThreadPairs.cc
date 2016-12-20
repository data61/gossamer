// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdThreadPairs.hh"

#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "Debug.hh"
#include "EdgeIndex.hh"
#include "EntryEdgeSet.hh"
#include "EstimateGraphStatistics.hh"
#include "ExternalBufferSort.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "PairAligner.hh"
#include "PairLink.hh"
#include "PairLinker.hh"
#include "ReadPairSequenceFileSequence.hh"
#include "ScaffoldGraph.hh"
#include "SuperGraph.hh"
#include "SuperPathId.hh"
#include "Timer.hh"
#include "TrivialVector.hh"

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

typedef vector<string> strings;

typedef pair<SuperPathId,SuperPathId> Link;
typedef pair<int64_t, int64_t> Offsets;

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

Debug showLinkStats("show-link-stats", "show the summary statistics for links supported by read pairs.");
Debug saveLinkMap("save-link-map", "write out the set of established links during pair resolution.");
Debug loadLinkMap("load-link-map", "rather than processing reads, read in a previously computed set of links.");
Debug extLinkMap("extend-link-map", "read the previous set of links, process reads, and save the merged result");
Debug discardUpdates("discard-updated-supergraph", "after pair threading, don't save the resulting supergraph.");
Debug showDistStats("show-dist-stats", "show the distribution of distances of pairs within a superpath.");

template <typename Dest>
class LinkMapCompiler
{
public:
    void push_back(const vector<uint8_t>& pItem)
    {
        const uint8_t* hd(&pItem[0]);
        PairLink::decode(hd, mLink);

        if (mLink.lhs == mPrev.lhs && mLink.rhs == mPrev.rhs)
        {
            mCount++;
            mLhsOffsetSum += mLink.lhsOffset;
            mLhsOffsetSum2 += mLink.lhsOffset * mLink.lhsOffset;
            mRhsOffsetSum += mLink.rhsOffset;
            mRhsOffsetSum2 += mLink.rhsOffset * mLink.rhsOffset;
            return;
        }

        if (mCount > 0)
        {
            mDest.push_back(mPrev.lhs, mPrev.rhs, mCount,
                            mLhsOffsetSum, mLhsOffsetSum2,
                            mRhsOffsetSum, mRhsOffsetSum2);
        }
        mCount = 1;
        mLhsOffsetSum = mLink.lhsOffset;
        mLhsOffsetSum2 = mLink.lhsOffset * mLink.lhsOffset;
        mRhsOffsetSum = mLink.rhsOffset;
        mRhsOffsetSum2 = mLink.rhsOffset * mLink.rhsOffset;
        mPrev = mLink;
    }

    void end()
    {
        if (mCount > 0)
        {
            mDest.push_back(mPrev.lhs, mPrev.rhs, mCount,
                            mLhsOffsetSum, mLhsOffsetSum2,
                            mRhsOffsetSum, mRhsOffsetSum2);
        }
        mDest.end();
    }

    LinkMapCompiler(Dest& pDest)
        : mDest(pDest),
          mPrev(SuperPathId(-1LL), SuperPathId(-1LL), 0, 0),
          mLink(SuperPathId(0), SuperPathId(0), 0, 0),
          mCount(0),
          mLhsOffsetSum(0), mLhsOffsetSum2(0),
          mRhsOffsetSum(0), mRhsOffsetSum2(0)
    {
    }

private:
    Dest& mDest;
    PairLink mPrev;
    PairLink mLink;
    uint64_t mCount;
    int64_t mLhsOffsetSum;
    uint64_t mLhsOffsetSum2;
    int64_t mRhsOffsetSum;
    uint64_t mRhsOffsetSum2;
};

class LinkMapWriter
{
public:
    void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                   const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                   const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
    {
        mOut << pLhs.value() << '\t'
             << pRhs.value() << '\t'
             << pCount << '\t'
             << pLhsOffsetSum << '\t'
             << pLhsOffsetSum2 << '\t'
             << pRhsOffsetSum << '\t'
             << pRhsOffsetSum2 << '\n';
    }

    void end()
    {
    }

    LinkMapWriter(ostream& pOut)
        : mOut(pOut)
    {
    }

private:
    ostream& mOut;
};

template <typename Dest>
class LinkFilter
{
public:
    void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                   const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                   const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
    {
        if (pCount < mMinLinkCount)
        {
            return;
        }

        // Eliminate links which are too far apart, 
        // i.e. cannot be spanned by any in-bounds path.
        //
        // Lhs              Rhs
        // ----------.......-------------
        //     ^     ^            ^
        //     +-----+-----------+
        //     |     |           |
        //   lhsAvg  lhsSize     rhsAvg
        //
        int64_t lhsAvg = pLhsOffsetSum / pCount;
        int64_t rhsAvg = pRhsOffsetSum / pCount;
        int64_t minDist =   rhsAvg + static_cast<int64_t>(mSuperGraph[pLhs].size(mEntries) + mK) - lhsAvg;

        if (minDist > mMaxInsertSize)
        {
            return;
        }

        mDest.push_back(pLhs, pRhs, pCount,
                        pLhsOffsetSum, pLhsOffsetSum2, pRhsOffsetSum, pRhsOffsetSum2);
    }

    void end()
    {
        mDest.end();
    }

    LinkFilter(Dest& pDest, uint64_t pMinLinkCount, uint64_t pMaxInsertSize,
               const SuperGraph& pSuperGraph, const EntryEdgeSet& pEntries)
        : mDest(pDest), mMinLinkCount(pMinLinkCount), mMaxInsertSize(pMaxInsertSize),
          mSuperGraph(pSuperGraph), mEntries(pEntries), mK(mEntries.K())
    {
    }

private:
    Dest& mDest;
    uint64_t mMinLinkCount;
    int64_t mMaxInsertSize;
    const SuperGraph& mSuperGraph;
    const EntryEdgeSet& mEntries;
    const uint64_t mK;
};

struct BiLinkMap
{
public:
    struct UniInfo
    {
        UniInfo(SuperPathId pOther, uint64_t pOffs)
            : mOther(pOther), mOffs(pOffs)
        {
        }

        SuperPathId mOther;
        int64_t mOffs;
    };

    typedef vector<UniInfo> UniInfos;
    typedef std::unordered_map<SuperPathId, UniInfos> UniLinkMap;

    int64_t lhsOffs(const Link& pLink) const
    {
        SuperPathId a = pLink.first;
        SuperPathId b = pLink.second;
        UniLinkMap::const_iterator l = mLhs.find(a);
        BOOST_ASSERT(l != mLhs.end());
        for (UniInfos::const_iterator i = l->second.begin(); i != l->second.end(); ++i)
        {
            if (i->mOther == b)
            {
                return i->mOffs;
            }
        }
        BOOST_ASSERT(false);
        return 0;
    }

    int64_t rhsOffs(const Link& pLink) const
    {
        SuperPathId a = pLink.first;
        SuperPathId b = pLink.second;
        UniLinkMap::const_iterator r = mRhs.find(b);
        BOOST_ASSERT(r != mRhs.end());
        for (UniInfos::const_iterator i = r->second.begin(); i != r->second.end(); ++i)
        {
            if (i->mOther == a)
            {
                return i->mOffs;
            }
        }
        BOOST_ASSERT(false);
        return 0;
    }

    void add(const Link& pLink, int64_t pLhsOffs, int64_t pRhsOffs)
    {
        SuperPathId lhs(pLink.first);
        SuperPathId rhs(pLink.second);
        mLhs[lhs].push_back(UniInfo(rhs, pLhsOffs));
        mRhs[rhs].push_back(UniInfo(lhs, pRhsOffs));
    }

    void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                   const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                   const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
    {
        add(make_pair(pLhs, pRhs), pLhsOffsetSum / pCount, pRhsOffsetSum / pCount);
    }

    void end() const
    {
    }

    void copy(const Link& pOldLink, const Link& pNewLink)
    {
        uint64_t l(lhsOffs(pOldLink));
        uint64_t r(rhsOffs(pOldLink));
        add(pNewLink, l, r);
    }

/*
    void insert(const pair<Link, LinkData>& data)
    {
        SuperPathId a = data.first.first;
        SuperPathId b = data.first.second;
        const LinkData& d = data.second;
        mLhs[a].push_back(UniInfo(b, d.lhsAvg()));
        mRhs[b].push_back(UniInfo(a, d.rhsAvg()));
    }
*/

    // Erase the given link.
    void erase(const Link& link)
    {
        const SuperPathId lhs = link.first;
        const SuperPathId rhs = link.second;

        UniLinkMap::iterator lhsIter = mLhs.find(lhs);
        BOOST_ASSERT(lhsIter != mLhs.end());
        UniInfos::iterator itr;

        UniInfos& rhss(lhsIter->second);
        for (itr = rhss.begin(); itr != rhss.end(); ++itr)
        {
            if (itr->mOther == rhs)
            {
                break;
            }
        }
        BOOST_ASSERT(itr != rhss.end());
        rhss.erase(itr);

        UniLinkMap::iterator rhsIter = mRhs.find(rhs);
        BOOST_ASSERT(rhsIter != mRhs.end());
        UniInfos& lhss(rhsIter->second);
        for (itr = lhss.begin(); itr != lhss.end(); ++itr)
        {
            if (itr->mOther == lhs)
            {
                break;
            }
        }
        BOOST_ASSERT(itr != lhss.end());
        lhss.erase(itr);
    }

    // Erase all links with the given id.
    void erase(SuperPathId pId)
    {
        // cerr << "BiLinkMap.erase " << pId.value() << '\n';
        eraseLhs(pId);
        eraseRhs(pId);
    }

    // Erase all links with the given lhs id from both the left-hand 
    // and right-hand indexes.
    // Note: This will invalidate any iterators pointing to erased elements.
    void eraseLhs(SuperPathId pId)
    {
        UniLinkMap::iterator lhsIter = mLhs.find(pId);
        if (lhsIter == mLhs.end())
        {
            // No links to erase.
            return;
        }
        const UniInfos& rs(lhsIter->second);
        for (UniInfos::const_iterator i = rs.begin(); i != rs.end(); ++i)
        {
            SuperPathId r = i->mOther;
            UniLinkMap::iterator rhsIter = mRhs.find(r);
            BOOST_ASSERT(rhsIter != mRhs.end());
            UniInfos& ls(rhsIter->second);

            for (UniInfos::iterator j = ls.begin(); j != ls.end(); ++j)
            {
                if (j->mOther == pId)
                {
                    // Move the rest of the vector down over this entry.
                    UniInfos::iterator k = j;
                    ::copy(++k, ls.end(), j);
                    ls.pop_back();
                    break;
                }
            }

            if (ls.empty())
            {
                mRhs.erase(rhsIter);
            }
        }
        mLhs.erase(lhsIter);
    }

    // Erase all links with the given rhs id from both the left-hand 
    // and right-hand indexes.
    // Note: This will invalidate any iterators pointing to erased elements.
    void eraseRhs(SuperPathId pId)
    {
        UniLinkMap::iterator rhsIter = mRhs.find(pId);
        if (rhsIter == mRhs.end())
        {
            // No links to erase.
            return;
        }
        const UniInfos& ls(rhsIter->second);
        for (UniInfos::const_iterator i = ls.begin(); i != ls.end(); ++i)
        {
            SuperPathId l = i->mOther;
            UniLinkMap::iterator lhsIter = mLhs.find(l);
            BOOST_ASSERT(lhsIter != mLhs.end());
            UniInfos& ls(lhsIter->second);

            for (UniInfos::iterator j = ls.begin(); j != ls.end(); ++j)
            {
                if (j->mOther == pId)
                {
                    // Move the rest of the vector down over this entry.
                    UniInfos::iterator k = j;
                    ::copy(++k, ls.end(), j);
                    ls.pop_back();
                    break;
                }
            }

            if (ls.empty())
            {
                mLhs.erase(lhsIter);
            }
        }
        mRhs.erase(rhsIter);
    }

    void dumpLinkStats(ostream& pOut, SuperGraph& pSG, uint64_t pInsert) const
    {
        const EntryEdgeSet& entries(pSG.entries());
        const uint64_t K(entries.K());
        pOut << "From\tTo\tLhsAvg\tRhsAvg\tLhsSize\tRhsSize\tGap\n";
        for (UniLinkMap::const_iterator
             i = mLhs.begin(); i != mLhs.end(); ++i)
        {
            SuperPathId l = i->first;
            for (UniInfos::const_iterator
                 j = i->second.begin(); j != i->second.end(); ++j)
            {
                SuperPathId r = j->mOther;
                int64_t l_ofs = j->mOffs;
                int64_t r_ofs = rhsOffs(Link(l, r));
                int64_t len = (pSG[l].size(entries) + K - l_ofs) + r_ofs;
                int64_t gap = int64_t(pInsert) - len;
                pOut << l.value() << '\t'
                     << r.value() << '\t'
                     << l_ofs << '\t'
                     << r_ofs << '\t'
                     << (pSG[l].size(entries) + K) << '\t'
                     << (pSG[r].size(entries) + K) << '\t'
                     << gap
                     << '\n';
            }
        }
    }

    void write(FileFactory& pFac, const string& pFilename)
    {
        FileFactory::OutHolderPtr outp(pFac.out(pFilename));
        ostream& out(**outp);
        for (UniLinkMap::const_iterator
             i = mLhs.begin(); i != mLhs.end(); ++i)
        {
            SuperPathId l = i->first;
            for (UniInfos::const_iterator
                 j = i->second.begin(); j != i->second.end(); ++j)
            {
                SuperPathId r = j->mOther;
                int64_t l_ofs = j->mOffs;
                int64_t r_ofs = rhsOffs(Link(l, r));
                out << l.value() << '\t' << r.value() << '\t'
                    << l_ofs << '\t' << r_ofs << '\n';
            }
        }
    }

    static bool read(FileFactory& pFac, const string& pFilename, BiLinkMap& pBiLinks)
    {
        pBiLinks.mLhs.clear();
        pBiLinks.mRhs.clear();

        try
        {
            FileFactory::InHolderPtr inp(pFac.in(pFilename));
            istream& in(**inp);
            while (in.good())
            {
                uint64_t l;
                uint64_t r;
                int64_t l_ofs;
                int64_t r_ofs;
                in >> l >> r >> l_ofs >> r_ofs;
                if (!in.good())
                {
                    break;
                }
                pBiLinks.add(Link(SuperPathId(l), SuperPathId(r)), l_ofs, r_ofs);
            }
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    UniLinkMap mLhs;
    UniLinkMap mRhs;
};

typedef vector<SuperPathId> Path;
typedef vector<Path> Paths;

void
shortestPaths(SuperGraph& pSG, Logger& pLog,
              const SuperPathId& pBegin, const SuperPathId& pEnd,
              int64_t pInitLen, int64_t pMinLen, int64_t pMaxLen,
              uint64_t pMaxPaths, uint64_t pSearchRadius, Paths& pPaths)
{
    LOG(pLog, debug) 
        << "Searching for path from " << pBegin.value() << " to " << pEnd.value() << '\n';
    SuperGraph::Node source(pSG.end(pSG[pBegin]));
    SuperGraph::Node sink(pSG.start(pSG[pEnd]));
    uint64_t num_paths = 0;
    for (SuperGraph::ShortestPathIterator 
         itr(pSG, source, sink, pMaxLen, pSearchRadius); itr.valid() && num_paths <= pMaxPaths; ++itr)
    {
        const Path& p(*itr);
        ++num_paths;
        int64_t sz = pInitLen;
        for (uint64_t j = 0; j < p.size(); ++j)
        {
            const SuperPath& w(pSG[p[j]]);
            sz += pSG.size(w);
        }
        stringstream ss;
        for (uint64_t i = 0; i < p.size(); ++i)
        {
            ss << " " << p[i].value();
        }
        LOG(pLog, debug)
            << "Found path between " << pBegin.value() << " and " << pEnd.value() 
            << " of length: " << sz << " with " << p.size() << " segments:" << ss.str() << '\n';

        if (sz > pMaxLen)
        {
            LOG(pLog, debug) << "Too long!";
            break;
        }
        if (sz < pMinLen)
        {
            LOG(pLog, debug) << "Too short!";
            continue;
        }

        pPaths.push_back(p);
    }
}

bool
distToSegment(const SuperGraph& pSG, const Path& pPath, 
              uint64_t pFrom, SuperPathId pSeg, uint64_t& pDist, uint32_t& pCursor)
{
    uint64_t d = 0;
    for (uint64_t i = pFrom; i < pPath.size(); ++i)
    {
        if (pPath[i] == pSeg)
        {
            pDist += d;
            pCursor = i;
            return true;
        }

        d += pSG.size(pPath[i]);
    }

    return false;
}

// Find the minimal-N path c which is a proper sub-path of all the given paths, P.
// i.e. for each p in P, the sequence of non-N segments of c, should appear in the
//      same order in p_s.
void
findConsensusPath(SuperGraph& pSG, const Paths& pPaths, Path& pPath)
{
    const uint64_t n = pPaths.size();
    pPath.clear();
    vector<uint32_t> cursor(n, 0);
    vector<uint32_t> next(n, 0);
    while (true)
    {
/*
        cerr << "cursor:";
        for (uint32_t i = 0; i < n; ++i)
        {
            cerr << ' ' << cursor[i];
        }
        cerr << '\n';
*/
        // Check if any of the paths is exhausted.
        for (uint32_t i = 0; i < n; ++i)
        {
            if (cursor[i] >= pPaths[i].size()) 
            {
                return;
            }
        }

        uint64_t d = 0;
        SuperPathId s = pPaths[0][cursor[0]];
        // cerr << "s = " << s.value() << '\n';
        bool found = true;
        for (uint64_t i = 1; i < n && found; ++i)
        {
            found = distToSegment(pSG, pPaths[i], cursor[i], s, d, next[i]);
        }
        // cerr << "found? " << found << '\n';
        if (found)
        {
            d = d / n;
            if (d != 0)
            {
                pPath.push_back(pSG.gapPath(d));
            }
            pPath.push_back(s);
            cursor[0] += 1;
            for (uint64_t i = 1; i < n; ++i)
            {
                cursor[i] = next[i] + 1;
            }
        }
        else
        {
            cursor[0] += 1;
        }
    }
}

// Calculate insert size mean and std. dev. from positive distances.
void reportDistStats(const PairLinker::Hist& pHist, Logger& pLog)
{
    if (pLog.sev() < info)
    {
        return;
    }
    
    // Find the mid-point (mean), and the target mass to retain.
    double sum = 0;
    double c = 0;
    for (PairLinker::Hist::const_iterator
         itr = pHist.begin(); itr != pHist.end(); ++itr)
    {
        if (itr->first > 0)
        {
            double l(itr->first);
            double n(itr->second);
            c += itr->second;
            sum += n * l;
        }
    }
    double mean = sum / c;
    const double halfTargetMass = 0.99 * sum / 2;

    // Locate the mid point in the histogram.
    PairLinker::Hist::const_iterator mid;
    for (mid = pHist.begin(); mid != pHist.end(); ++mid)
    {
        if (mid->first > 0 && mid->first > mean)
        {
            break;
        }
    }

//      cerr << "halfTargetMass: " << halfTargetMass << '\n';

    double midMass = double(mid->first) * double(mid->second);
    if (   mid != pHist.end()
        && mid != pHist.begin())
    {
        // Locate the left-hand side of the included region.
        double mass = midMass / 2;
        PairLinker::Hist::const_iterator left = mid;
        --left;
        while (   left != pHist.begin()
               && mass < halfTargetMass
               && left->first > 0)
        {
            mass += double(left->first) * double(left->second);
            --left;
            // cerr << "lhs mass: " << mass << '\n';
        }


        // Locate the right-hand side of the included region.
        mass = midMass / 2;
        PairLinker::Hist::const_iterator right = mid;
        ++right;
        while (   right != pHist.end()
               && mass < halfTargetMass)
        {
            mass += double(right->first) * double(right->second);
            ++right;
            // cerr << "rhs mass: " << mass << '\n';
        }

/*
        cerr << "lhs: " << left->first << '\n';
        cerr << "rhs: " << (  right == pHist.end() 
                            ? "END"
                            : lexical_cast<string>(right->first)) << '\n';
*/
        // Calculate the mean and std. dev. of the bounded region.
        double sum = 0;
        double sum2 = 0;
        uint64_t c = 0;
        for (PairLinker::Hist::const_iterator itr = left; itr != right; ++itr)
        {
            double l(itr->first);
            double n(itr->second);
            c += itr->second;
            sum += n * l;
            sum2 += n * l * l;
        }
        double m = sum / c;
        double sd = sqrt(c * sum2 - sum * sum) / c;
        double sdPc = 100.0 * sd / m;
        LOG(pLog, info) << "calculated insert size mean: " << m;
        LOG(pLog, info) << "calculated insert size std. dev.: " << sd << " bases ("
                       << sdPc << "% of mean)";
    }
}

} // namespace anonymous

void
GossCmdThreadPairs::operator()(const GossCmdContext& pCxt)
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
        if (mEstimateOnly)
        {
            return;
        }
    }

    auto sgp = SuperGraph::read(mIn, fac);
    SuperGraph& sg(*sgp);
    const EntryEdgeSet& entries(sg.entries());
    const uint64_t K(entries.K());

    map<int64_t,uint64_t> dist;
    BiLinkMap biLinks;
    ExternalBufferSort sorter(1024ULL * 1024ULL * 1024ULL, fac);

    if (loadLinkMap.on() || extLinkMap.on())
    {
        if (!BiLinkMap::read(fac, mIn + ".link_map", biLinks))
        {
            log(::error, "could not open link file " + mIn + ".links");
        }
    }
    if (!loadLinkMap.on())
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
        const PairAligner alnr(g, entries, idx);

        std::deque<GossReadSequence::Item> items;

        {
            GossReadSequenceFactoryPtr seqFac
                = std::make_shared<GossReadSequenceBasesFactory>();

            GossReadParserFactory lineParserFac(LineParser::create);
            for (auto& f: mLines)
            {
                items.push_back(GossReadSequence::Item(f,
                                lineParserFac, seqFac));
            }

            GossReadParserFactory fastaParserFac(FastaParser::create);
            for (auto& f: mFastas)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastaParserFac, seqFac));
            }

            GossReadParserFactory fastqParserFac(FastqParser::create);
            for (auto& f: mFastqs)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastqParserFac, seqFac));
            }
        }

        UnboundedProgressMonitor umon(log, 100000, " read pairs");
        LineSourceFactory lineSrcFac(BackgroundLineSource::create);
        ReadPairSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

        log(info, "mapping pairs.");

        UniquenessCache ucache(sg, coverage);
        BackgroundMultiConsumer<PairLinker::ReadPair> grp(128);
        vector<PairLinkerPtr> linkers;
        mutex mut;
        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            linkers.push_back(PairLinkerPtr(new PairLinker(sg, alnr, mOrientation, ucache, sorter, mut)));
            grp.add(*linkers.back());
        }

        while (reads.valid())
        {
            grp.push_back(PairLinker::ReadPair(reads.lhs().clone(), reads.rhs().clone()));
            ++reads;
        }
        grp.wait();

        log(info, "merging results.");
        for (vector<PairLinkerPtr>::const_iterator
             i = linkers.begin(); i != linkers.end(); ++i)
        {
            const PairLinker::Hist& d((*i)->getDist());
            for (PairLinker::Hist::const_iterator
                 j = d.begin(); j != d.end(); ++j)
            {
                dist[j->first] += j->second;
            }
        }
    }

    if (showDistStats.on())
    {
        cerr << "Distribution of distances of pairs mapped to a single superpath.\n";
        for (PairLinker::Hist::const_iterator
             itr = dist.begin(); itr != dist.end(); ++itr)
        {
            cerr << itr->first << ": " << itr->second << '\n';
        }
        cerr << "-------------\n";
    }

    reportDistStats(dist, log);

    const double dev = mInsertTolerance * mInsertStdDevFactor * mExpectedInsertSize;
    const uint64_t maxInsertSize = mExpectedInsertSize + dev;
    const uint64_t minInsertSize = mExpectedInsertSize - dev;
    const double expCov(coverage);

    LOG(log, info) << "expected insert size = " << mExpectedInsertSize;
    LOG(log, info) << "insert std dev. factor = " << mInsertStdDevFactor;
    LOG(log, info) << "insert tolerance = " << mInsertTolerance;
    LOG(log, info) << "max insert size = " << maxInsertSize;
    LOG(log, info) << "min insert size = " << minInsertSize;

    FileFactory::OutHolderPtr outp(fac.out(mIn + ".links"));
    LinkFilter<BiLinkMap> f(biLinks, mMinLinkCount, maxInsertSize, sg, entries);
    LinkMapCompiler<LinkFilter<BiLinkMap> > c(f);
    sorter.sort(c);

    // log(info, "found " + lexical_cast<string>(biLinks.size()) + " distinct links");

    if (showLinkStats.on())
    {
        biLinks.dumpLinkStats(cerr, sg, mExpectedInsertSize);
    }
    if (saveLinkMap.on())
    {
        biLinks.write(fac, mIn + ".link_map");
    }


    // Loop until fixed point.
    set<SuperPathId> valid;
    bool extd;
    uint64_t rounds = 0;
    uint64_t newPaths = 0;
    do 
    {
        extd = false;
        for (BiLinkMap::UniLinkMap::iterator
             lhsIter = biLinks.mLhs.begin(); 
             lhsIter != biLinks.mLhs.end(); 
             lhsIter = biLinks.mLhs.begin() )
        {
            if (!(rounds++ % 16384))
            {
                LOG(log, info) << biLinks.mLhs.size() << " pairs remain, "
                               << newPaths << " new paths";
            }
            SuperPathId a = lhsIter->first;

            BiLinkMap::UniInfos& rhss(lhsIter->second);
            for (uint64_t rhsIx = 0; rhsIx < rhss.size(); )
            {
                SuperPathId b = rhss[rhsIx].mOther;
                Link l(make_pair(a, b));

                if (a == b)
                {
                    // Found a loop - skip it.
                    biLinks.erase(l);
                    continue;
                }

                // Find paths between a and b.
                Paths ps;
                const SuperPath& lhs(sg[a]);
                const int64_t initLen =   (lhs.size(entries) + K - biLinks.lhsOffs(l)) 
                                        + biLinks.rhsOffs(l);
                const int64_t initGap = max(int64_t(0), int64_t(mExpectedInsertSize) - initLen);
                shortestPaths(sg, log, a, b, initLen, minInsertSize, maxInsertSize, 100, mSearchRadius, ps);
                Path p;

                if (ps.empty())
                {
//                    cerr << "No path\n";
/*
                    // No path.
                    biLinks.erase(l);
                    continue;
*/

                    if (   mFillGaps 
                        // && initLen > 0
                        && initGap < int64_t(mMaxGap))
                    {
//                        cerr << "inserting " << initGap << " Ns\n";
                        p.clear();
                        p.push_back(a);
                        if (initGap)
                        {
                            p.push_back(sg.gapPath(initGap));
                        }
                        p.push_back(b);
                    }
                    else
                    {
                        biLinks.erase(l);
                        continue;
                    }
                }
                else if (ps.size() > 1)
                {
//                    cerr << "Too many paths (" << ps.size() << ")\n";
/*
                    // No unique path.
                    ++rhsIx;
                    continue;
*/

                    if (mConsolidatePaths)
                    {
                        Path cp;
                        for (uint64_t i = 0; i < ps.size(); ++i)
                        {
                            Path& p(ps[i]);
                            p.insert(p.begin(), a);
                            p.push_back(b);
                        }
                        findConsensusPath(sg, ps, cp);

/*  
                        cerr << "consensus path:";
                        for (uint64_t i = 0; i < cp.size(); ++i)
                        {
                            cerr << ' ' << cp[i].value();
                        }
                        cerr << '\n';
*/                      
                    
                        p = cp;
                    }
                    else
                    {
                        biLinks.erase(l);
                        continue;
                    }
                }
                else
                {
                    // cerr << "Single path\n";
                    p = ps[ps.size() / 2];
                    p.insert(p.begin(), a);
                    p.push_back(b);
                }

                // Found an acceptable path: GO WITH IT!
                ++newPaths;
                extd = true;
                SuperPathId aRC = sg.reverseComplement(a);
                SuperPathId bRC = sg.reverseComplement(b);
                uint64_t bSz = sg.size(b);
                uint64_t aRCSz = sg.size(aRC);
                l = sg.link(p);
                SuperPathId n = l.first;
                SuperPathId nRC = l.second;

                // cerr << "n: " << n.value() << "\tn': " << nRC.value() << '\n';

                BiLinkMap::UniLinkMap::iterator ui;

                // a -> ... -> b
                // \___________/
                //   n

                // Extend links that end with a.
                // (X, a, l_pos, r_pos) --> (X, n, l_pos, r_pos)
                ui = biLinks.mRhs.find(a);
                if (ui != biLinks.mRhs.end())
                {
                    for (BiLinkMap::UniInfos::iterator
                         v = ui->second.begin(); v != ui->second.end(); ++v)
                    {
                        Link oldLnk(make_pair(v->mOther, a));
                        Link newLnk(make_pair(v->mOther, n));
                        biLinks.copy(oldLnk, newLnk);
                    }
                }

                // Extend links that start with b.
                // (b, X, l_pos, r_pos) --> (n, X, n - b + l_pos), r_pos)
                ui = biLinks.mLhs.find(b);
                if (ui != biLinks.mLhs.end())
                {
                    for (BiLinkMap::UniInfos::iterator
                         v = ui->second.begin(); v != ui->second.end(); ++v)
                    {
                        Link oldLnk(make_pair(b, v->mOther));
                        Link newLnk(make_pair(n, v->mOther));
                        uint64_t l(biLinks.lhsOffs(oldLnk));
                        uint64_t r(biLinks.rhsOffs(oldLnk));
                        l += sg.size(n) - bSz;
                        biLinks.add(newLnk, l, r);
                    }
                }

                // Extend links that start with a'.
                // (a', X, l_pos, r_pos) --> (n', X, n' - a' + l_pos), r_pos)
                ui = biLinks.mLhs.find(aRC);
                if (ui != biLinks.mLhs.end())
                {
                    for (BiLinkMap::UniInfos::iterator
                         v = ui->second.begin(); v != ui->second.end(); ++v)
                    {
                        Link oldLnk(make_pair(aRC, v->mOther));
                        Link newLnk(make_pair(nRC, v->mOther));
                        uint64_t l(biLinks.lhsOffs(oldLnk));
                        uint64_t r(biLinks.rhsOffs(oldLnk));
                        l += sg.size(nRC) - aRCSz;
                        biLinks.add(newLnk, l, r);
                    }
                }

                // Extend links that end with b'.
                // (X, b', l_pos, r_pos) --> (X, n', l_pos, r_pos)
                ui = biLinks.mRhs.find(bRC);
                if (ui != biLinks.mRhs.end())
                {
                    for (BiLinkMap::UniInfos::iterator
                         v = ui->second.begin(); v != ui->second.end(); ++v)
                    {
                        Link oldLnk(make_pair(v->mOther, bRC));
                        Link newLnk(make_pair(v->mOther, nRC));
                        biLinks.copy(oldLnk, newLnk);
                    }
                }

                // Ensure we don't attempt to delete a superpath multiple times.
                set<SuperPathId> deleted;
                for (uint64_t j = 0; j < p.size(); ++j)
                {
                    SuperPathId s(p[j]);
                    if (   !deleted.count(s) 
                        && sg.unique(sg[s], expCov))
                    {
                        const SuperPathId sRC(sg.reverseComplement(s));
                        deleted.insert(s);
                        deleted.insert(sRC);
                        biLinks.erase(s);
                        biLinks.erase(sRC);
                        sg.erase(s);
                    }
                }

                // Don't consider any further links from this lhs!
                break;
            }

            // Remove a. There are either no paths from it, or It's been extended, 
            // in which case a there's a new link in its place.
            biLinks.eraseLhs(a);
        }
    }
    while (extd);
    LOG(log, info) << "all pairs processed, " << newPaths << " new paths";

    if (discardUpdates.on())
    {
        return;
    }

    log(info, "writing supergraph");
    sg.write(mIn, fac);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

GossCmdPtr
GossCmdFactoryThreadPairs::create(App& pApp, const variables_map& pOpts)
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

    uint64_t expectedSize = 0;
    chk.getMandatory("insert-expected-size", expectedSize);

    double stdDev = 10.0;
    chk.getOptional("insert-size-std-dev", stdDev);
    double stdDevFactor = stdDev / 100.0;

    double tolerance = 2.0;
    chk.getOptional("insert-size-tolerance", tolerance);

    uint64_t c = 10;
    chk.getOptional("min-link-count", c);

    chk.exclusive("paired-ends", "mate-pairs", "innies", "outies");
    bool pe = false;
    chk.getOptional("paired-ends", pe);

    bool mp = false;
    chk.getOptional("mate-pairs", mp);

    bool inn = false;
    chk.getOptional("innies", inn);

    bool out = false;
    chk.getOptional("outies", out);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    uint64_t cr = 4;
    chk.getOptional("edge-cache-rate", cr);

    uint64_t rad = 10;
    chk.getOptional("search-radius", rad);

    bool estimateOnly = false;
    chk.getOptional("estimate-only", estimateOnly);

    if (estimateOnly && !inferCoverage)
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::usage_info("Must be inferring coverage to estimate-only."));
    }

    PairLinker::Orientation o(  mp ? PairLinker::MatePairs
                              : inn ? PairLinker::Innies
                              : out ? PairLinker::Outies
                              : PairLinker::PairedEnds);

    bool consPaths = false;
    chk.getOptional("consolidate-paths", consPaths);

    bool fillGaps = false;
    chk.getOptional("fill-gaps", fillGaps);

    uint64_t maxGap = numeric_limits<uint64_t>::max();
    chk.getOptional("max-gap", maxGap);

    bool del;
    chk.getOptional("delete-scaffold", del);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdThreadPairs(in, fastas, fastqs, lines, c,
            inferCoverage, expectedCoverage, expectedSize, stdDevFactor,
            tolerance, o, T, cr, rad, estimateOnly, consPaths, fillGaps, maxGap, del));
}

GossCmdFactoryThreadPairs::GossCmdFactoryThreadPairs()
    : GossCmdFactory("generate a de Bruijn graph's supergraph.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("expected-coverage");
    mCommonOptions.insert("edge-cache-rate");
    mCommonOptions.insert("min-link-count");
    mCommonOptions.insert("estimate-only");
    mCommonOptions.insert("paired-ends");
    mCommonOptions.insert("mate-pairs");
    mCommonOptions.insert("innies");
    mCommonOptions.insert("outies");
    mCommonOptions.insert("insert-expected-size");
    mCommonOptions.insert("insert-size-std-dev");
    mCommonOptions.insert("insert-size-tolerance");
    mCommonOptions.insert("delete-scaffold");

    mSpecificOptions.addOpt<uint64_t>("search-radius", "",
            "only find paths within this number of edges of a sink - 0 is unlimited (default 10)");
    mSpecificOptions.addOpt<bool>("consolidate-paths", "",
            "join multiple paths into a single representative one containing Ns");
    mSpecificOptions.addOpt<bool>("fill-gaps", "",
            "fill gaps, up to the size limit, between paired contigs with Ns");
    mSpecificOptions.addOpt<uint64_t>("max-gap", "",
            "the size, in bases, of the maximum gap to fill (default: no limit)");
}

