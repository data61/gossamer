// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "TransCmdAssemble.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "MultithreadedBatchTask.hh"
#include "ProgressMonitor.hh"
#include "Timer.hh"

#include <string>
#include <cmath>
#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/iterator/counting_iterator.hpp>

#include "SmallByteArray.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "LineParser.hh"
#include "ReadSequenceFileSequence.hh"
#include "ReadPairSequenceFileSequence.hh"
#include "ReverseComplementAdapter.hh"
#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "GossReadSequenceBases.hh"
#include "ResolveTranscripts.hh"
#include "ExternalBufferSort.hh"
#include "BoundedQueue.hh"
#include "VByteCodec.hh"

#undef DEBUG
#undef DUMP_CONTIGS
#undef DUMP_COMPONENT_INTERMEDIATES
#undef VERBOSE_TRACE_CONTIGS

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;
typedef pair<GossReadPtr, GossReadPtr> ReadPair;

namespace // anonymous
{
    void seqToVec(const char* pSeq, SmallBaseVector& pVec)
    {
        pVec.clear();
        for (const char* s = pSeq; *s; ++s)
        {
            switch (*s)
            {
                case 'A':
                case 'a':
                {
                    pVec.push_back(0);
                    break;
                }
                case 'C':
                case 'c':
                {
                    pVec.push_back(1);
                    break;
                }
                case 'G':
                case 'g':
                {
                    pVec.push_back(2);
                    break;
                }
                case 'T':
                case 't':
                {
                    pVec.push_back(3);
                    break;
                }
            }
        }
    }

    struct ContigInfo
    {
        struct Sentinel {};

        uint32_t mAvgCount;
        uint32_t mLength;
        rank_type mBeginEdge;
        SmallByteArray mData;

        ContigInfo(uint32_t pAvgCount, const Graph& pGraph,
                   const SmallBaseVector& pVec)
            : mAvgCount(pAvgCount)
        {
            compressContig(pGraph, pVec);
#ifdef DEBUG
            SmallBaseVector seq;
            decompressContig(pGraph, seq);
            if (lexical_cast<string>(pVec) != lexical_cast<string>(seq))
            {
                compressContig(pGraph, pVec);
                cerr << ">from\n";
                pVec.print(cerr);
                decompressContig(pGraph, seq);
                cerr << ">to\n";
                seq.print(cerr);
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << general_error_info("Round-trip compressContig failed"));
            }
#endif
        }

        ContigInfo(const Sentinel&)
            : mAvgCount(0), mLength(0)
        {
        }

        void compressContig(const Graph& pGraph, const SmallBaseVector& pContig)
        {
            mLength = pContig.size();
            uint64_t rho = pGraph.K() + 1;

            vector<uint8_t> data;
            data.reserve((mLength - rho)*2);

            mBeginEdge = pGraph.rank(Graph::Edge(pContig.kmer(rho, 0)));
            uint64_t upper = pContig.size() - rho;

            for (uint64_t i = 1; i <= upper; ++i)
            {
                Graph::Edge en(pContig.kmer(rho, i));
                rank_type rnkn = pGraph.rank(en);
                pair<rank_type,rank_type> rs
                    = pGraph.beginEndRank(pGraph.from(en));
                if (rnkn < rs.first || rs.second <= rnkn)
                {
                    BOOST_THROW_EXCEPTION(Gossamer::error()
                        << general_error_info("Internal error in compressContig"));
                }
                uint8_t outEdge = rnkn - rs.first;
                switch (rs.second - rs.first)
                {
                    case 1:
                    {
                        break;
                    }
                    case 2:
                    {
                        data.push_back(outEdge);
                        break;
                    }
                    case 3:
                    case 4:
                    {
                        data.push_back((outEdge & 2) ? 1 : 0);
                        data.push_back((outEdge & 1) ? 1 : 0);
                        break;
                    }
                    default:
                    {
                        BOOST_THROW_EXCEPTION(Gossamer::error()
                            << general_error_info("Internal error in compressContig (rank)"));
                    }

                }
            }

            mData.setSize((data.size() + 7) / 8);
            for (uint64_t i = 0; i < data.size(); ++i)
            {
                if (data[i])
                {
                    mData[i/8] |= ((uint8_t)1) << (i&7);
                }
                else
                {
                    mData[i/8] &= ~(((uint8_t)1) << (i&7));
                }
            }
        }

