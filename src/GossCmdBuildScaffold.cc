// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildScaffold.hh"

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
#include <boost/tuple/tuple.hpp>
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
    Debug showStats("show-stats", "show the distribution of distances of pairs within a superpath.");

    template <typename Dest>
    class LinkMapCompiler
    {
    public:
        void push_back(const vector<uint8_t>& pItem)
        {
            const uint8_t* hd(&pItem[0]);
            PairLink::decode(hd, mLink);

            // cerr << "c\t" << mLink.lhs.value() << '\t' << mLink.rhs.value() << '\n';

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

    // TODO: Consider, for mate-pairs, eliminating links that join short contigs.
    // e.g. contigs shorter than 1/2 the insert size.
    // Alternatively - in downstream processing - only consider mate-pairs links that
    // join otherwise disconnected components.
    template <typename Dest>
    class LinkFilter
    {
    public:
        void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                       const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                       const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
        {
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

            // cerr << "f\t" << pLhs.value() << '\t' << pRhs.value() << '\t' << pCount << '\n';

            mDest.push_back(pLhs, pRhs, pCount,
                            pLhsOffsetSum, pLhsOffsetSum2, pRhsOffsetSum, pRhsOffsetSum2);
        }

        void end()
        {
            mDest.end();
        }

        LinkFilter(Dest& pDest, uint64_t pMaxInsertSize,
                   const SuperGraph& pSuperGraph, const EntryEdgeSet& pEntries)
            : mDest(pDest), mMaxInsertSize(pMaxInsertSize),
              mSuperGraph(pSuperGraph), mEntries(pEntries), mK(mEntries.K())
        {
        }

    private:
        Dest& mDest;
        int64_t mMaxInsertSize;
        const SuperGraph& mSuperGraph;
        const EntryEdgeSet& mEntries;
        const uint64_t mK;
    };

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
            }

            // Calculate the mean and std. dev. of the bounded region.
            double sum = 0;
            double sum2 = 0;
            uint64_t c = 0;
            LOG(pLog, info) << "left: " << left->first << '\t'
                            << "right: " << right->first << '\t'
                            << "mid: " << mid->first;
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

    string nextScafFile(FileFactory& pFac, const string& mIn)
    {
        uint64_t i = 0;
        string filename;
        do
        {
            filename = mIn + "-scaf" + lexical_cast<string>(i++);
        }
        while (pFac.exists(filename + ".header"));
        return filename;
    }

} // namespace anonymous

void
GossCmdBuildScaffold::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);

    Timer t;

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

    map<int64_t,uint64_t> dist;
    ExternalBufferSort sorter(1024ULL * 1024ULL * 1024ULL, fac);

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
            items.push_back(GossReadSequence::Item(f, lineParserFac, seqFac));
        }

        GossReadParserFactory fastaParserFac(FastaParser::create);
        for (auto& f: mFastas)
        {
            items.push_back(GossReadSequence::Item(f, fastaParserFac, seqFac));
        }

        GossReadParserFactory fastqParserFac(FastqParser::create);
        for (auto& f: mFastqs)
        {
            items.push_back(GossReadSequence::Item(f, fastqParserFac, seqFac));
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

    if (showStats.on())
    {
        LOG(log, info) << "Distribution of distances of pairs mapped to a single superpath.";
        for (PairLinker::Hist::const_iterator
             itr = dist.begin(); itr != dist.end(); ++itr)
        {
            LOG(log, info) << itr->first << ": " << itr->second;
        }
        LOG(log, info) << "-------------";
    }

    reportDistStats(dist, log);

    const double dev = mInsertTolerance * mInsertStdDevFactor * mExpectedInsertSize;
    const uint64_t maxInsertSize = mExpectedInsertSize + dev;
    const uint64_t minInsertSize = mExpectedInsertSize - dev;
    const uint64_t insertRange = 2 * dev;

    LOG(log, info) << "expected insert size = " << mExpectedInsertSize;
    LOG(log, info) << "insert std dev. factor = " << mInsertStdDevFactor;
    LOG(log, info) << "insert tolerance = " << mInsertTolerance;
    LOG(log, info) << "max insert size = " << maxInsertSize;
    LOG(log, info) << "min insert size = " << minInsertSize;

    string filename = ScaffoldGraph::nextScafBaseName(pCxt, mIn);
    LOG(log, info) << "writing scaffold to " << filename;

    ScaffoldGraph::Builder builder(filename, fac, sg, mExpectedInsertSize, insertRange, mOrientation);
    LinkFilter<ScaffoldGraph::Builder> filt(builder, maxInsertSize, sg, entries);
    LinkMapCompiler<LinkFilter<ScaffoldGraph::Builder> > comp(filt);
    sorter.sort(comp);
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

GossCmdPtr
GossCmdFactoryBuildScaffold::create(App& pApp, const variables_map& pOpts)
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

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildScaffold(in, fastas, fastqs, lines,
            inferCoverage, expectedCoverage, expectedSize, stdDevFactor,
            tolerance, o, T, cr, estimateOnly));
}

GossCmdFactoryBuildScaffold::GossCmdFactoryBuildScaffold()
    : GossCmdFactory("build a scaffold graph from a pair library")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("expected-coverage");
    mCommonOptions.insert("edge-cache-rate");
    mCommonOptions.insert("estimate-only");
    mCommonOptions.insert("paired-ends");
    mCommonOptions.insert("mate-pairs");
    mCommonOptions.insert("innies");
    mCommonOptions.insert("outies");
    mCommonOptions.insert("insert-expected-size");
    mCommonOptions.insert("insert-size-std-dev");
    mCommonOptions.insert("insert-size-tolerance");
}
