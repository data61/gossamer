// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef KMERALIGNER_HH
#define KMERALIGNER_HH

#ifndef EDGEINDEX_HH
#include "EdgeIndex.hh"
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class KmerAligner
{
public:

    enum Dir { Forward, RevComp };

    bool operator()(Gossamer::position_type pKmer, SuperPathId& pSuperPathId)
    {
        uint64_t ofs;
        return (*this)(pKmer, pSuperPathId, ofs);
    }

    bool operator()(Gossamer::position_type pKmer, SuperPathId& pSuperPathId, uint64_t& pOffs)
    {
        uint64_t seg = 0;
        uint64_t segOff = 0;
        if (!kmerSegment(pKmer, Forward, seg, segOff))
        {
            return false;
        }

        EdgeIndex::SuperPathIdAndOffset p(std::make_pair(SuperPathId(0), 0));
        if (mIndex.superpath(seg, p))
        {
            pSuperPathId = p.first;
            pOffs = segOff + p.second;
            return true;
        }

        return false;
    }

    bool operator()(Gossamer::position_type pKmer, uint64_t pKmerOffs,
                    const Dir& pDir, SuperPathId& pSuperPathId, uint64_t& pOffs)
    {
        uint64_t seg = 0;
        uint64_t segOff = 0;

        if (!kmerSegment(pKmer, pDir, seg, segOff))
        {
            return false;
        }

        EdgeIndex::SuperPathIdAndOffset p(std::make_pair(SuperPathId(0), 0));
        if (mIndex.superpath(seg, p))
        {
            SuperPathId id(p.first);
            uint64_t pathOff(p.second);
            uint64_t off = pathOff + segOff;

            // Calculate the position of the start of the read.
            if (pDir == Forward)
            {
                // In the forward direction, the read start is towards the
                // segment start.
                if (pKmerOffs > off)
                {
                    return false;
                }
                off -= pKmerOffs;
            }
            else
            {
                // In the reverse direction, the read start is towards the
                // segment end.
                off += pKmerOffs;
            }

            pSuperPathId = id;
            pOffs = off;
            return true;
        }

        return false;
    }

    void reset()
    {
        mPrevState.valid = false;
    }

    KmerAligner(const Graph& pGraph, const EntryEdgeSet& pEntryEdges, 
                const EdgeIndex& pIndex)
        : mGraph(pGraph), mEntryEdges(pEntryEdges), mIndex(pIndex), mRho(mGraph.K() + 1)
    {
        reset();
    }

private:

    struct SegVis 
    {
        bool operator()(const Graph::Edge& pEdge, const Gossamer::rank_type& pRank)
        {
            EdgeIndex::SegmentRank seg;
            EdgeIndex::EdgeOffset offs;
            if (mIndex.segment(pRank, seg, offs))
            {
                mHit = true;
                mSegRank = seg;
                mOffs = offs - mOffs;
                return false;
            }
            mSegRank = pRank;
            ++mOffs;
            return true;
        }

        Graph::Edge segment() const
        {
            if (mHit)
            {
                uint64_t endRank = mEntryEdges.endRank(mSegRank);
                Graph::Edge e(mEntryEdges.select(endRank).value());
                return mGraph.reverseComplement(e);
            }
            return mGraph.select(mSegRank);
        }

        uint64_t offset() const
        {
            if (mHit)
            {
                return mEntryEdges.length(mSegRank) - mOffs;
            }
            return mOffs;
        }

        SegVis(const Graph& pGraph, const EdgeIndex& pEdgeIndex, 
               const EntryEdgeSet& pEntryEdges, const Dir& pDir)
            : mHit(false), mSegRank(0), mOffs(0), mDir(pDir),
              mGraph(pGraph), mIndex(pEdgeIndex), mEntryEdges(pEntryEdges)
        {
        }

    private:

        bool mHit;
        uint64_t mSegRank;
        uint64_t mOffs;
        const Dir mDir;
        const Graph& mGraph;
        const EdgeIndex& mIndex;
        const EntryEdgeSet& mEntryEdges;
    };

    // Finds the segment (as an EntryEdgeSet rank) and the offset of the given kmer,
    // if a unique one exists.
    bool kmerSegment(Gossamer::position_type pKmer, const Dir& pDir, 
                     uint64_t& pSeg, uint64_t& pOffs)
    {
        // If this kmer immediately follows the last kmer along the same linear path,
        // just update and return the previous result.
        if (mPrevState.valid)
        {
            Graph::Node n(mGraph.to(Graph::Edge(mPrevState.kmer)));
            std::pair<Gossamer::rank_type, Gossamer::rank_type> r = mGraph.beginEndRank(n);
            if (   r.second - r.first == 1
                && mGraph.select(r.first) == Graph::Edge(pKmer))
            {
                mPrevState.hits++;
                mPrevState.kmer = pKmer;
                if (pDir == Forward)
                {
                    mPrevState.offs += 1;
                }
                else
                {
                    mPrevState.offs -= 1;
                }
                pSeg = mPrevState.seg;
                pOffs = mPrevState.offs;
                return true;
            }
        }

        mPrevState.misses++;

        Graph::Edge e(pKmer);
        if (!mGraph.access(e))
        {
            mPrevState.valid = false;
            return false;
        }

        if (pDir == Forward)
        {
            // Trace backwards in the graph, i.e. fowards along the rc edge.
            e = mGraph.reverseComplement(e);
        }
        SegVis vis(mGraph, mIndex, mEntryEdges, pDir);
        mGraph.linearPath(e, vis);
        e = vis.segment();
        e = mGraph.reverseComplement(e);
        if (!mEntryEdges.accessAndRank(EntryEdgeSet::Edge(e.value()), pSeg))
        {
            mPrevState.valid = false;
            return false;
        }

        // SegVis counts the number of edges visited - we want the offset.
        pOffs = vis.offset() - 1;
        mPrevState.valid = true;
        mPrevState.kmer = pKmer;
        mPrevState.seg = pSeg;
        mPrevState.offs = pOffs;

        return true;
    }

    const Graph& mGraph;
    const EntryEdgeSet& mEntryEdges;
    const EdgeIndex& mIndex;
    const uint64_t mRho;

    struct
    {
        bool valid;
        Gossamer::position_type kmer;
        uint64_t seg;
        uint64_t offs;
        uint64_t hits;
        uint64_t misses;
    } mPrevState;
};

#endif // KMERALIGNER_HH