        void decompressContig(const Graph& pGraph, SmallBaseVector& pContig) const
        {
            pContig.clear();

            uint64_t rho = pGraph.K() + 1;

            Graph::Edge edge = pGraph.select(mBeginEdge);
            pGraph.seq(edge, pContig);

            uint64_t ptr = 0;
            uint64_t upper = mLength - rho + 1;
            for (uint64_t i = 1; i < upper; ++i)
            {
                Graph::Node n(pGraph.to(edge));
                pair<rank_type,rank_type> rs = pGraph.beginEndRank(n);
                rank_type rnk = rs.first;
                switch (rs.second - rs.first)
                {
                    case 1:
                    {
                        break;
                    }
                    case 2:
                    {
                        rnk += (mData[ptr/8] & (1 << (ptr&7))) ? 1 : 0;
                        ++ptr;
                        break;
                    }
                    case 3:
                    case 4:
                    {
                        rnk += (mData[ptr/8] & (1 << (ptr&7))) ? 2 : 0;
                        ++ptr;
                        rnk += (mData[ptr/8] & (1 << (ptr&7))) ? 1 : 0;
                        ++ptr;
                        break;
                    }
                    default:
                    {
                        BOOST_THROW_EXCEPTION(Gossamer::error()
                            << general_error_info("Internal error in decompressContig (rank)"));
                    }
                }
                edge = pGraph.select(rnk);
                pContig.push_back(edge.value() & 3);
            }
        }

    };

    struct SeedInfo
    {
        uint64_t mRank;
        uint32_t mCount;

        SeedInfo()
            : mRank(0), mCount(0)
        {
        }

        SeedInfo(uint64_t pRank, uint32_t pCount)
            : mRank(pRank), mCount(pCount)
        {
        }
    };

    bool seedInfoGt(const SeedInfo& pLhs, const SeedInfo& pRhs)
    {
        return pLhs.mCount > pRhs.mCount;
    }

    struct EdgeInfo
    {
        Graph::Edge mEdge;
        uint64_t mRank;
        uint32_t mCount;

        EdgeInfo()
            : mEdge(Graph::Edge(position_type(0))), mRank(0), mCount(0)
        {
        }

        EdgeInfo(Graph::Edge pEdge, uint64_t pRank, uint32_t pCount)
            : mEdge(pEdge), mRank(pRank), mCount(pCount)
        {
        }
    };

    bool edgeInfoGt(const EdgeInfo& pLhs, const EdgeInfo& pRhs)
    {
        return pLhs.mCount > pRhs.mCount;
    }


    bool dinucleotideRepeat(Graph::Node pNode, uint64_t pK)
    {
        const double sTrivialRepeatThresh = 0.6;
        Gossamer::position_type kmer = pNode.value();
        std::vector<uint8_t> bases;
        bases.reserve(pK);
        for (uint64_t i = 0; i < pK; ++i)
        {
            bases.push_back((uint8_t)(kmer & 3));
            kmer >>= 2;
        }
        uint64_t rpts = 0;
        for (uint64_t i = 2; i < pK; ++i)
        {
            if (bases[i-2] == bases[i])
            {
                ++rpts;
            }
        }

        return ((double)rpts / pK) > sTrivialRepeatThresh;
    }

    template<typename T>
    double entropyOrder0(const T& pNodeOrEdge, uint64_t pSize)
    {
        const double sInvLog2 = 1.0 / M_LN2;
        uint8_t counts[4] = { 0, 0, 0, 0 };
        Gossamer::position_type mer = pNodeOrEdge.value();
        for (uint64_t i = 0; i < pSize; ++i)
        {
            ++counts[mer & 3];
            mer >>= 2;
        }
        double entropy = 0;
        for (uint64_t i = 0; i < 4; ++i)
        {
            if (counts[i] > 0)
            {
                double p = (double)counts[i] / pSize;
                entropy -= p * log(p);
            }
        }
        return entropy * sInvLog2;
    }

    template<typename T>
    double entropyOrder1(const T& pNodeOrEdge, uint64_t pSize)
    {
        const double sInvLog2 = 1.0 / M_LN2;
        uint8_t counts[4][4] = {
            { 0, 0, 0, 0 },
            { 0, 0, 0, 0 },
            { 0, 0, 0, 0 },
            { 0, 0, 0, 0 }
        };
        Gossamer::position_type mer = pNodeOrEdge.value();
        for (uint64_t i = 0; i < pSize-1; ++i)
        {
            ++counts[mer & 0x3][(mer & 0xc) >> 2];
            mer >>= 2;
        }
        double entropy = 0;
        for (uint64_t i = 0; i < 4; ++i)
        {
            for (uint64_t j = 0; j < 4; ++j)
            {
                if (counts[i][j] > 0)
                {
                    double p = (double)counts[i][j] / (pSize-1);
                    entropy -= p * log(p);
                }
            }
        }
        return entropy * sInvLog2;
    }

    struct FindSeedEdgeThread;
    typedef std::shared_ptr<FindSeedEdgeThread> FindSeedEdgeThreadPtr;

    struct FindSeedEdgeThread : public MultithreadedBatchTask::WorkThread
    {
        uint64_t mThreadId;
        const Graph& mGraph;
        uint64_t mBegin;
        uint64_t mEnd;
        const uint64_t mMinSeedCoverage;
        const double mMinSeedEntropy;
        deque<SeedInfo> mSeedEdges;

        typedef pair<Graph::Edge,uint32_t> count_type;

        virtual void operator()();

        FindSeedEdgeThread(int64_t pThreadId, const Graph& pGraph,
                           uint64_t pBegin, uint64_t pEnd,
                           uint64_t pMinSeedCoverage, double pMinSeedEntropy,
                           MultithreadedBatchTask& pTask)
            : MultithreadedBatchTask::WorkThread(pTask),
              mThreadId(pThreadId), mGraph(pGraph),
              mBegin(pBegin), mEnd(pEnd),
              mMinSeedCoverage(pMinSeedCoverage),
              mMinSeedEntropy(pMinSeedEntropy)
        {
        }
    };


    void
    FindSeedEdgeThread::operator()()
    {
        // const int64_t workQuantum = 1000000ll * numThreads();
        const int64_t workQuantum = (int64_t)(((uint64_t)((int64_t)-1)) >> 1);
        int64_t workUntilReport = workQuantum;
        uint64_t rho = mGraph.K() + 1;

        for (uint64_t i = mBegin; i < mEnd; ++i)
        {
            if (--workUntilReport <= 0)
            {
                if (!reportWorkDone(i - mBegin))
                {
                    return;
                }
                workUntilReport = workQuantum;
            }

            Graph::Edge e = mGraph.select(i);
            uint32_t c = mGraph.weight(i);

            if (!c || c < mMinSeedCoverage)
            {
                continue;
            }

            double e_ent = entropyOrder0(e, rho);
            if (e_ent < mMinSeedEntropy)
            {
                continue;
            }

            mSeedEdges.push_back(SeedInfo(i, c));
        }

        sort(mSeedEdges.begin(), mSeedEdges.end(), seedInfoGt);
        reportWorkDone(mEnd - mBegin);
    }


} // namespace anonymous


class TransPairAligner
{
public:

    void push_back(ReadPair pPair)
    {
        const GossRead& lhsRead(*pPair.first);
        const GossRead& rhsRead(*pPair.second);

        uint16_t rho = mGraph.K() + 1;

        SmallBaseVector lhs;
        if (!lhsRead.extractVector(lhs))
        {
            return;
        }

        SmallBaseVector rhs;
        if (!rhsRead.extractVector(rhs))
        {
            return;
        }

        mWhichComponent.clear();
        mWhichComponent.reserve(lhs.size() + rhs.size() - 2 * rho);

        for (uint64_t i = 0; i < lhs.size() - rho; ++i)
        {
            Graph::Edge e(lhs.kmer(rho, i));
            uint64_t rnk;
            if (!mGraph.accessAndRank(e, rnk))
            {
                continue;
            }
            if (mKmerPresent[rnk])
            {
                mWhichComponent.push_back(mComponentMap[rnk]);
            }
        }

        for (uint64_t i = 0; i < rhs.size() - rho; ++i)
        {
            Graph::Edge e(rhs.kmer(rho, i));
            uint64_t rnk;
            if (!mGraph.accessAndRank(e, rnk))
            {
                continue;
            }
            if (mKmerPresent[rnk])
            {
                mWhichComponent.push_back(mComponentMap[rnk]);
            }
        }

        if (!mWhichComponent.size())
        {
            return;
        }

        sort(mWhichComponent.begin(), mWhichComponent.end());

        uint32_t component = mWhichComponent.front();
        uint32_t count = 0;
        uint32_t curComponent = mWhichComponent.front();
        uint32_t curCount = 0;

        for (auto comp: mWhichComponent)
        {
            if (curComponent == comp)
            {
                ++count;
                if (curCount > count)
                {
                    component = curComponent;
                    count = curCount;
                }
            }
            else
            {
                curComponent = comp;
                curCount = 1;
            }
        }

        mWhichComponent.clear();

        deque<uint8_t> v;
        VByteCodec::encode(component, v);
        lhs.encode(v);
        rhs.encode(v);

        mMappableReads += 2;

        {
            unique_lock<mutex> lk(mMutex);
            mComponentReadCount[component] += 2;
            mSorter.push_back(v);
        }
    }

