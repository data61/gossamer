// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "EntryEdgeSet.hh"

#include "Debug.hh"
#include "GossamerException.hh"
#include "ProgressMonitor.hh"
#include "WorkQueue.hh"

#include <boost/lambda/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <memory>
#include <boost/math/special_functions/round.hpp>
#include <istream>

using namespace boost;
using namespace std;

namespace // anonymous
{

string edge(uint64_t pK, const Gossamer::position_type& pX)
{
    string s;
    Gossamer::position_type x(pX);
    for (uint64_t i = 0; i < pK; ++i)
    {
        s.push_back("ACGT"[x & 3]);
        x >>= 2;
    }
    return string(s.rbegin(), s.rend());
}

struct PathVisitor
{
    double meanCount() const
    {
        return double(mCountSum) / double(mLength);
    }

    uint64_t length() const
    {
        return mLength;
    }

    bool operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
    {
        ++mLength;
        mCountSum += mGraph.multiplicity(pRank);
        return true;
    }

    PathVisitor(const Graph& pGraph) :
        mGraph(pGraph), mLength(0), mCountSum(0)
    {
    }

    const Graph& mGraph;
    uint64_t mLength;
    uint64_t mCountSum;
};

struct BatchBuilder
{

    void operator()()
    {
        for (uint64_t r = mBegin; r != mEnd; ++r)
        {
            Graph::Edge e = mGraph.select(r);
            Graph::Node f = mGraph.from(e);
            if (mGraph.inDegree(f) != 1 || mGraph.outDegree(f) != 1)
            {
                mEdges[r - mBegin] = true;

                // Find the number of edges in the path, and its mean count.
                PathVisitor vis(mGraph);
                Graph::Edge x = mGraph.linearPath(e, vis);
                uint32_t l = vis.length();
                uint32_t c = (uint32_t) boost::math::round(vis.meanCount());
                uint64_t rc = mGraph.rank(mGraph.reverseComplement(x));
                mLengths.push_back(l);
                mCounts.push_back(c);
                mRCs.push_back(rc);
                mHist[c]++;
            }
        }
        
        std::unique_lock<std::mutex> lk(mMutex);
        ++mNumDone;
        mCond.notify_one();
    }

    uint64_t size() const
    {
        return mEnd - mBegin;
    }

    uint64_t count() const
    {
        return mCounts.size();
    }

    BatchBuilder(const Graph& pGraph, uint64_t pBegin, uint64_t pEnd,
                 std::mutex& pMutex, std::condition_variable& pCond, uint64_t& pNumDone)
        : mGraph(pGraph), mBegin(pBegin), mEnd(pEnd),
          mEdges(pEnd - pBegin), mCounts(), mLengths(), mRCs(), mHist(),
          mNumDone(pNumDone), mMutex(pMutex), mCond(pCond)
    {
    }
 
    const Graph& mGraph;
    const uint64_t mBegin;
    const uint64_t mEnd;
    dynamic_bitset<> mEdges;
    vector<uint32_t> mCounts;
    vector<uint32_t> mLengths;
    vector<uint64_t> mRCs;
    map<uint64_t, uint64_t> mHist;
    uint64_t& mNumDone;
    std::mutex& mMutex;
    std::condition_variable& mCond;
};

typedef std::shared_ptr<BatchBuilder> BatchBuilderPtr;

} // namespace anonymous

static Debug showEntryStats("show-entry-set-stats", "print some statistics about the number of entry sets");
static Debug dumpEntryGraph("dump-entry-sets-graph", "print all the edges in the input graph");
static Debug dumpEntrySansEnds("dump-entry-sets-intermediate", "print the entry edges prior to computing the corresponding ends");

EntryEdgeSet::Header::Header(const string& pFileName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr ip(pFactory.in(pFileName));
    istream& i(**ip);
    i.read(reinterpret_cast<char*>(this), sizeof(Header));
    if (version != EntryEdgeSet::version)
    {
        uint64_t v = EntryEdgeSet::version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(v, version)));
    }
}

