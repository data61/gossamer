// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PAIRLINKER_HH
#define PAIRLINKER_HH

#ifndef EXTERNALBUFFERSORTER_HH
#include "ExternalBufferSort.hh"
#endif

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef KMERALIGNER_HH
#include "KmerAligner.hh"
#endif

#ifndef PAIRALIGNER_HH
#include "PairAligner.hh"
#endif

#ifndef PAIRLINK_HH
#include "PairLink.hh"
#endif

#ifndef SUPERPATH_HH
#include "SuperPath.hh"
#endif

#ifndef TRIVIALVECTOR_HH
#include "TrivialVector.hh"
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_SSTREAM
#include <sstream>
#define STD_SSTREAM
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif


class UniquenessCache
{
public:
    
    bool unique(SuperPathId pId)
    {
        uint64_t id = pId.value();
        {
            std::unique_lock<std::mutex> l(mMutex);
            if (mKnown[id])
            {
                return mUnique[id];
            }
        }

        bool u = mSG.unique(mSG[pId], mExpectedCoverage);
        std::unique_lock<std::mutex> l(mMutex);
        mUnique[id] = u;
        mKnown[id] = true;
        return u;
    }

    UniquenessCache(const SuperGraph& pSG, double pExpectedCoverage)
        : mSG(pSG), mExpectedCoverage(pExpectedCoverage),
        mKnown(pSG.size()), mUnique(pSG.size())
    {
    }

private:
    
    std::mutex mMutex;
    const SuperGraph& mSG;
    const double mExpectedCoverage;
    boost::dynamic_bitset<> mKnown;
    boost::dynamic_bitset<> mUnique;
};

class PairLinker
{
public:
    typedef std::pair<GossReadPtr, GossReadPtr> ReadPair;
    typedef std::map<int64_t, uint64_t> Hist;

    enum Orientation { PairedEnds, MatePairs, Innies, Outies };

    const Hist& getDist() const
    {
        return mDist;
    }

    void printAlign(std::ostream& pOut, const GossRead& pRead, SuperPathId pId, int64_t pOffs, std::string pTag="")
    {
        std::string r = pRead.print();
        size_t n1 = r.find_first_of('\n');
        // size_t n2 = r.find_first_of('\n', n1 + 1);
        std::string h = r.substr(0, n1);
        // std::string s = r.substr(n1 + 1, n2 - 1);

        pOut << h << "\t" << pId.value() << "\t" << pOffs << "\t" << pTag << "\n";
    }

    void push_back(ReadPair pPair)
    {
        const GossRead& lhsRead(*pPair.first);
        const GossRead& rhsRead(*pPair.second);

        SuperPathId lhsId(0);
        SuperPathId lhsRcId(0);
        SuperPathId rhsId(0);
        SuperPathId rhsRcId(0);
        uint64_t lhsOff(0);
        uint64_t rhsOff(0);

        bool aligned = false;
        int64_t lhsStartOff(0);
        int64_t lhsEndOff(0);
        int64_t rhsStartOff(0);
        int64_t rhsEndOff(0);

        int64_t rhsRcStartOff(0);
        int64_t rhsRcEndOff(0);
        int64_t lhsRcStartOff(0);
        int64_t lhsRcEndOff(0);

        const uint64_t K = mSuperGraph.entries().K();
        const int64_t lhsReadLen = lhsRead.length();
        const int64_t rhsReadLen = rhsRead.length();

        switch (mOrient)
        {
            case PairedEnds:    // Paired end data: L -> <- R
            case Innies:        // Innies = PairedEnds
                aligned =   mAligner.alignRead(lhsRead, lhsId, lhsOff, KmerAligner::Forward)
                         && mUCache.unique(lhsId)
                         && mAligner.alignRead(rhsRead, rhsId, rhsOff, KmerAligner::RevComp)
                         && mUCache.unique(rhsId);
                break;

            case MatePairs:     // Mate pairs data: R <- -> L
                aligned =   mAligner.alignRead(lhsRead, rhsId, rhsOff, KmerAligner::Forward)
                         && mUCache.unique(rhsId)
                         && mAligner.alignRead(rhsRead, lhsId, lhsOff, KmerAligner::RevComp)
                         && mUCache.unique(lhsId);
                break;

            case Outies:        // Outies: L <- -> R     (i.e. in-order mate pairs)
                aligned =   mAligner.alignRead(rhsRead, rhsId, rhsOff, KmerAligner::Forward)
                         && mUCache.unique(rhsId)
                         && mAligner.alignRead(lhsRead, lhsId, lhsOff, KmerAligner::RevComp)
                         && mUCache.unique(lhsId);
                break;

            default:
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << Gossamer::general_error_info("Unrecognised pair orientation: " 
                                                        + boost::lexical_cast<std::string>(mOrient)));
        }