    TransPairAligner(const Graph& pGraph, const vector<uint32_t>& pComponentMap,
               vector<uint64_t>& pComponentReadCount,
               const vector<bool>& pKmerPresent, ExternalBufferSort& pSorter,
               mutex& pMutex)
        : mGraph(pGraph), mComponentMap(pComponentMap),
          mComponentReadCount(pComponentReadCount),
          mKmerPresent(pKmerPresent), mMappableReads(0),
          mSorter(pSorter), mMutex(pMutex)
    {
    }

    uint64_t mappableReads() const
    {
        return mMappableReads;
    }

private:

    const Graph& mGraph;
    const vector<uint32_t>& mComponentMap;
    vector<uint64_t>& mComponentReadCount;
    const vector<bool>& mKmerPresent;
    vector<uint32_t> mWhichComponent;
    uint64_t mMappableReads;
    ExternalBufferSort& mSorter;
    mutex& mMutex;
};

typedef std::shared_ptr<TransPairAligner> TransPairAlignerPtr;


class SortThread
{
public:
    SortThread(ExternalBufferSort& pSorter)
        : mSorter(pSorter), mQueue(1024ULL),
          mThread(mem_fn(&SortThread::threadRun), this)
    {
    }

    ~SortThread()
    {
        // mThread.interrupt();
        mThread.join();
    }

    void push_back(vector<uint8_t>& pData);

    bool get(vector<uint8_t>& pData);

    void end()
    {
    }

private:
    void threadRun()
    {
        try {
            mSorter.sort(*this);
        }
        catch (...)
        {
        }
        mQueue.finish();
    }

    ExternalBufferSort& mSorter;
    BoundedQueue< vector<uint8_t> > mQueue;
    thread mThread;
};


void
SortThread::push_back(vector<uint8_t>& pData)
{
    mQueue.put(pData);
}


bool
SortThread::get(vector<uint8_t>& pData)
{
    return mQueue.get(pData);
}


struct TransCmdAssemble : public GossCmd
{
    TransCmdAssemble(
         const std::string& pIn, uint64_t pMinCoverage,
         double pMinConnectivityRatio,
         uint64_t pMinSeedCoverage, double pMinSeedEntropy,
         uint64_t pMinLength, uint64_t pNumThreads,
         const strings& pFastas, const strings& pFastqs,
         const strings& pLines, const std::string& pOut)
    : mIn(pIn), mMinCoverage(pMinCoverage),
      mMinConnectivityRatio(pMinConnectivityRatio),
      mMinSeedCoverage(pMinSeedCoverage), mMinSeedEntropy(pMinSeedEntropy),
      mMinLength(pMinLength), mNumThreads(pNumThreads),
      mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), mOut(pOut)
    {
    }

    const string mIn;
    const uint64_t mMinCoverage;
    const double mMinConnectivityRatio;
    const uint64_t mMinSeedCoverage;
    const double mMinSeedEntropy;
    const uint64_t mMinLength;
    const uint64_t mNumThreads;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const string mOut;

    GraphPtr mGPtr;
    dynamic_bitset<> mSeen;

    void findSeedEdges(Logger& pLog, deque<SeedInfo>& pSeedEdges);

    bool step(Logger& pLog, const EdgeInfo& pFrom, bool pFwd, EdgeInfo& pNextEdge) const;

    void scan(Logger& pLog, deque<EdgeInfo>& pEdges, const EdgeInfo& pEdge, bool pFwd);

    void operator()(const GossCmdContext& pCxt);

    void initialiseReads(std::deque<GossReadSequence::Item>& pItems);
};


void
TransCmdAssemble::initialiseReads(std::deque<GossReadSequence::Item>& pItems)
{
    GossReadSequenceFactoryPtr seqFac
        = std::make_shared<GossReadSequenceBasesFactory>();

    GossReadParserFactory lineParserFac(LineParser::create);
    for (auto& f: mLines)
    {
        pItems.push_back(GossReadSequence::Item(f, lineParserFac, seqFac));
    }

    GossReadParserFactory fastaParserFac(FastaParser::create);
    for (auto& f: mFastas)
    {
        pItems.push_back(GossReadSequence::Item(f, fastaParserFac, seqFac));
    }

    GossReadParserFactory fastqParserFac(FastqParser::create);
    for (auto& f: mFastqs)
    {
        pItems.push_back(GossReadSequence::Item(f, fastqParserFac, seqFac));
    }
}


void
TransCmdAssemble::findSeedEdges(Logger& pLog, deque<SeedInfo>& pSeedEdges)
{
    Graph& g(*mGPtr);

    uint64_t N = g.count();
    uint64_t J = mNumThreads;
    uint64_t S = N / J;
    vector<uint64_t> threadStarts;
    threadStarts.reserve(J + 1);

    for (uint64_t i = 0; i < J; ++i)
    {
        threadStarts.push_back(i * S);
    }
    threadStarts.push_back(N);

    vector<FindSeedEdgeThreadPtr> blocks;
    {
        ProgressMonitorNew mon(pLog, N);
        MultithreadedBatchTask task(mon);

        for (uint64_t i = 0; i < threadStarts.size() - 1; ++i)
        {
            uint64_t b = threadStarts[i];
            uint64_t e = threadStarts[i+1];
            if (b != e)
            {
                FindSeedEdgeThreadPtr blk(
                    new FindSeedEdgeThread(i, g, b, e,
                        mMinSeedCoverage, mMinSeedEntropy, task));
                blocks.push_back(blk);
                task.addThread(blk);
            }
        }

        pLog(info, "Pass 1 - processing seed edges");
        task();
    }

    pLog(info, "Done.");

    deque<deque<SeedInfo> > seedEdgeRuns;
    for (uint64_t i = 0; i < blocks.size(); ++i)
    {
        FindSeedEdgeThread* blk = blocks[i].get();
        seedEdgeRuns.push_back(deque<SeedInfo>());
        blk->mSeedEdges.swap(seedEdgeRuns.back());
    }

    while (seedEdgeRuns.size() > 1)
    {
        deque<SeedInfo> a;
        a.swap(seedEdgeRuns.front());
        seedEdgeRuns.pop_front();
        deque<SeedInfo> b;
        b.swap(seedEdgeRuns.front());
        seedEdgeRuns.pop_front();

        deque<SeedInfo> c;
        merge(a.begin(), a.end(), b.begin(), b.end(),
              back_insert_iterator< deque<SeedInfo> >(c),
              seedInfoGt);

        seedEdgeRuns.push_back(deque<SeedInfo>());
        seedEdgeRuns.back().swap(c);
    }

    if (!seedEdgeRuns.empty())
    {
        pSeedEdges.swap(seedEdgeRuns.front());
    }
}