void
EntryEdgeSet::build(const Graph& pGraph, const string& pBaseName, 
                    FileFactory& pFactory, Logger& pLog, uint64_t pThreads)
{
    Logger& log(pLog);
    std::mutex mtx;
    std::condition_variable cnd;

    LOG(log, info) << "Locating entry edges";
    const uint64_t numBatches(64 * pThreads);
    const uint64_t batchSize = pGraph.count() / numBatches;
    uint64_t numDone = 0;
    WorkQueue q(pThreads);
    vector<BatchBuilderPtr> batches;
    for (uint64_t i = 0; i < numBatches; ++i)
    {
        uint64_t begin = i * batchSize;
        uint64_t end = i == (numBatches - 1) ? pGraph.count() : (i + 1) * batchSize;
        batches.push_back(BatchBuilderPtr(new BatchBuilder(pGraph, begin, end, mtx, cnd, numDone)));
        q.push_back(std::bind<void>(std::ref(*batches.back())));
    }

    {
        uint64_t prevDone = 0;
        std::unique_lock<std::mutex> lk(mtx);
        while (numDone < numBatches)
        {
            cnd.wait(lk);
            if (numDone > prevDone)
            {
                mtx.unlock();
                prevDone = numDone;
                stringstream ss;
                double pct = 100 * double(numDone) / numBatches;
                ss << std::fixed << std::setprecision(2) << pct << "%";
                log(info, ss.str());
                mtx.lock();
            }
        }
    }

    q.wait();

    uint64_t n = 0;
    for (uint64_t i = 0; i < batches.size(); ++i)
    {
        n += batches[i]->count();
    }
    
    const uint64_t K(pGraph.K());
    const uint64_t rho(K + 1);
    {
        const Gossamer::position_type z = Gossamer::position_type(1) << (2 * rho);
        LOG(log, info) << "Writing entry edges";
        SparseArray::Builder es(pBaseName + ".edges", pFactory, z, n);
        VariableByteArray::Builder cs(pBaseName + ".counts", pFactory, n, 1.0 / 1024.0);
        VariableByteArray::Builder ls(pBaseName + ".lengths", pFactory, n, 1.0 / 256.0);

        for (uint64_t i = 0; i < batches.size(); ++i)
        {
            const BatchBuilder& b(*batches[i]);
            uint64_t j = 0;
            for (uint64_t k = 0; k < b.size(); ++k)
            {
                if (b.mEdges[k])
                {
                    uint64_t r = b.mBegin + k;
                    Graph::Edge e(pGraph.select(r));

                    es.push_back(e.value());
                    cs.push_back(b.mCounts[j]);
                    ls.push_back(b.mLengths[j]);
                    j += 1;
                }
            }
        }
        es.end(z);
        cs.end();
        ls.end();
    }

    LOG(log, info) << "Writing counts histogram";
    {
        map<uint64_t, uint64_t> hist;
        for (uint64_t i = 0; i < batches.size(); ++i)
        {
            const BatchBuilder& b(*batches[i]);
            for (map<uint64_t, uint64_t>::const_iterator
                 j = b.mHist.begin(); j != b.mHist.end(); ++j)
            {
                hist[j->first] += j->second;
            }
        }
        FileFactory::OutHolderPtr outp(pFactory.out(pBaseName + ".counts-hist.txt"));
        ostream& out(**outp);
        for (map<uint64_t,uint64_t>::const_iterator i = hist.begin(); i != hist.end(); ++i)
        {
            out << i->first << '\t' << i->second << endl;
        }
    }

    LOG(log, info) << "Writing end edges";
    {
        SparseArray es(pBaseName + ".edges", pFactory);
        IntegerArray::BuilderPtr xs = IntegerArray::builder(RankBits, pBaseName + ".ends", pFactory);

        for (uint64_t i = 0; i < batches.size(); ++i)
        {
            const BatchBuilder& b(*batches[i]);
            uint64_t j = 0;
            for (uint64_t k = 0; k < b.size(); ++k)
            {
                if (b.mEdges[k])
                {
                    Graph::Edge rc(pGraph.select(b.mRCs[j]));
                    IntegerArray::value_type x(es.rank(rc.value()));
                    xs->push_back(x);
                    j += 1;
                }
            }
        }
        (*xs).end();
    }

    {
        Header h;
        h.version = EntryEdgeSet::version;
        h.K = K;

        FileFactory::OutHolderPtr hoPtr(pFactory.out(pBaseName + ".header"));
        ostream& ho(**hoPtr);
        ho.write(reinterpret_cast<const char*>(&h), sizeof(Header));
    }
}

void
EntryEdgeSet::remove(const std::string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    SparseArray::remove(pBaseName + ".edges", pFactory);
    VariableByteArray::remove(pBaseName + ".counts", pFactory);
    VariableByteArray::remove(pBaseName + ".lengths", pFactory);
    IntegerArray::remove(pBaseName + ".ends", pFactory);
}

EntryEdgeSet::EntryEdgeSet(const std::string& pBaseName, FileFactory& pFactory)
    : mHeader(pBaseName + ".header", pFactory),
      mEdges(pBaseName + ".edges", pFactory),
      mCounts(pBaseName + ".counts", pFactory),
      mLengths(pBaseName + ".lengths", pFactory),
      mEndsHolder(IntegerArray::create(RankBits, pBaseName + ".ends", pFactory)),
      mEnds(*mEndsHolder)
{
    mM = (Gossamer::position_type(1) << (2 * K())) - 1;

    if (showEntryStats.on())
    {
        cerr << "mEdges.count() = " << mEdges.count() << endl;
    }
}

