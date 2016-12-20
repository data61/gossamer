// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdFixReads.hh"

#include "Debug.hh"
#include "EdgeIndex.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadDispatcher.hh"
#include "GossReadHandler.hh"
#include "GossReadProcessor.hh"
#include "Graph.hh"
#include "KmerAligner.hh"
#include "LineParser.hh"
#include "StringFileFactory.hh"
#include "Timer.hh"

#include <cctype>
#include <cmath>
#include <iostream>
#include <queue>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/math/distributions.hpp>
#include <thread>
#include <utility>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace   // anonymous
{

    // Union-find data structure.
    // Note: Implicitly contains all elements of T in their own singleton sets at construction.
    template<typename T>
    class DisjointSet
    {
        typedef pair<T, uint64_t> ParentRank;
    public:

        // union!
        void join(const T& pX, const T& pY)
        {
            const T& xRoot = find(pX);
            const T& yRoot = find(pY);
            if (xRoot == yRoot)
            {
                return;
            }

            // Merge shallowest root under deepest.
            ParentRank& prX(lookup(xRoot));
            ParentRank& prY(lookup(yRoot));
            if (prX.second < prY.second)
            {
                prX.first = yRoot;
            }
            else if (prY.second < prX.second)
            {
                prY.first = xRoot;
            }
            else
            {
                prY.first = xRoot;
                prX.second += 1;
            }
        }

        T find(const T& pX)
        {
            ParentRank& prX(lookup(pX));
            if (prX.first != pX)
            {
                prX.first = find(prX.first);
            }
            return prX.first;
        }

        DisjointSet()
            : mParents()
        {
        }

    private:

        ParentRank& lookup(const T& pX)
        {
            return (mParents.insert(make_pair(pX, ParentRank(pX, 0))).first)->second;
        }

        std::unordered_map<T, ParentRank> mParents;
    };

    class OfsCmp 
    {
    public:

        bool operator()(uint64_t pX, uint64_t pY) const
        {
            auto itrX = mKmerOfs.find(pX);
            auto itrY = mKmerOfs.find(pY);
            if (itrX != mKmerOfs.end() && itrY != mKmerOfs.end())
            {
                return itrX->second < itrY->second;
            }
            return pX < pY;
        }

        OfsCmp(const std::unordered_map<uint64_t, uint64_t>& pKmerOfs)
            : mKmerOfs(pKmerOfs)
        {
        }

    private:

        const std::unordered_map<uint64_t, uint64_t>& mKmerOfs;
    };

    class CompCmpGt
    {
    public:
        
        bool operator()(uint64_t pX, uint64_t pY) const
        {   
            map<uint64_t, double>::const_iterator itrX(mCompWeights.find(pX));
            map<uint64_t, double>::const_iterator itrY(mCompWeights.find(pY));
            if (itrX != mCompWeights.end() && itrY != mCompWeights.end())
            {
                return itrX->second > itrY->second;
            }
            return pX > pY;
        }

        CompCmpGt(const map<uint64_t, double>& pCompWeights)
            : mCompWeights(pCompWeights)
        {
        }

    private:

        const map<uint64_t, double>& mCompWeights;
    };

    typedef std::tuple<uint64_t, uint64_t, string> Frag;
    class FragCmp
    {
    public:
        
        bool operator()(const Frag& pX, const Frag& pY) const
        {
            return std::get<0>(pX) < std::get<0>(pY);
        }

        FragCmp()
        {
        }
    };

    template <typename T>
    class RcEdgeAdapter
    {
    public:

        bool operator()(const Graph::Edge& pEdge, uint64_t pRank)
        {
            const Graph::Edge e(mGraph.reverseComplement(pEdge));
            const uint64_t r = mGraph.rank(e);
            return mVis(e, r);
        }

        RcEdgeAdapter(const Graph& pGraph, T& pVis)
            : mGraph(pGraph), mVis(pVis)
        {
        }

    private:
        const Graph& mGraph;
        T& mVis;
    };

    // Used to accumulate edges along a linear path up to (but not including) some rank.
    class EdgeAccum
    {
    public:

        bool operator()(const Graph::Edge& pEdge, uint64_t pRank)
        {
            if (pRank == mStopRank)
            {
                return false;
            }
            mRanks.push_back(pRank);
            return true;
        }

        EdgeAccum(vector<uint64_t>& pRanks, uint64_t pStopRank)
            : mRanks(pRanks), mStopRank(pStopRank)
        {
        }

    private:
        vector<uint64_t>& mRanks;
        const uint64_t mStopRank;
    };

    // Used to accumulate no more than the specified number of edges along a linear path.
    class FixedLenEdgeAccum
    {
    public:

        bool operator()(const Graph::Edge& pEdge, uint64_t pRank)
        {
            if (!mStepsLeft)
            {
                return false;
            }
            mRanks.push_back(pRank);
            return --mStepsLeft;
        }

        FixedLenEdgeAccum(vector<uint64_t>& pRanks, uint64_t pSteps=-1)
            : mStepsLeft(pSteps), mRanks(pRanks)
        {
        }

    private:
        uint64_t mStepsLeft;
        vector<uint64_t>& mRanks;
    };

    class Scanner : public GossReadHandler
    {
        static constexpr double sIndelRate = 0.15;
        static constexpr uint64_t sMinEdges = 100;
        static constexpr uint64_t sMaxBranch = 1024;
        static constexpr double sMinHitPairP = 1.0e-9;
        static constexpr uint64_t sMaxPairLookAhead = 100000; // 100;
        static constexpr uint64_t sMaxMatchK = 21;

    public:

        // template<typename T>
        void pileup(const vector<string>& pItems, ostream& pOut) const
        {
            vector<string> lines;
            for (uint64_t i = 0; i < pItems.size(); ++i)
            {
                bool done = false;
                string item = lexical_cast<string>(pItems[i]);
                for (uint64_t j = 0; j < lines.size(); ++j)
                {
                    string& line(lines[j]);
                    if (line.size() < i)
                    {
                        line += string(i - line.size(), ' ') + item;
                        done = true;
                        break;
                    }
                }
                if (!done)
                {
                    string line(i, ' ');
                    line += item;
                    lines.push_back(line);
                }
            }
            for (uint64_t i = 0; i < lines.size(); ++i)
            {
                cerr << lines[i] << '\n';
            }
            
        }

        // Skips 0s.
        // template<uint64_t>
        void pileup(const vector<uint64_t>& pItems, ostream& pOut) const
        {
            vector<string> items;
            for (uint64_t i = 0; i < pItems.size(); ++i)
            {
                string item;
                if (pItems[i])
                {
                    item = lexical_cast<string>(pItems[i]);
                }
                items.push_back(item);
            }
            pileup(items, pOut);
        }

        pair<uint64_t, uint64_t> rankK(const Gossamer::position_type& pKmer, uint64_t pK) const
        {
            const uint64_t rho = mGraph.K() + 1;
            const uint64_t dRho = rho - pK;
            Gossamer::position_type lo = pKmer;
            lo >>= dRho * 2;
            Gossamer::position_type hi = lo;
            lo <<= dRho * 2;
            hi += 1;
            hi <<= dRho * 2;
            pair<uint64_t,uint64_t> r = mGraph.rank(Graph::Edge(lo), Graph::Edge(hi));
            //cerr << kmerToString(rho, pKmer) << '\t' << kmerToString(rho, lo) << '\t' << kmerToString(rho, hi) << '\t' << r.first << '\t' << r.second << '\n';
            return r;
        }

        // True if pTo follows pFrom.
        bool adjSegment(SuperPathId pFrom, SuperPathId pTo)
        {
            EntryEdgeSet::Edge e(mEntries.reverseComplement(mEntries.select(mEntries.endRank(pFrom.value()))));
            pair<uint64_t, uint64_t> rs = mEntries.beginEndRank(mEntries.to(e));
            bool res = rs.first <= pTo.value() && pTo.value() < rs.second;
            return res;
        }

        typedef pair<SuperPathId, uint64_t> SegOfs;

        // Returns the distance in bases between ranked edges pFrom and pTo, as long as they fall on the same, or adjacent linear segments.
        uint64_t dist(const vector<SegOfs>& pSegOfsMap, uint64_t pFromPos, uint64_t pToPos)
        {
            const SegOfs& from(pSegOfsMap[pFromPos]);
            const SegOfs& to(pSegOfsMap[pToPos]);
            if (from.first == to.first)
            {
                return to.second - from.second;
            }
           
            if (adjSegment(from.first, to.first))
            {
                return mEntries.length(from.first.value()) - from.second + to.second;
            }

            return 0;
        }

        // Probability of not seeing a k-mer of this size in this graph.
        double coProbKmer(uint64_t pK) const
        {
            return 1.0 - min(1.0, mGraph.count() / pow(4, pK));
        }

        double probHitPair(const vector<SegOfs>& pSegOfsMap, const vector<uint64_t>& pHiKs, uint64_t pI, uint64_t pJ)
        {
            const uint64_t l = dist(pSegOfsMap, pI, pJ);
            if (!l)
            {
                return 0.0;
            }
    
            const double o = pJ - pI;
            const double v = 2 * l * sIndelRate * (1.0 - sIndelRate);
            const double sd = sqrt(v);
            const boost::math::normal norm(0.0, sd);
            const double prDist = 1.0 - boost::math::cdf(norm, fabs(o - l));
            const double prI = coProbKmer(pHiKs[pI]);
            const double prJ = coProbKmer(pHiKs[pJ]);
            const double pr = prI * prJ * prDist;
            return pr;
        }

        // Add all edges (ranks) between pFromPos and pToPos (exclusive) to pEdges.
        // Returns true if the positions are on adjacent edges.
        bool fillReadEdges(const vector<SegOfs>& pSegOfsMap, const vector<uint64_t>& pRanks, uint64_t pFromPos, uint64_t pToPos, vector<uint64_t>& pEdges) // const
        {
            BOOST_ASSERT(pFromPos <= pToPos);
            const SegOfs& from(pSegOfsMap[pFromPos]);
            const SegOfs& to(pSegOfsMap[pToPos]);
            // cerr << "fill\t" << pFromPos << " (" << from.first.value() << "," << from.second << ")\t" << pToPos << " (" << to.first.value() << "," << to.second << ")\n";

            if (from.first == to.first)
            {
                // Same linear path.
                EdgeAccum vis(pEdges, pRanks[pToPos]);
                mGraph.linearPath(mGraph.select(pRanks[pFromPos]), vis);
                return false;
            }

            // Accumulate to end of first linear path.
            // If the starting (base) offset is already within the last path edge, then don't perform any traversal!
            // cerr << pEdges.size() << '\n';
            if (pFromPos < mEntries.length(from.first.value()))
            {
                EdgeAccum vis1(pEdges, -1);
                mGraph.linearPath(mGraph.select(pRanks[pFromPos]), vis1);
            }
            // cerr << pEdges.size() << '\n';

            // Continue along second (adjacent) linear path.
            Graph::Edge e(mEntries.select(to.first.value()).value());
            EdgeAccum vis2(pEdges, pRanks[pToPos]);
            mGraph.linearPath(e, vis2);
            // cerr << pEdges.size() << '\n';
            return true;
        }

        // If the position [pFrom, pTo) in pUsed are false, sets them, and returns true.
        // Otherwise returns false (returning pUsed to its original state.)
        bool occupyRange(uint64_t pFrom, uint64_t pTo, dynamic_bitset<>& pUsed) const
        {
            // cerr << "occupyRange\t" << pFrom << '\t' << pTo << '\n';
            bool fits = true;
            for (uint64_t i = pFrom; i < pTo; ++i)
            {
                if (pUsed[i])
                {
                    // Rewind
                    for (uint64_t j = pFrom; j < i; ++j)
                    {
                        pUsed[j] = false;
                    }
                    fits = false;
                    break;
                }
                pUsed[i] = true;
            }
            return fits;
        }

        // If the position [pFrom, pTo) in pUsed are false, sets them, and returns true.
        // Otherwise returns false (leaving pUsed untouched.)
        bool freeRange(uint64_t pFrom, uint64_t pTo, const dynamic_bitset<>& pUsed) const
        {
            for (uint64_t i = pFrom; i < pTo; ++i)
            {
                if (pUsed[i])
                {
                    return false;
                }
            }
            
            return true;
        }

        // Sets bits in range [pFrom, pTo).
        void setRange(uint64_t pFrom, uint64_t pTo, dynamic_bitset<>& pUsed) const
        {
            for (uint64_t i = pFrom; i < pTo; ++i)
            {
                pUsed[i] = true;
            }
        }

        bool matchingBases(char pA, char pB) const
        {
            if (pA == pB)
            {
                return true;
            }

            switch (pA)
            {
                case 'a':
                case 'A':
                    return pB == 'a' || pB == 'A';

                case 'c':
                case 'C':
                    return pB == 'c' || pB == 'C';

                case 't':
                case 'T':
                    return pB == 't' || pB == 'T';

                case 'g':
                case 'G':
                    return pB == 'g' || pB == 'G';
            };

            return false;
        }

        // Returns the length of an optimally aligned prefix of pRead to pPath.
        uint64_t matchLen(const string& pRead, const string& pPath) const
        {
            static const int32_t gapCost = -1;
            static const int32_t substCost = -4;
            static const int32_t matchCost = 1;

            const int32_t m = pRead.size();
            const int32_t n = pPath.size();

            vector<uint32_t> f(n + 1);
            for (int32_t j = 0; j <= n; ++j)
            {
                f[j] = j * gapCost;
            }

            int32_t best = n * gapCost;
            uint32_t bestI = 0;
            for (int32_t i = 1; i <= m; ++i)
            {
                int32_t prev = i * gapCost;
                for (int32_t j = 1; j <= n; ++j)
                {
                    const int32_t ins = f[j] + gapCost;
                    const int32_t del = prev + gapCost;
                    const int32_t match = f[j - 1] + (matchingBases(pRead[i - 1], pPath[j - 1]) ? matchCost : substCost);
                    const int32_t cur = max(match, max(del, ins));
                    f[j - 1] = prev;
                    prev = cur;
                }
                f[n] = prev;
                if (prev > best)
                {
                    best = prev;
                    bestI = i;
                }
            }

            return bestI;
        }

        uint64_t matchLen(uint64_t pStartRank, uint64_t pReadOfs, const string& pRead) // const
        {
            vector<uint64_t> edges;
            FixedLenEdgeAccum vis(edges);
            mGraph.linearPath(mGraph.select(pStartRank), vis);
            string path;
            sequence(edges, path);
            const uint64_t maxReadLen = min(uint64_t(path.size() * 1.5), pRead.size() - pReadOfs);
            string read(pRead.substr(pReadOfs, maxReadLen));
            const uint64_t len = matchLen(read, path);
            return len;
        }

        uint64_t matchLenReverse(uint64_t pStartRank, uint64_t pReadOfs, const string& pRead) // const
        {
            vector<uint64_t> edges;
            FixedLenEdgeAccum vis(edges);
            RcEdgeAdapter<FixedLenEdgeAccum> rvis(mGraph, vis);
            mGraph.linearPath(mGraph.reverseComplement(mGraph.select(pStartRank)), rvis);
            reverse(edges.begin(), edges.end());
            string path;
            sequence(edges, path);
            const uint64_t maxMatchLen = path.size() * 1.5;
            const uint64_t startOfs = maxMatchLen >= pReadOfs ? 0 : pReadOfs - maxMatchLen;
            string read(pRead.substr(startOfs, pReadOfs));
            reverse(read.begin(), read.end());
            reverse(path.begin(), path.end());
            const uint64_t len = matchLen(read, path);
            BOOST_ASSERT(len <= read.size());
            return len;
        }

        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            throw "!";
        }

        void operator()(const GossRead& pRead)
        {
            const uint64_t loK(ceil(log(mGraph.count()) / log(4.0)));
            const uint64_t hiK(mGraph.K() + 1);
            const uint64_t numLocs(pRead.length());
            vector<uint64_t> hiKs(numLocs, 0);
            vector<uint64_t> ranks(numLocs, -1);
            for (GossRead::Iterator itr(pRead, hiK); itr.valid(); ++itr)
            {
                Gossamer::edge_type e(*itr);
                uint64_t ofs = itr.offset();
                uint64_t hk = hiK;
                uint64_t lk = loK;

                // check the lower bound is sensible.
                {
                    pair<uint64_t, uint64_t> rs = rankK(e, lk);
                    if (rs.second - rs.first == 0)
                    {
                        continue;
                    }
                }

                // check the upper bound is sensible.
                {
                    pair<uint64_t, uint64_t> rs = rankK(e, hk);
                    if (rs.second - rs.first > 1)
                    {
                        continue;
                    }
                    if (rs.second - rs.first == 1)
                    {
                        //cerr << ofs << '\t' << hk << endl;
                        hiKs[ofs] = hk;
                        ranks[ofs] = rs.first;
                        //unique_lock<mutex> lock(mMutex);
                        //mHist[foundK]++;
                        continue;
                    }
                }

                uint64_t foundK = 0;
                uint64_t rnk = 0;
                while (hk >= lk)
                {
                    uint64_t mk = (hk + lk) / 2;
                    pair<uint64_t, uint64_t> rs = rankK(e, mk);
                    //cerr << lk << '\t' << mk << '\t' << hk << '\t' << (rs.second - rs.first) << endl;
                    if (rs.second - rs.first == 0)
                    {
                        hk = mk - 1;
                        continue;
                    }
                    if (rs.second - rs.first > 1)
                    {
                        lk = mk + 1;
                        continue;
                    }
                    foundK = mk;
                    rnk = rs.first;
                    lk = mk + 1;
                }
                if (foundK)
                {
                    //cerr << ofs << '\t' << foundK << endl;
                    hiKs[ofs] = foundK;
                    ranks[ofs] = rnk;
                    //unique_lock<mutex> lock(mMutex);
                    //mHist[foundK]++;
                }
            }

            // cerr << pRead.read() << '\n';
            // pileup(hiKs, cerr);
            /*
            {
                const uint64_t rho = mGraph.K() + 1;
                vector<string> kmers;
                for (uint64_t i = 0; i < hiKs.size(); ++i)
                {
                    if (hiKs[i] == 0)
                    {
                        kmers.push_back("");
                        continue;
                    }
                    Graph::Edge e = mGraph.select(ranks[i]);
                    e.value() >>= 2 * (rho - hiKs[i]);
                    kmers.push_back(kmerToString(hiKs[i], e.value()));
                }
                pileup(kmers, cerr);
            }
            */

            // segments
            mKmerAligner.reset();
            vector<SegOfs> segOfs(ranks.size(), SegOfs(SuperPathId(0), 0));
            typedef std::unordered_map<uint64_t, vector<uint64_t> > SegPosType;
            SegPosType segPos;
            {
                // vector<string> segStrs;
                for (uint64_t i(0); i != ranks.size(); ++i)
                {
                    // string segStr;
                    SuperPathId id(0);
                    if (hiKs[i] == 0)
                    {
                        continue;
                    }
                    Graph::Edge e(mGraph.select(ranks[i]));
                    uint64_t ofs = 0;
                    bool res = mKmerAligner(e.value(), id, ofs);
                    BOOST_ASSERT(res);
                    (void) (&res); // suppress a warning in the release build.
                    segOfs[i] = make_pair(id, ofs);
                    segPos[id.value()].push_back(i);
                    // segStr = lexical_cast<string>(id.value()) + "," + lexical_cast<string>(ofs);
                    // segStrs.push_back(segStr);
                }
                // pileup(segStrs, cerr);
            }

            // Filter out positions which are the only occurrence of a linear path in the read
            // (and occur too far in from the ends of the path to plausibly link anything.)
            uint64_t cancelled = 0;
            for (uint64_t i = 0; i < hiKs.size(); ++i)
            {
                const SegOfs& so(segOfs[i]);
                if (segPos[so.first.value()].size() == 1)
                {
                    const uint64_t pathLen = mEntries.length(so.first.value()) + mEntries.K();
                    const uint64_t distToReadEnd = numLocs - i;
                    const uint64_t distToPathEnd = pathLen - so.second;
                    const uint64_t distToReadStart = i;
                    const uint64_t distToPathStart = so.second;
                    // If the path starts before the read, and ends after it, cancel the hit.
                    if (   distToPathStart > distToReadStart
                        && distToPathEnd > distToReadEnd)
                    {
                        hiKs[i] = 0;
                        ranks[i] = -1;
                        ++cancelled;
                        segPos.erase(so.first.value());
                    }
                }
            }
            // cerr << "cancelled\t" << cancelled << '\n';

            map<uint64_t, vector<pair<double, uint64_t> > > pairLinks;
            map<pair<uint64_t, uint64_t>, double> pairPrMap;
            DisjointSet<uint64_t> components;
            set<uint64_t> keyPos;
            const uint64_t maxPairLookAhead = numLocs / 3;
            for (SegPosType::const_iterator itr(segPos.begin()); itr != segPos.end(); ++itr)
            {
                uint64_t seg = itr->first;
                const vector<uint64_t>& pos = itr->second;
                for (uint64_t x = 0; x < pos.size(); ++x)
                {
                    const uint64_t i = pos[x];
                    // Following hits in this segment.
                    for (uint64_t y = x + 1; y < pos.size(); ++y)
                    {
                        const uint64_t j = pos[y];
                        BOOST_ASSERT(i < j);
                        const double pr = probHitPair(segOfs, hiKs, i, j);
                        if (pr >= sMinHitPairP)
                        {
                            pairLinks[i].push_back(make_pair(pr, j));
                            pairPrMap[make_pair(i, j)] = pr;
                            components.join(i, j);
                            keyPos.insert(i);
                            keyPos.insert(j);
                        }
                    }

                    // All hits in adjacent segments.
                    const uint64_t z(mEntries.count());
                    const pair<uint64_t, uint64_t> rs = mHood.rank(Gossamer::position_type(seg * z), 
                                                                   Gossamer::position_type((seg + 1) * z));
                    for (uint64_t r = rs.first; r != rs.second; ++r)
                    {
                        const uint64_t nSeg = (mHood.select(r) - seg * z).asUInt64();
                        if (nSeg == seg)
                        {
                            continue;
                        }
                        SegPosType::const_iterator nSegItr = segPos.find(nSeg);
                        if (nSegItr == segPos.end())
                        {
                            continue;
                        }
                        const vector<uint64_t>& adjPos = nSegItr->second;
                        for (uint64_t y = 0; y != adjPos.size(); ++y)
                        {
                            const uint64_t j = adjPos[y];
                            if (j >= i)
                            {
                                // Cyclical graph!
                                continue;
                            }
                            if (j > i + maxPairLookAhead)
                            {
                                // Too far ahead!
                                continue;
                            }

                            const double pr = probHitPair(segOfs, hiKs, i, j);
                            if (pr >= sMinHitPairP)
                            {
                                pairLinks[i].push_back(make_pair(pr, j));
                                pairPrMap[make_pair(i, j)] = pr;
                                components.join(i, j);
                                keyPos.insert(i);
                                keyPos.insert(j);
                            }
                        }
                    }
                }
            }

                
            // cerr << "seg lengths\n";

            map<uint64_t, vector<uint64_t> > groups;
            for (set<uint64_t>::const_iterator itr = keyPos.begin(); itr != keyPos.end(); ++itr)
            {
                const uint64_t rep = components.find(*itr);
                groups[rep].push_back(*itr);
            }

            map<uint64_t, double> groupWeight;
            for (map<pair<uint64_t, uint64_t>, double>::const_iterator itr = pairPrMap.begin(); itr != pairPrMap.end(); ++itr)
            {
                const uint64_t rep = components.find(itr->first.first);
                groupWeight[rep] += itr->second;
            }

/*
            cerr << "components\n";
            for (map<uint64_t, vector<uint64_t> >::const_iterator itr = groups.begin(); itr != groups.end(); ++itr)
            {
                cerr << groupWeight[itr->first] << '\t';
                for (uint64_t i = 0; i < itr->second.size(); ++i)
                {
                    cerr << itr->second[i] << '\t';
                }
                cerr << '\n';
            }
*/

            if (groupWeight.empty())
            {
                unique_lock<mutex> lock(mMutex);
                mOut << '>' << pRead.label() << '\n';
                const string& read(pRead.read());
                for (uint64_t i = 0; i < read.size(); ++i)
                {
                    mOut << char(tolower(read[i]));
                }
                mOut << '\n';
                return;
            }

            // Sort components in order of decreasing weight.           
            vector<uint64_t> compReps;
            compReps.reserve(groupWeight.size());
            for (map<uint64_t, double>::const_iterator itr(groupWeight.begin()); itr != groupWeight.end(); ++itr)
            {
                compReps.push_back(itr->first);
            }

            const uint64_t numComps = compReps.size();
            sort(compReps.begin(), compReps.end(), CompCmpGt(groupWeight));
            BOOST_ASSERT(numComps == compReps.size());
            (void) (&numComps); // suppress a warning in the release build.

            {
                vector<Frag> frags;
                dynamic_bitset<> usedPos(numLocs);
                uint64_t numUsedComps = 0;
                uint64_t numJuncs = 0;
                vector<uint64_t> usedSegs;
                for (uint64_t i = 0; i < compReps.size(); ++i)
                {
                    BOOST_ASSERT(compReps[i] <= numLocs);

                    // cerr << "comp rep: " << compReps[i] << '\n';
                    vector<uint64_t> edges;
                    const vector<uint64_t>& comp(groups[compReps[i]]);
                    const uint64_t firstHitPos = *min_element(comp.begin(), comp.end());
                    uint64_t firstPos = firstHitPos;
                    uint64_t curPos = firstPos;

                    // Expand the component.
                    vector<uint64_t> compSegs;
                    uint64_t compJuncs = 0;
                    bool fits = true;
                    while (true)
                    {
                        const uint64_t curSeg = segOfs[curPos].first.value();
                        if (compSegs.empty() || compSegs.back() != curSeg)
                        {
                            compSegs.push_back(curSeg);
                        }
                        const vector<pair<double, uint64_t> >& links(pairLinks[curPos]);
                        if (links.empty())
                        {
                            // No links - we're done with this component.
                            break;
                        }
                        const uint64_t nextPos = max_element(links.begin(), links.end())->second; 
                        if (!freeRange(curPos, nextPos + 1, usedPos))
                        {
                            fits = false;
                            break;
                        }
                        compJuncs += fillReadEdges(segOfs, ranks, curPos, nextPos, edges);
                        curPos = nextPos;
                    }
    
                    if (!fits)
                    {
                        continue;
                    }

                    // Add last component edge.
                    edges.push_back(ranks[curPos]);
                    setRange(firstHitPos, curPos + 1, usedPos);
                    uint64_t lastPos = curPos + hiKs[curPos] - 1;
                    BOOST_ASSERT(lastPos < numLocs);
                    // goto done;

                    // cerr << "firstHitPos\t" << firstHitPos << '\n';

                    // Extend the component backwards along its first linear path.
                    if (firstPos != 0)
                    {
                        // If the path extends before the start of the read, just fill it up to that length.
                        const uint64_t readLenBefore = firstPos;
                        const SegOfs& so(segOfs[firstPos]);
                        const uint64_t pathLenBefore = so.second;
                        vector<uint64_t> preEdges;
                        // cerr << "readLenBefore\t" << readLenBefore << '\n';
                        // cerr << "pathLenBefore\t" << pathLenBefore << '\n';
                        // cerr << "num edges\t" << edges.size() << '\n';
                        if (pathLenBefore > mGraph.K() + 1)
                        {
                            if (pathLenBefore >= readLenBefore)
                            {
                                fits = occupyRange(0, firstPos, usedPos);
                                if (fits)
                                {
                                    preEdges.reserve(readLenBefore + edges.size());
                                    FixedLenEdgeAccum vis(preEdges, readLenBefore);
                                    RcEdgeAdapter<FixedLenEdgeAccum> rvis(mGraph, vis);
                                    mGraph.linearPath(mGraph.reverseComplement(mGraph.select(ranks[firstPos])), rvis);
                                    firstPos = 0;
                                }
                            }
                            else
                            {
                                // Find the read position of the beginning of the linear path.
                                const int64_t len = matchLenReverse(ranks[firstPos], firstPos, pRead.read());
                                // cerr << "len\t" << len << '\n';
                                if (len)
                                {
                                    fits = occupyRange(firstPos - len, firstPos, usedPos);
                                    if (fits)
                                    {
                                        preEdges.reserve(readLenBefore + edges.size());
                                        FixedLenEdgeAccum vis(preEdges);
                                        RcEdgeAdapter<FixedLenEdgeAccum> rvis(mGraph, vis);
                                        mGraph.linearPath(mGraph.reverseComplement(mGraph.select(ranks[firstPos])), rvis);
                                        firstPos -= len;
                                    }
                                }
                            }

                            // Edges were accumulated in the reverse order. i.e. away from the first hit, not towards.
                            if (preEdges.size())
                            {
                                reverse(preEdges.begin(), preEdges.end());
                                // Drop last edge, so we don't repeat it.
                                preEdges.pop_back();
                            }
                            preEdges.insert(preEdges.end(), edges.begin(), edges.end());
                            edges.swap(preEdges);
                        }
                        // cerr << "num edges\t" << edges.size() << '\n';
                    }

                    if (!fits)
                    {
                        continue;
                    }

                    {
                        // If the path extends beyond the end of the read, just fill it in up to that length.
                        const uint64_t readLenAfter = numLocs - lastPos;
                        const SegOfs& so(segOfs[curPos]);
                        const uint64_t segLen = mEntries.length(so.first.value()) + mGraph.K();
                        const uint64_t pathLenAfter = segLen - so.second;
                        // cerr << "readLenAfter\t" << readLenAfter << '\n';
                        // cerr << "pathLenAfter\t" << pathLenAfter << '\n';
                        // cerr << "num edges\t" << edges.size() << '\n';
                        if (readLenAfter && pathLenAfter > mGraph.K() + 1)
                        {
                            if (pathLenAfter >= readLenAfter)
                            {
                                fits = occupyRange(lastPos, numLocs - 1, usedPos);
                                if (fits)
                                {
                                    // Drop last edge, so we don't repeat it.
                                    edges.pop_back();
                                    FixedLenEdgeAccum vis(edges, readLenAfter);
                                    mGraph.linearPath(mGraph.select(ranks[curPos]), vis);
                                    lastPos = numLocs;
                                }
                            }
                            else
                            {
                                // Find the read position of the end of the linear path.
                                const int64_t len = matchLen(ranks[curPos], curPos, pRead.read());
                                // cerr << "curPos\t" << curPos << '\n';
                                // cerr << "len\t" << len << '\n';
                                if (len)
                                {
                                    fits = occupyRange(lastPos, lastPos + len, usedPos);
                                    if (fits)
                                    {
                                        // Drop last edge, so we don't repeat it.
                                        edges.pop_back();
                                        FixedLenEdgeAccum vis(edges);
                                        mGraph.linearPath(mGraph.select(ranks[curPos]), vis);
                                        lastPos = lastPos + len;
                                    }
                                }
                            }
                        }
                        // cerr << "num edges\t" << edges.size() << '\n';
                    }

                    if (!fits)
                    {
                        continue;
                    }
                    frags.push_back(Frag(firstPos, lastPos, ""));
                    sequence(edges, std::get<2>(frags.back()));
                    numUsedComps += 1;
                    numJuncs += compJuncs;
                    usedSegs.insert(usedSegs.end(), compSegs.begin(), compSegs.end());
                }
                // cerr << "used " << usedComps << " of " << compReps.size() << " components.\n";
                // cerr << frags.size() << '\n';
                {
                    FragCmp cmp;
                    sort(frags.begin(), frags.end(), cmp);
                }

                // Write sequence of interspersed gaps and frags.
                string corRead;
                corRead.reserve(numLocs * 1.5);
                uint64_t gapPos = 0;
                for (uint64_t i = 0; i < frags.size(); ++i)
                {
                    const Frag& frag(frags[i]);
                    for (uint64_t j = gapPos; j < std::get<0>(frag); ++j)
                    {
                        corRead += tolower(pRead.read()[j]);
                    }
                    corRead += std::get<2>(frag);
                    gapPos = std::get<1>(frag);
                }
                for (uint64_t j = gapPos; j < pRead.length(); ++j)
                {
                    corRead += tolower(pRead.read()[j]);
                }
                // Final "gap".
                stringstream usedSegsSs;
                if (!usedSegs.empty())
                {
                    usedSegsSs << lexical_cast<string>(usedSegs[0]);
                    for (uint64_t i = 1; i < usedSegs.size(); ++i)
                    {
                        usedSegsSs << ':' << lexical_cast<string>(usedSegs[i]);
                    }
                }
                unique_lock<mutex> lock(mMutex);
                mOut << '>' << pRead.label() << ' ' << pRead.read().size() << ',' << corRead.size() << ',' << numUsedComps << ',' << numJuncs << ",[" << usedSegsSs.str() << "]\n";
                mOut << corRead << '\n';
            }
        }

        ~Scanner()
        {
            for (map<uint64_t,uint64_t>::const_iterator i = mHist.begin(); i != mHist.end(); ++i)
            {
                cerr << i->first << '\t' << i->second << endl;
            }
        }

        Scanner(const Graph& pGraph, const EntryEdgeSet& pEntries, const EdgeIndex& pEdgeIndex,
                const SparseArray& pHood, ostream& pOut, mutex& pMutex)
            : mGraph(pGraph), mEntries(pEntries), mHood(pHood), 
              mKmerAligner(pGraph, pEntries, pEdgeIndex), mOut(pOut), mMutex(pMutex)
        {
        }

    private:
    
        Scanner(const Scanner&);

        void sequence(const vector<uint64_t>& pEdges, string& pSeq) const
        {
            static const char bases[4] = {'A', 'C', 'G', 'T'};
            pSeq.reserve(pSeq.size() + pEdges.size() + mGraph.K());
            kmerToString(mGraph.K() + 1, mGraph.select(pEdges.front()).value(), pSeq);
            // cerr << pEdges[0] << '\t' << kmerToString(mGraph.K() + 1, mGraph.select(pEdges[0]).value()) << '\n';
            for (uint64_t i = 1; i < pEdges.size(); ++i)
            {
                // cerr << pEdges[i] << '\t' << kmerToString(mGraph.K() + 1, mGraph.select(pEdges[i]).value()) << '\n';
                Gossamer::position_type p = mGraph.select(pEdges[i]).value();
                pSeq += bases[p.asUInt64() & 3];
            }
        }

        const Graph& mGraph;
        const EntryEdgeSet& mEntries;
        const SparseArray& mHood;
        KmerAligner mKmerAligner;
        ostream& mOut;
        mutex& mMutex;
        map<uint64_t,uint64_t> mHist;
    };

    typedef std::shared_ptr<Scanner> ScannerPtr;

    template<typename T>
    void bfs(const EntryEdgeSet& pEntries, uint64_t pStart, uint64_t pDepth, T& pVis)
    {
        typedef pair<uint64_t, uint64_t> EdgeDepth;
        queue<EdgeDepth> q;
        q.push(EdgeDepth(pStart, pDepth));
        unordered_set<uint64_t> seen;
        while (q.size())
        {
            EdgeDepth ed(q.front());
            const uint64_t cur = ed.first;
            const uint64_t depth = ed.second;
            q.pop();

            if (!seen.insert(cur).second)
            {
                continue;
            }

            pVis(cur);
            
            if (depth)
            {
                EntryEdgeSet::Edge e(pEntries.reverseComplement(pEntries.select(pEntries.endRank(cur))));
                pair<uint64_t, uint64_t> rs = pEntries.beginEndRank(pEntries.to(e));
                for (uint64_t r = rs.first; r != rs.second; ++r)
                {
                    q.push(EdgeDepth(r, depth - 1));
                }
            }
        }
    }

    struct BfsCounter
    {
        void operator()(uint64_t pRank)
        {
            ++mCount;
        }

        BfsCounter()
            : mCount(0)
        {
        }
        
        uint64_t mCount;
    };

    struct BfsAccum
    {
        void operator()(uint64_t pRank)
        {
            mRanks.push_back(pRank);
        }

        BfsAccum()
            : mRanks()
        {
        }
        
        vector<uint64_t> mRanks;
    };

    void buildNeighbourhoodMatrix(FileFactory& pFactory, const string& pName, uint64_t pDepth, const EntryEdgeSet& pEntries)
    {
        BfsCounter cnt;
        for (uint64_t i = 0; i < pEntries.count(); ++i)
        {
            bfs(pEntries, i, pDepth, cnt);
        }

        const uint64_t z(pEntries.count());
        const Gossamer::position_type n(z * z);
        const uint64_t m(cnt.mCount);

        SparseArray::Builder bld(pName, pFactory, n, m);
        BfsAccum accum;
        for (uint64_t i = 0; i < pEntries.count(); ++i)
        {
            accum.mRanks.clear();
            bfs(pEntries, i, pDepth, accum);
            sort(accum.mRanks.begin(), accum.mRanks.end());
            for (uint64_t j = 0; j < accum.mRanks.size(); ++j)
            {
                Gossamer::position_type p(i * z + accum.mRanks[j]);
                bld.push_back(p);
            }
        }
        bld.end(n);
    }

}   // namespace anonymous