bool
TransCmdAssemble::step(Logger& pLog, const EdgeInfo& pFrom, bool pFwd, EdgeInfo& pNextEdge) const
{
    const double sMinEntropy = 0.0;

    Graph& g(*mGPtr);
    uint32_t K = g.K();

    std::vector<EdgeInfo> nextEdges;
    nextEdges.reserve(4);

    // Find the next candidates.
    pair<rank_type,rank_type> nodeRank
        = pFwd ? g.beginEndRank(g.to(pFrom.mEdge))
               : g.beginEndRank(g.to(g.reverseComplement(pFrom.mEdge)));

    for (rank_type r = nodeRank.first; r < nodeRank.second; ++r)
    {
        EdgeInfo e;

        if (pFwd)
        {
            if (mSeen[r])
            {
                continue;
            }
            e.mRank = r;
            e.mEdge = g.select(r);
        }
        else
        {
            e.mEdge = g.reverseComplement(g.select(r));
            e.mRank = g.rank(e.mEdge);
            if (mSeen[e.mRank])
            {
                continue;
            }
        }

        e.mCount = g.multiplicity(e.mRank);

        Graph::Node nextNode = pFwd ? g.to(e.mEdge) : g.from(e.mEdge);

        double connectivityRatio = (pFrom.mCount > e.mCount)
                ? ((double)e.mCount/pFrom.mCount)
                : ((double)pFrom.mCount/e.mCount);

#ifdef VERBOSE_TRACE_CONTIGS
        {
            SmallBaseVector seq;
            g.seq(e.mEdge, seq);
            pLog(info, "Potential next edge: " + lexical_cast<string>(seq));
            pLog(info, "Count: " + lexical_cast<string>(e.mCount));
            pLog(info, "Entropy of next node: " + lexical_cast<string>(entropyOrder0(nextNode, K)));
            pLog(info, "Connectivity ratio: " + lexical_cast<string>(connectivityRatio));
        }
#endif

        if (!e.mCount || e.mCount < mMinCoverage
            || entropyOrder0(nextNode, K) < sMinEntropy
            || connectivityRatio < mMinConnectivityRatio)
        {
            continue;
        }

        nextEdges.push_back(e);
    }

    switch (nextEdges.size())
    {
        case 0:
        {
            // No edges.
#ifdef VERBOSE_TRACE_CONTIGS
            pLog(info, "No suitable edges.");
#endif
            return false;
        }

        case 1:
        {
            // One edge.
            pNextEdge = nextEdges.front();
#ifdef VERBOSE_TRACE_CONTIGS
            SmallBaseVector seq;
            g.seq(pNextEdge.mEdge, seq);
            pLog(info, "One suitable edge: " + lexical_cast<string>(seq));
#endif
            return true;
        }

        case 2:
        {
            // Two edges.
            const EdgeInfo& e1 = nextEdges[0];
            const EdgeInfo& e2 = nextEdges[1];
            pNextEdge = e1.mCount >= e2.mCount ? e1 : e2;
#ifdef VERBOSE_TRACE_CONTIGS
            pLog(info, "Two suitable edges.");
            SmallBaseVector seq;
            g.seq(pNextEdge.mEdge, seq);
            pLog(info, "Picking: " + lexical_cast<string>(seq));
#endif
            return true;
        }

        default:
        {
            // Lots of edges.  Choose the one with the highest coverage.

            // Not the only possible option, of course.  We could pick
            // the one with the coverage ratio that's closest to one.
            // Or we could filter out any insignificant ones and try
            // them all recursively.
            //
            // It probably makes no difference, given that "false"
            // breaks will be fixed by welding.

            vector<EdgeInfo>::iterator it = nextEdges.begin();
            pNextEdge = *it;
            for (++it; it != nextEdges.end(); ++it)
            {
                if (it->mCount > pNextEdge.mCount)
                {
                    pNextEdge = *it;
                }
            }
#ifdef VERBOSE_TRACE_CONTIGS
            pLog(info, lexical_cast<string>(nextEdges.size()) + " suitable edges.");
            SmallBaseVector seq;
            g.seq(pNextEdge.mEdge, seq);
            pLog(info, "Picking: " + lexical_cast<string>(seq));
#endif

            return true;
        }
    }
}


void
TransCmdAssemble::scan(Logger& pLog, deque<EdgeInfo>& pEdges,
                       const EdgeInfo& pEdge, bool pFwd)

{
    EdgeInfo edge = pEdge;

    for (;;)
    {
        EdgeInfo nextEdge;
        if (!step(pLog, edge, pFwd, nextEdge))
        {
            break;
        }
        if (pFwd)
        {
            pEdges.push_back(nextEdge);
        }
        else
        {
            pEdges.push_front(nextEdge);
        }
        mSeen[nextEdge.mRank] = true;
        edge = nextEdge;
    }
}


struct ContigWeldGraph
{
    struct AdjListEntry
    {
        uint32_t mVertTo;
        uint32_t mCount;

        bool operator<(const AdjListEntry& pRhs) const
        {
            return mVertTo < pRhs.mVertTo;
        }

        AdjListEntry(uint32_t pVertTo = 0, uint32_t pCount = 0)
            : mVertTo(pVertTo), mCount(pCount)
        {
        }
    };

    // TODO: This wasn't a great idea, as it turns out.
    struct AdjList
    {
        static const uint32_t sMinOrder = 4;

