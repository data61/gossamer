// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PAIRALIGNER_HH
#define PAIRALIGNER_HH

#ifndef EDGEINDEX_HH
#include "EdgeIndex.hh"
#endif

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef KMERALIGNER_HH
#include "KmerAligner.hh"
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class PairAligner
{
public:
    typedef KmerAligner::Dir Dir;
    typedef std::map<uint64_t,uint64_t> Scores;
    typedef std::map<SuperPathId, Scores> SuperPathScores;

    bool alignKmers(const std::vector<std::pair<Gossamer::position_type, uint64_t> >& pKmers, 
                    SuperPathId& pId, uint64_t& pOffset, const Dir& pDir)
    {
        mKmerAligner.reset();
        SuperPathScores hyp;
        SuperPathId id(0);
        uint64_t ofs;
        for (std::vector<std::pair<Gossamer::position_type, uint64_t> >::const_iterator 
             itr = pKmers.begin(); itr != pKmers.end(); ++itr)
        {
            if (mKmerAligner((*itr).first, (*itr).second, pDir, id, ofs))
            {
                hyp[id][ofs]++;
            }
        }
        if (hyp.size() == 0)
        {
            return false;
        }

        selectAnchor(hyp, pId, pOffset);

        return true;
    }

    bool alignRead(const GossRead& pRead, SuperPathId& pId, uint64_t& pOffset, const Dir& pDir)
    {
        mKmerAligner.reset();
        SuperPathScores hyp;
        SuperPathId id(0);
        uint64_t ofs;
        for (GossRead::Iterator itr(pRead, mRho); itr.valid(); ++itr)
        {
            if (mKmerAligner(itr.kmer(), itr.offset(), pDir, id, ofs))
            {
                hyp[id][ofs]++;
            }
        }
        if (hyp.size() == 0)
        {
            return false;
        }

        selectAnchor(hyp, pId, pOffset);
        return true;
    }

    PairAligner(const Graph& pGraph, const EntryEdgeSet& pEntryEdges, 
                const EdgeIndex& pIndex)
        : mKmerAligner(pGraph, pEntryEdges, pIndex), mRho(pGraph.K() + 1)
    {
    }

private:

    void selectAnchor(const SuperPathScores& pHyp, SuperPathId& pId, uint64_t& pOffset) const
    {
        SuperPathId id(pHyp.begin()->first);
        uint64_t off = pHyp.begin()->second.begin()->first;
        uint64_t votes = pHyp.begin()->second.begin()->second;
        for (std::map<SuperPathId,Scores>::const_iterator i = pHyp.begin(); i != pHyp.end(); ++i)
        {
            for (Scores::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
            {
                if (j->second > votes)
                {
                    id = i->first;
                    off = j->first;
                    votes = j->second;
                }
            }
            pId = id;
            pOffset = off;
        }
    }

    KmerAligner mKmerAligner;
    const uint64_t mRho;

};

#endif // PAIRALIGNER_HH
