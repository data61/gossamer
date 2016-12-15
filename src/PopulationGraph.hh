// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef POPULATIONGRAPH_HH
#define POPULATIONGRAPH_HH

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

class PopulationGraph : public GraphEssentials<PopulationGraph>
{
public:
    struct Header
    {
        uint64_t version;
        uint64_t K;
    };

    uint64_t K() const
    {
        return mHeader.K;
    }

    Gossamer::rank_type rank(const Edge& pLhs) const
    {
        return mEdges.rank(pLhs.value());
    }

    std::pair<Gossamer::rank_type,Gossamer::rank_type> rank(const Edge& pLhs, const Edge& pRhs) const
    {
        return mEdges.rank(pLhs.value(), pRhs.value());
    }

    static void init(uint64_t pK, const std::string& pBaseName, FileFactory& pFactory)
    {
        Gossamer::position_type n(1);
        n <<= 2 * pK;
        {
            SparseArray::Builder bld(pBaseName + ".edges", pFactory, n, 0);
            bld.end();
        }
        {
            VariableByteArray::Builder bld(pBaseName + ".pop-freq", pFactory, 0, 1);
            bld.end()
        }
        {
            VariableByteArray::Builder bld(pBaseName + ".freq-sum", pFactory, 0, 1);
            bld.end()
        }
    }

    static void inc(const std::string& pPopGraph, const std::string& pNewGraph, FileFactory& pFactory)
    {
        // estimate the number of edges in the final graph.
        // If the number of existing samples is 0 then it is trivial.
        // If the number of existing samples is > 0 then estimate the
        // average number of unique edges per sample.
        uint64_t n = 0;
        if (popGraph.samples() == 0)
        {
            n = newGraph.count();
        }
        else
        {
            uint64_t x = 0;
            uint64_t z = popGraph.mPopFreq.size();
            for (uint64_t i = 0; i < z; ++i)
            {
                uint64_t f = popGraph.mPopFreq[i];
                if (f == 1)
                {
                    x += 1;
                }
            }
            n = z + (static_cast<double>(x) / static_cast<double>(popGraph.mHeader.numSamples)) * newGraph.count();
        }
    }

private:
    Header mHeader;
    SparseArray mEdges;
    VariableByteArray mPopFreq;
    VariableByteArray mFreqSum;
};


#endif // POPULATIONGRAPH_HH