        mutex mMutex;
        uint32_t mOrder;
        vector<AdjListEntry> mMain;
        deque<AdjListEntry> mOverflow;
        
        void copyOverflowToMain(bool pMinimiseStorage = false)
        {
            if (mOverflow.empty())
            {
                return;
            }

            size_t newSize = mMain.size() + mOverflow.size();
            size_t orderSize = size_t(1) << mOrder;
            if (newSize > orderSize)
            {
                ++mOrder;
                orderSize *= 2;
            }

            mMain.reserve(pMinimiseStorage ? newSize : max(orderSize, newSize));
            sort(mOverflow.begin(), mOverflow.end());
            vector<AdjListEntry>::iterator mid = mMain.end();
            copy(mOverflow.begin(), mOverflow.end(),
                back_insert_iterator< vector<AdjListEntry> >(mMain));
            inplace_merge(mMain.begin(), mid, mMain.end());
            mOverflow.clear();
        }

        AdjList()
          : mOrder(sMinOrder)
        {
        }

        AdjListEntry& search(uint32_t pVertTo)
        {
            // Search in main first.
            vector<AdjListEntry>::iterator ii
                = lower_bound(mMain.begin(), mMain.end(),
                              AdjListEntry(pVertTo, 0));
            if (ii != mMain.end() && ii->mVertTo == pVertTo)
            {
                return *ii;
            }

            // Now search in the overflow.
            for (auto& entry: mOverflow)
            {
                if (entry.mVertTo == pVertTo)
                {
                    return entry;
                }
            }

            if (mOverflow.size() >= mOrder)
            {
                copyOverflowToMain();
            }

            mOverflow.push_back(AdjListEntry(pVertTo, 0));

            return mOverflow.back();
        }
    };

    deque<ContigInfo> mContigInfo;
    ptr_vector<AdjList> mGraph;
    vector< vector<uint32_t> > mComponents;

    uint32_t addContig(const ContigInfo& pInfo)
    {
        mContigInfo.push_back(pInfo);
        return mContigInfo.size() - 1;
    }

    void solidifyGraph()
    {
        mGraph.resize(mContigInfo.size());
    }

    void addEdge(uint32_t pC1, uint32_t pC2)
    {
        if (pC1 > pC2)
        {
            swap(pC1, pC2);
        }
        AdjList& adjlist = mGraph[pC1];
        unique_lock<mutex> lock(adjlist.mMutex);
        AdjListEntry& edge = adjlist.search(pC2);
        ++edge.mCount;
    }

    void trimAndAssembleComponents(Logger& pLog)
    {
        vector_property_map<uint32_t> rank_map(mContigInfo.size());
        vector_property_map<uint32_t> parent_map(mContigInfo.size());
        disjoint_sets< vector_property_map<uint32_t>,
                       vector_property_map<uint32_t> >
            sets(rank_map, parent_map);

        for (uint32_t i = 1; i < mContigInfo.size(); ++i)
        {
            sets.make_set(i);
        }

        for (uint32_t v1 = 0; v1 < mGraph.size(); ++v1)
        {
            AdjList& adjlist = mGraph[v1];
            adjlist.copyOverflowToMain(true);

            const uint32_t sMinEdgeCoverage = 1;
            const double sCoverageFactor = 0.04;

            deque<AdjListEntry> keepEdges;

            for (auto& edge: adjlist.mMain)
            {
                uint32_t v2 = edge.mVertTo;
                uint32_t coverage = edge.mCount;

                if (coverage < sMinEdgeCoverage)
                {
                    continue;
                }

                const ContigInfo& contig1 = mContigInfo[v1];
                const ContigInfo& contig2 = mContigInfo[v2];

                if (coverage < sCoverageFactor * contig1.mAvgCount)
                {
                    continue;
                }

                if (coverage < sCoverageFactor * contig2.mAvgCount)
                {
                    continue;
                }

                keepEdges.push_back(edge);
                sets.union_set(v1, v2);
            }

            vector<AdjListEntry> newEdges(keepEdges.size());
            copy(keepEdges.begin(), keepEdges.end(), newEdges.begin());
            newEdges.swap(adjlist.mMain);
        }

        sets.compress_sets(
            make_counting_iterator<uint32_t>(1),
            make_counting_iterator<uint32_t>(mContigInfo.size()));

        vector<uint32_t> cs;
        cs.reserve(mContigInfo.size());

        for (uint32_t i = 1; i < mContigInfo.size(); ++i)
        {
            cs.push_back(get(parent_map,i));
        }

        Gossamer::sortAndUnique(cs);

        uint32_t components = cs.size();

        mComponents.resize(components);

        for (uint32_t i = 1; i < mContigInfo.size(); ++i)
        {
            vector<uint32_t>::const_iterator
                ii = lower_bound(cs.begin(), cs.end(), get(parent_map,i));
            mComponents[ii - cs.begin()].push_back(i);
        }
    }

};


class ContigLinker
{
public:
    struct Alignment
    {
        std::vector<uint32_t> mContigs;
        std::vector<uint16_t> mPositions;
        std::vector<Graph::Edge> mKmers;

        void reserve(uint64_t pAmt)
        {
            mContigs.reserve(pAmt);
            mPositions.reserve(pAmt);
            mKmers.reserve(pAmt);
        }

        void clear()
        {
            mContigs.clear();
            mPositions.clear();
            mKmers.clear();
        }
    };

    bool alignRead(GossReadPtr pRead, Alignment& pAlign)
    {
        pAlign.clear();

        mBasesInReads += pRead->length();
        uint32_t K = mGraph.K();
        uint32_t alignedKmers = 0;

        for (GossRead::Iterator it(*pRead, K+1); it.valid(); ++it)
        {
            Graph::Edge e(it.kmer());
            pAlign.mKmers.push_back(e);
            uint64_t rnk;
            if (mGraph.accessAndRank(e, rnk))
            {
                ++alignedKmers;
                pAlign.mContigs.push_back(mKmerToContigMap[rnk]);
                pAlign.mPositions.push_back(mKmerToPositionMap[rnk]);
            }
            else
            {
                pAlign.mContigs.push_back(0);
                pAlign.mPositions.push_back(0);
            }
        }

        return alignedKmers >= K;
    }