        if (aligned)
        {
            rhsRcId = mSuperGraph.reverseComplement(rhsId);
            lhsRcId = mSuperGraph.reverseComplement(lhsId);

            const int64_t lhsLen = mSuperGraph.size(lhsId) + K;
            const int64_t rhsLen = mSuperGraph.size(rhsRcId) + K;

            switch (mOrient)
            {
                // lhs read mapped forward, rhs read mapped rc
                //  lhsOff is the offset of lhs's first rho-mer on lhsId
                //  rhsOff is the offset of rhs's last rho-mer on rhsId
                // where lhsId and rhsId are on the same strand.
                case Innies:
                case MatePairs:
                case PairedEnds:
                    lhsStartOff = lhsOff;
                    rhsEndOff = rhsOff + K;
                    break;

                // lhs read mapped rc, rhs read mapped forward
                //  lhsOff is the offset of lhs's last rho-mer on lhsId
                //  rhsOff is the offset of rhs's first rho-mer on rhsId
                // where lhsId and rhsId are on the same strand.
                case Outies:
                    lhsStartOff = lhsOff + K + 1 - lhsReadLen;
                    rhsEndOff = rhsOff + rhsReadLen - 1;
                    break;
            }

            lhsEndOff = lhsStartOff + lhsReadLen;
            rhsStartOff = rhsEndOff - rhsReadLen;
            rhsRcEndOff = rhsLen - rhsStartOff;
            lhsRcStartOff = lhsLen - lhsEndOff;

            rhsRcStartOff = rhsRcEndOff - rhsReadLen;
            lhsRcEndOff = lhsRcStartOff + lhsReadLen;

            if (lhsId == rhsId)
            {
                int64_t d = (static_cast<int64_t>(rhsEndOff) - static_cast<int64_t>(lhsStartOff));
                mDist[d]++;
                // printAlign(std::cout, lhsRead, lhsId, lhsStartOff, "same");
                // printAlign(std::cout, rhsRead, rhsId, rhsEndOff, "same");
            }
            if (lhsId != rhsId)
            {
                PairLink lnk(lhsId, rhsId, lhsStartOff, rhsEndOff);
                TrivialVector<uint8_t,PairLink::maxBytes> v;
                PairLink::encode(lnk, v);
                {
                    std::unique_lock<std::mutex> lk(mMutex);
                    //cerr << "p\t" << lhsId.value() << '\t' << rhsId.value() << '\t'
                    //              << lhsStartOff << '\t' << rhsEndOff << '\n';
                    mSorter.push_back(v);
                }
                // printAlign(std::cout, lhsRead, lhsId, lhsStartOff);
                // printAlign(std::cout, rhsRead, rhsId, rhsEndOff);

                PairLink lnk_rc(rhsRcId, lhsRcId, rhsRcStartOff, lhsRcEndOff);
                v.clear();
                PairLink::encode(lnk_rc, v);
                {
                    std::unique_lock<std::mutex> lk(mMutex);
                    //cerr << "r\t" << rhsRcId.value() << '\t' << lhsRcId.value() << '\t'
                    //              << rhsRcStartOff << '\t' << lhsRcEndOff << '\n';
                    mSorter.push_back(v);
                }
            }
        }
        else
        {
            // printAlign(std::cout, lhsRead, SuperPathId(0), 0, "unaln");
            // printAlign(std::cout, rhsRead, SuperPathId(0), 0, "unaln");
        }
    }

    PairLinker(const SuperGraph& pSuperGraph, const PairAligner& pAligner,
               Orientation pOrient, UniquenessCache& pUCache,
               ExternalBufferSort& pSorter, std::mutex& pMutex)
        : mSuperGraph(pSuperGraph), mAligner(pAligner), 
          mOrient(pOrient), mUCache(pUCache),
          mSorter(pSorter), mMutex(pMutex)
    {
    }

private:

    const SuperGraph& mSuperGraph;
    PairAligner mAligner;
    const Orientation mOrient;
    Hist mDist;
    UniquenessCache& mUCache;
    ExternalBufferSort& mSorter;
    std::mutex& mMutex;
};

typedef std::shared_ptr<PairLinker> PairLinkerPtr;

#endif  // PAIRLINKER_HH
