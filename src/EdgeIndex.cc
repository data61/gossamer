// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "BackgroundMultiConsumer.hh"
#include "EdgeIndex.hh"
#include "FixedWidthBitArray.hh"

using namespace boost;
using namespace std;

constexpr uint64_t EdgeIndex::version;

namespace { // anonymous
    class EdgeCollector
    {
    public:
        bool operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            mRanks.push_back(pRank);
            return true;
        }

        EdgeCollector(vector<uint64_t>& pRanks)
            : mRanks(pRanks)
        {
        }

    private:
        vector<uint64_t>& mRanks;
    };

    class SegmentIndexer
    {
    public:
        
        void push_back(uint64_t pRank)
        {
            mRanks.clear();
            Graph::Edge e(mEntryEdges.select(pRank).value());
            EdgeCollector vis(mRanks);
            mGraph.linearPath(e, vis);
            for (uint64_t i = 0; i < mRanks.size(); ++i)
            {
                uint64_t r(mRanks[i]);
                if (!(r & mMask))
                {
                    BOOST_ASSERT((r >> mDiv) < mSegmentIndex.size());
                    mSegmentIndex[r >> mDiv] = EdgeIndex::SegmentAndOffset(pRank, i);
                }
            }
        }

        SegmentIndexer(const Graph& pGraph,
                       EdgeIndex::SegmentIndex& pSegmentIndex,
                       const EntryEdgeSet& pEntryEdges,
                       uint64_t pDiv)
            : mGraph(pGraph), mSegmentIndex(pSegmentIndex), mEntryEdges(pEntryEdges),
              mDiv(pDiv), mMask((1ULL << pDiv) - 1)
        {
        }

    private:
        
        const Graph& mGraph;
        EdgeIndex::SegmentIndex& mSegmentIndex;
        const EntryEdgeSet& mEntryEdges;
        const uint64_t mDiv;
        const uint64_t mMask;
        vector<uint64_t> mRanks;
    };

    typedef std::shared_ptr<SegmentIndexer> SegmentIndexerPtr;

} // namespace anonymous

void
EdgeIndex::write(const std::string& pBaseName, FileFactory& pFactory) const
{
    string name = pBaseName + "-edge-index";

    // mHeader
    {
        FileFactory::OutHolderPtr op(pFactory.out(name + ".header"));
        ostream& o(**op);
        Header h;
        h.version = version;
        h.div = mDiv;
        o.write(reinterpret_cast<const char*>(&h), sizeof(h));
    }

    // mSegmentIndex
    {
        MappedArray<uint64_t>::Builder segArr(name + ".segs", pFactory);
        for (SegmentIndex::const_iterator
             i = mSegmentIndex.begin(); i != mSegmentIndex.end(); ++i)
        {
            segArr.push_back(i->first);
            segArr.push_back(i->second);
        }
        segArr.end();
    }

    // mPathIndex
    {
        MappedArray<uint32_t>::Builder pathArr(name + ".paths", pFactory);
        FixedWidthBitArray<1>::Builder multiArr(name + ".multi", pFactory);
        for (uint64_t i = 0; i < mPathIndex.size(); ++i)
        {
            const PathIdAndOffset& x(mPathIndex[i]);
            pathArr.push_back(x.first);
            pathArr.push_back(x.second);
            multiArr.push_back(mMulti[i]);
        }
        pathArr.end();
        multiArr.end();
    }
}