    void findWeld(const Alignment& pAlign)
    {
        const double sDivergence = 10.0;
        const double sMaxRatio = 100.0;

        uint32_t K = mGraph.K();
        uint64_t kk = K/2 - 1;
        for (size_t i = kk; i < pAlign.mContigs.size() - kk - 1; ++i)
        {
            uint32_t c1 = pAlign.mContigs[i];
            uint32_t c2 = pAlign.mContigs[i+1];
            if (!c1 || !c2 || c1 == c2)
            {
                continue;
            }

            uint16_t p1 = pAlign.mPositions[i];
            uint16_t p2 = pAlign.mPositions[i+1];

            bool goodSupportB = true;
            bool goodSupportLL = true;
            bool goodSupportRL = true;
            bool goodSupportLR = true;
            bool goodSupportRR = true;
            for (uint16_t j = 1; j <= kk; ++j)
            {
                goodSupportB &= pAlign.mContigs[i-j] == c1;
                goodSupportB &= pAlign.mContigs[i+j+1] == c2;

                goodSupportLL &= pAlign.mPositions[i-j] == p1 - j;
                goodSupportLR &= pAlign.mPositions[i-j] == p1 + j;
                goodSupportRL &= pAlign.mPositions[i+j+1] == p2 - j;
                goodSupportRR &= pAlign.mPositions[i+j+1] == p2 + j;
            }
            if (!goodSupportB
                || (!goodSupportLL && !goodSupportLR)
                || (!goodSupportRL && !goodSupportRR))
            {
                continue;
            }

            const ContigInfo& contig1 = mCtgWeldGraph.mContigInfo[c1];
            const ContigInfo& contig2 = mCtgWeldGraph.mContigInfo[c2];

            Graph::Node n = mGraph.to(pAlign.mKmers[i]);

            if (entropyOrder0(n, K) < mMinSeedEntropy
                || dinucleotideRepeat(n, K))
            {
                return;
            }

            double ratio = (double)contig1.mAvgCount / contig2.mAvgCount;
            if (ratio < 1.0) ratio = 1.0 / ratio;
            if (ratio > sMaxRatio)
            {
                return;
            }

            double d1 = sqrt(contig1.mAvgCount);
            double d2 = sqrt(contig2.mAvgCount);
            double mean = 0.5 * (contig1.mAvgCount + contig2.mAvgCount);
            double delta = d1 > d2 ? d1 : d2;
            double mu = d1 > d2 ? contig1.mAvgCount : contig2.mAvgCount;
            if (mu - mean  > sDivergence * delta)
            {
                return;
            }

            mCtgWeldGraph.addEdge(c1, c2);
        }
    }


    bool majorityContig(const Alignment& pAlign, uint32_t& pContig)
    {
        const uint32_t sMinEdges = 2;

        uint32_t bestContig = 0;
        uint32_t bestSize = 0;
        uint32_t curContig = 0;
        uint32_t curSize = 0;
        bool curSense = false;

        for (size_t i = 1; i < pAlign.mContigs.size(); ++i)
        {
            uint32_t c1 = pAlign.mContigs[i-1];
            uint32_t c2 = pAlign.mContigs[i];
            uint32_t p1 = pAlign.mPositions[i-1];
            uint32_t p2 = pAlign.mPositions[i];

            if (curContig)
            {
                // We are tracking a contig.
                if (c2 == curContig
                    && (curSense ? p1 + 1 == p2 : p1 == p2 + 1))
                {
                    // Good continuation
                    ++curSize;
                    continue;
                }

                // Stop tracking.
                if (curSize > bestSize)
                {
                    bestContig = curContig;
                    bestSize = curSize;
                    curContig = 0;
                    curSize = 0;
                }
            }

            // Start tracking a new contig.
            if (c1 && c1 == c2)
            {
                if (p1 + 1 == p2)
                {
                    curContig = c1;
                    curSize = 1;
                    curSense = true;
                    continue;
                }
                else if (p1 == p2 + 1)
                {
                    curContig = c1;
                    curSize = 1;
                    curSense = false;
                    continue;
                }
            }

            // Not tracking.
            curContig = 0;
            curSize = 0;
        }

        pContig = bestContig;
        return bestSize >= sMinEdges;
    }


    void push_back(ReadPair pPair)
    {
        // Align reads and look for single-read welds.

        bool alignL = alignRead(pPair.first, mReadL);
        if (alignL)
        {
            findWeld(mReadL);
        }

        bool alignR = alignRead(pPair.first, mReadR);
        if (alignR)
        {
            findWeld(mReadR);
        }

        // If both reads align to contigs, potentially part of a scaffold.
        if (alignL && alignR)
        {
            uint32_t contigL, contigR;
            if (majorityContig(mReadL, contigL)
                && majorityContig(mReadR, contigR)
                && contigL != contigR)
            {
                mCtgWeldGraph.addEdge(contigL, contigR);
            }
        }
    }

    ContigLinker(const Graph& pGraph, ContigWeldGraph& pCtgWeldGraph,
            const vector<uint32_t>& pKmerToContigMap,
            const vector<uint16_t>& pKmerToPositionMap,
            double pMinSeedEntropy)
        : mGraph(pGraph), mCtgWeldGraph(pCtgWeldGraph),
          mKmerToContigMap(pKmerToContigMap),
          mKmerToPositionMap(pKmerToPositionMap),
          mMinSeedEntropy(pMinSeedEntropy), mBasesInReads(0)
    {
        mReadL.reserve(100);
        mReadR.reserve(100);
    }

    uint64_t basesInReads() const
    {
        return mBasesInReads;
    }

private:
    const Graph& mGraph;
    ContigWeldGraph& mCtgWeldGraph;
    const vector<uint32_t>& mKmerToContigMap;
    const vector<uint16_t>& mKmerToPositionMap;
    const double mMinSeedEntropy;
    uint64_t mBasesInReads;
    Alignment mReadL;
    Alignment mReadR;
};

typedef std::shared_ptr<ContigLinker> ContigLinkerPtr;