void
GossCmdFixReads::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    CmdTimer t(log);

    GraphPtr gPtr(Graph::open(mIn, fac));
    const Graph& g(*gPtr);
    EntryEdgeSet ee(mIn + "-entries", fac);
    auto sgPtr = SuperGraph::create(mIn, fac);
    SuperGraph& sg = *sgPtr;

    log(info, "building edge index");
    auto eixPtr = EdgeIndex::create(g, ee, sg, 1, mNumThreads, log);
    EdgeIndex& eix = *eixPtr;
    mutex mut;

    log(info, "building segment neighbourhood matrix");
    StringFileFactory sfac;
    buildNeighbourhoodMatrix(sfac, "neighbourhood", 1, ee);
    const SparseArray hood("neighbourhood", sfac);

    FileFactory::OutHolderPtr outPtr(fac.out(mOut));
    ostream& out(**outPtr);

    log(info, "processing reads");
    std::vector<std::shared_ptr<GossReadHandler> > sps;
    for (uint64_t i = 0; i < mNumThreads; ++i)
    {
        sps.push_back(std::shared_ptr<GossReadHandler>(new Scanner(g, ee, eix, hood, out, mut)));
    }
    GossReadDispatcher handler(sps);
    GossReadProcessor::processSingle(pCxt, mFastas, mFastqs, mLines, handler);
}


GossCmdPtr
GossCmdFactoryFixReads::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());

    string g;
    chk.getRepeatingOnce("graph-in", g);

    strings fas;
    chk.getOptional("fasta-in", fas);

    strings fasFiles;
    chk.getOptional("fastas-in", fasFiles);
    chk.expandFilenames(fasFiles, fas, fac);

    strings fqs;
    chk.getOptional("fastq-in", fqs);

    strings fqsFiles;
    chk.getOptional("fastqs-in", fqsFiles);
    chk.expandFilenames(fqsFiles, fqs, fac);

    strings ls;
    chk.getOptional("line-in", ls);

    string out = "-";
    chk.getOptional("output-file", out, GossOptionChecker::FileCreateCheck(fac, false));

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdFixReads(g, fas, fqs, ls, out, T));
}

GossCmdFactoryFixReads::GossCmdFactoryFixReads()
    : GossCmdFactory("read error correction")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("fastas-in");
    mCommonOptions.insert("fastqs-in");
    mCommonOptions.insert("kmer-size");
    mCommonOptions.insert("output-file");
}