unique_ptr<EdgeIndex>
EdgeIndex::read(const std::string& pBaseName, FileFactory& pFactory, const Graph& pGraph)
{
    string name = pBaseName + "-edge-index";

    // mHeader
    Header h;
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
                    << Gossamer::general_error_info("No edge index header file found.")
                    << Gossamer::version_mismatch_info(make_pair(SuperGraph::version, 0)));
        }
        (**ip).read(reinterpret_cast<char*>(&h), sizeof(h));
        if (h.version != EdgeIndex::version)
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << boost::errinfo_file_name(name + ".header")
                    << Gossamer::version_mismatch_info(make_pair(EdgeIndex::version, h.version)));
        }
    }
    
    unique_ptr<EdgeIndex> ix(new EdgeIndex(pGraph, h.div));

    // mSegmentIndex
    {
        
        MappedArray<uint64_t>::LazyIterator segItr(name + ".segs", pFactory);
        while (segItr.valid())
        {
            SegmentAndOffset so;
            so.first = *segItr;
            ++segItr;
            so.second = *segItr;
            ++segItr;
            ix->mSegmentIndex.push_back(so);
        }
    }

    // mPathIndex
    {
        MappedArray<uint32_t>::LazyIterator pathItr(name + ".paths", pFactory);
        FixedWidthBitArray<1>::LazyIterator multiItr(FixedWidthBitArray<1>::lazyIterator(name + ".multi", pFactory));
        const uint64_t sz(pathItr.size());
        uint64_t i = 0;
        ix->mPathIndex.resize(sz, PathIdAndOffset(0, 0));
        ix->mMulti.resize(sz);
        while (pathItr.valid())
        {
            PathId id(*pathItr);
            ++pathItr;
            SegmentOffset ofs(*pathItr);
            ++pathItr;
            ix->mPathIndex[i] = PathIdAndOffset(id, ofs);
            ix->mMulti[i] = *multiItr;
            ++multiItr;
        }
    }

    return ix;
}

unique_ptr<EdgeIndex>
EdgeIndex::create(const Graph& pGraph, const EntryEdgeSet& pEntryEdges,
                  const SuperGraph& pSuper, uint64_t pDiv, uint64_t pNumThreads, 
                  Logger& pLog)
{
    BOOST_ASSERT(pEntryEdges.count() < numeric_limits<SegmentRank>::max());

    unique_ptr<EdgeIndex> ix(new EdgeIndex(pGraph, pDiv));
    uint64_t numEdges = 1 + (pGraph.count() >> pDiv);
    ix->mSegmentIndex.resize(numEdges);

    // Construct the segment index.
    LOG(pLog, info) << "constructing segment index";

    BackgroundMultiConsumer<uint64_t> grp(128);
    vector<SegmentIndexerPtr> indexers;
    for (uint64_t i = 0; i < pNumThreads; ++i) 
    {       
        indexers.push_back(SegmentIndexerPtr(
                            new SegmentIndexer(pGraph, ix->mSegmentIndex, pEntryEdges, pDiv)));
        grp.add(*indexers.back());
    }               

    for (uint64_t seg = 0; seg < pEntryEdges.count(); ++seg)
    {
        grp.push_back(seg);
    }
    grp.wait();

    // Construct the path index.

    // First, we count the number of times each segment is referenced.
    LOG(pLog, info) << "counting segments";
    const uint64_t numEntryEdges(pEntryEdges.count());
    vector<uint32_t> segCounts(numEntryEdges, 0);
    for (SuperGraph::PathIterator i(pSuper); i.valid(); ++i)
    {
        const SuperPath p = pSuper[*i];
        const vector<SuperPath::Segment>& seg(p.segments());
        for (uint64_t j = 0; j < seg.size(); ++j)
        {
            if (seg[j].isLinearPath())
            {
                segCounts[seg[j]]++;
            }
        }
    }

    // Using the segment use counts, we can index those that are unique
    LOG(pLog, info) << "constructing path index";
    ix->mPathIndex.resize(numEntryEdges);
    ix->mMulti.resize(numEntryEdges);
    for (SuperGraph::PathIterator i(pSuper); i.valid(); ++i)
    {
        const SuperPath p = pSuper[*i];
        const vector<SuperPath::Segment>& seg(p.segments());
        int64_t l = 0;
        for (uint64_t j = 0; j < seg.size(); ++j)
        {
            const SuperPath::Segment s = seg[j];
            if (s.isLinearPath())
            {
                if (segCounts[s] != 1)
                {
                    ix->mMulti[s] = true;
                }
                else
                {
                    ix->mPathIndex[s] = PathIdAndOffset((*i).value(), l);
                    // cerr << (*i).value() << '\t' << l << '\n';
                }
            }
            else if (s.isGap())
            {
                l += pEntryEdges.K();
            }
            const int64_t sl = s.length(pEntryEdges);
            if (sl < 0 && -sl > l)
            {
                l = 0;
                continue;
            }
            l += sl;
        }
    }

    LOG(pLog, info) << "completed edge index construction";

    return ix;
}

EdgeIndex::EdgeIndex(const Graph& pGraph, uint64_t pDiv)
    : mDiv(pDiv), mGraph(pGraph), mSegmentIndex(), mPathIndex(), mMulti()
{
}