void
TransCmdAssemble::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);
    Timer t;

    LineSourceFactory lineSrcFac(BackgroundLineSource::create);

    log(info, "Assembling transcripts");
    mGPtr = Graph::open(mIn, fac);
    Graph& g(*mGPtr);
    uint64_t K = g.K();

    mSeen.clear();
    mSeen.resize(g.count());

    deque<SeedInfo> seedEdges;
    log(info, "  Number of edges: " + lexical_cast<string>(g.count()));

    findSeedEdges(log, seedEdges);

    log(info, "  Number of seed edges: "
            + lexical_cast<string>(seedEdges.size()));

    log(info, "Pass 2 - assembling spanning contigs");

    std::vector<uint32_t> kmerToContigMap(g.count());
    std::vector<uint16_t> kmerToPositionMap(g.count());
    ContigWeldGraph linkGraph;
    {
        ProgressMonitorNew mon(log, seedEdges.size());

        linkGraph.addContig(ContigInfo(ContigInfo::Sentinel()));

        deque<EdgeInfo> edges;
        uint64_t contig = 1;
        uint64_t edgesInContigs = 0;
        const int64_t workQuantum = 100000ull;
        int64_t workUntilReport = workQuantum;

        for (uint64_t workDone = 0; !seedEdges.empty(); seedEdges.pop_front())
        {
            ++workDone;
            if (--workUntilReport <= 0)
            {
                mon.tick(workDone);
                workUntilReport = workQuantum;
            }

            SeedInfo s = seedEdges.front();
            if (mSeen[s.mRank])
            {
                continue;
            }

            EdgeInfo e(g.select(s.mRank), s.mRank, s.mCount);
            edges.push_back(e);
            mSeen[e.mRank] = true;

#ifdef VERBOSE_TRACE_CONTIGS
            {
                SmallBaseVector seq;
                g.seq(e.mEdge, seq);
                log(info, "Seed edge: " + lexical_cast<string>(seq));
            }
#endif

            EdgeInfo e_rc;
            e_rc.mEdge = g.reverseComplement(e.mEdge);
            e_rc.mRank = g.rank(e_rc.mEdge);
            e_rc.mCount = g.multiplicity(e_rc.mRank);

            // Scan forward
            scan(log, edges, e, true);

            // Scan reverse
            scan(log, edges, e, false);

            SmallBaseVector seq;
            g.seq(g.from(edges.front().mEdge), seq);
            uint64_t averageCoverage = 0;
            for (auto& edge: edges)
            {
                seq.push_back(edge.mEdge.value().value() & 3);
                averageCoverage += edge.mCount;
            }
            averageCoverage
                = (uint64_t)(((double)averageCoverage / edges.size()) + 0.5);

            if (seq.size() > (size_t)(~uint16_t(0)))
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << general_error_info("Internal error: sequence too big ("
                        + lexical_cast<string>(seq.size()) + ")"));
            }

            if (seq.size() >= 2*K && averageCoverage >= mMinCoverage)
            {
                linkGraph.addContig(ContigInfo(averageCoverage, g, seq));

                edgesInContigs += edges.size();
                for (uint64_t j = 0; j < edges.size(); ++j)
                {
                    const EdgeInfo& edge = edges[j];
                    kmerToContigMap[edge.mRank] = contig;
                    kmerToPositionMap[edge.mRank] = (uint16_t)j;
                    Graph::Edge e_rc = g.reverseComplement(edge.mEdge);
                    uint64_t rnk_rc = g.rank(e_rc);
                    kmerToContigMap[rnk_rc] = contig;
                    kmerToPositionMap[rnk_rc]
                        = (uint16_t)(edges.size() - j - 1);
                }
                ++contig;
#ifdef DUMP_CONTIGS
                std::cerr << ">contig-" << contig << '\n' << seq << '\n';
                std::cerr.flush();
#endif
            }

            // Mark the RC edges too.
            for (auto& edge: edges)
            {
                Graph::Edge e_rc = g.reverseComplement(edge.mEdge);
                mSeen[g.rank(e_rc)] = true;
            }

            edges.clear();
        }
        mon.end();
        log(info, "  Number of contigs: " + lexical_cast<string>(contig));
        log(info, "  Number of edges in contigs: "
            + lexical_cast<string>(edgesInContigs));
        
        {
            std::stringstream ss;
            ss << "  Amount of graph covered by contigs: "
               << std::fixed << std::setprecision(2)
               << (edgesInContigs * 200.0 / g.count()) << '%';
            log(info, ss.str());
        }
    }

    linkGraph.solidifyGraph();
    uint64_t basesInReads = 0;
    {
        log(info, "Pass 3 - linking contigs");

        std::deque<GossReadSequence::Item> items;
        initialiseReads(items);

        UnboundedProgressMonitor umon(log, 100000, " pairs");
        ReadPairSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

        BackgroundMultiConsumer<ReadPair> grp(128);
        vector<ContigLinkerPtr> linkers;
        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            linkers.push_back(ContigLinkerPtr(new ContigLinker(
                g, linkGraph, kmerToContigMap, kmerToPositionMap,
                mMinSeedEntropy
            )));
            grp.add(*linkers.back());
        }

        while (reads.valid())
        {
            grp.push_back(make_pair(reads.lhs().clone(), reads.rhs().clone()));
            ++reads;
        }
        grp.wait();

        for (auto& plink: linkers)
        {
            basesInReads += plink->basesInReads();
        }

        vector<uint16_t>().swap(kmerToPositionMap);
    }

    FileFactory::OutHolderPtr outPtr(fac.out(mOut));
    ostream& out(**outPtr);

    vector<bool> kmerPresent;
    uint64_t numComponents;
    kmerPresent.reserve(kmerToContigMap.size());
    {
        log(info, "Extracting components");
        linkGraph.trimAndAssembleComponents(log);

        vector<uint32_t> contigToComponentMap(linkGraph.mContigInfo.size());

        {
            uint32_t i = 0;
            for (auto& component: linkGraph.mComponents)
            {
                for (auto ctg: component)
                {
                    contigToComponentMap[ctg] = i;
                }
                ++i;
            }
        }

        {
            uint64_t i = 0;
            for (auto& ctg: kmerToContigMap)
            {
                if (ctg)
                {
                    ctg = contigToComponentMap[ctg];
                    kmerPresent[i] = true;
                }
                ++i;
            }
        }
        numComponents = linkGraph.mComponents.size();
        log(info, "  Extracted " + lexical_cast<string>(numComponents)
                + " components");
    }

    ExternalBufferSort sorter(1024ULL * 1024ULL * 1024ULL, fac);
    uint64_t numNonEmptyComponents = 0;
    dynamic_bitset<uint64_t> nonEmptyComponents(numComponents);
    uint64_t totalMappableReads = 0;
    {
        log(info, "Pass 4 - mapping reads to contigs");

        std::deque<GossReadSequence::Item> items;
        initialiseReads(items);

        UnboundedProgressMonitor umon(log, 100000, " pairs");
        ReadPairSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

        BackgroundMultiConsumer<ReadPair> grp(128);
        vector<uint64_t> componentReadCount(numComponents);
        vector<TransPairAlignerPtr> linkers;
        linkers.reserve(mNumThreads);
        mutex mut;
        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            linkers.push_back(std::make_shared<TransPairAligner>(
                g, kmerToContigMap, componentReadCount, kmerPresent, sorter,
                mut));
            grp.add(*linkers.back());
        }

        while (reads.valid())
        {
            grp.push_back(make_pair(reads.lhs().clone(), reads.rhs().clone()));
            ++reads;
        }
        grp.wait();

        for (uint64_t i = 0; i < mNumThreads; ++i)
        {
            totalMappableReads += linkers[i]->mappableReads();
        }

        for (uint64_t i = 0; i < numComponents; ++i)
        {
            if (componentReadCount[i] >= ResolveTranscripts::sMinReads)
            {
               nonEmptyComponents[i] = true;
               ++numNonEmptyComponents;
            }
        }

        log(info, "  " + lexical_cast<string>(totalMappableReads) + " mappable reads");
        log(info, "  " + lexical_cast<string>(numNonEmptyComponents) + " non-empty components");
    }

    {
        log(info, "Pass 5 - processing components");

        SortThread queue(sorter);
        vector<uint8_t> read;

        bool moreInQueue = queue.get(read);
        if (!moreInQueue)
        {
            log(warning, "No reads appear to map to components");
            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
            return;
        }
        uint32_t alignedComponent;
        ResolveTranscripts::ReadInfo lhsRead;
        ResolveTranscripts::ReadInfo rhsRead;
        {
            vector<uint8_t>::const_iterator i = read.begin();
            alignedComponent = VByteCodec::decode(i);
            lhsRead.mRead.decode(i);
            rhsRead.mRead.decode(i);
        }

        uint32_t nonEmptyCompId = 0;
        uint32_t compId = 0;
        for (auto& component: linkGraph.mComponents)
        {
            ResolveTranscripts resolver(lexical_cast<string>(nonEmptyCompId),
                        g, log, out, mMinLength, totalMappableReads);

            typedef pair<ResolveTranscripts::ReadInfo,
                        ResolveTranscripts::ReadInfo> read_pair_info;
            vector<read_pair_info> readPairs;

            while (moreInQueue && alignedComponent < compId)
            {
                moreInQueue = queue.get(read);
                if (!moreInQueue)
                {
                    break;
                }
                vector<uint8_t>::iterator i = read.begin();
                alignedComponent = VByteCodec::decode(i);
                lhsRead.mRead.decode(i);
                rhsRead.mRead.decode(i);
            }

            while (moreInQueue && alignedComponent == compId)
            {
                moreInQueue = queue.get(read);
                if (!moreInQueue)
                {
                    break;
                }
                readPairs.push_back(read_pair_info(lhsRead, rhsRead));
                vector<uint8_t>::iterator i = read.begin();
                alignedComponent = VByteCodec::decode(i);
                lhsRead.mRead.decode(i);
                rhsRead.mRead.decode(i);
            }

            if (nonEmptyComponents[compId])
            {
#ifdef DUMP_COMPONENT_INTERMEDIATES
                out << "# BEGIN Contents of component " << nonEmptyCompId << " (" << compId << ")\n";
                out << "# Contigs:\n";
#endif

                for (auto ctg: component)
                {
                    const ContigInfo& info = linkGraph.mContigInfo[ctg];
                    SmallBaseVector seq;
                    info.decompressContig(g, seq);
#ifdef DUMP_COMPONENT_INTERMEDIATES
                    out << ">component" << compId << "--" << "contig" << ctg << "\n";
                    seq.print(out);
#endif
                    resolver.addContig(seq);
                }
#ifdef DUMP_COMPONENT_INTERMEDIATES
                unsigned readId = 0;
                out << "# Reads:\n";
#endif
                for (auto& readPair: readPairs)
                {
#ifdef DUMP_COMPONENT_INTERMEDIATES
                    out << ">component" << compId << "--" << "read" << readId << "/1\n";
                    readPair.first.mRead.print(out);
                    out << ">component" << compId << "--" << "read" << readId << "/2\n";
                    readPair.second.mRead.print(out);
                    ++readId;
#endif
                    resolver.addReadPair(readPair.first, readPair.second);
                }
#ifdef DUMP_COMPONENT_INTERMEDIATES
                out << "# END Contents of component " << nonEmptyCompId << " (" << compId << ")\n";
                out.flush();
#endif
                resolver.processComponent();
                ++nonEmptyCompId;
            }
            ++compId;
        }

        while (moreInQueue)
        {
            moreInQueue = queue.get(read);
            if (!moreInQueue)
            {
                break;
            }
            vector<uint8_t>::iterator i = read.begin();
            alignedComponent = VByteCodec::decode(i);
            lhsRead.mRead.decode(i);
            rhsRead.mRead.decode(i);
        }
    }

    mGPtr = GraphPtr();
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
TransCmdFactoryAssemble::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    uint64_t minCoverage = 1;
    chk.getOptional("min-coverage", minCoverage);

    uint64_t minSeedCoverage = 2;
    chk.getOptional("min-seed-coverage", minSeedCoverage);

    double minSeedEntropy = 1.5;
    chk.getOptional("min-seed-entropy", minSeedEntropy);

    double minConnectivityRatio = 0.0;
    chk.getOptional("min-connectivity-ratio", minConnectivityRatio);

    uint64_t l = 0;
    chk.getOptional("min-length", l);

    string out = "-";
    chk.getOptional("output-file", out, GossOptionChecker::FileCreateCheck(fac, false));

    uint64_t numThreads = 1;
    chk.getOptional("num-threads", numThreads);

    strings fastas;
    chk.getRepeating0("fasta-in", fastas, readChk);

    strings fastqs;
    chk.getRepeating0("fastq-in", fastqs, readChk);

    strings lines;
    chk.getRepeating0("line-in", lines, readChk);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new TransCmdAssemble(in, minCoverage, minConnectivityRatio, minSeedCoverage, minSeedEntropy, l, numThreads, fastas, fastqs, lines, out));
}


TransCmdFactoryAssemble::TransCmdFactoryAssemble()
    : GossCmdFactory("assemble the graph greedily")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("output-file");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("min-length");

    mSpecificOptions.addOpt<uint64_t>("min-seed-coverage", "", "minimum seed coverage");
    mSpecificOptions.addOpt<double>("min-seed-entropy", "", "minimum seed entropy");
    mSpecificOptions.addOpt<string>("components-output-file", "", "output of the components info file");
}

