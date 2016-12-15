// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ENTRYSETPACKET_HH
#define ENTRYSETPACKET_HH

#ifndef TAGGEDNUM_HH
#include "TaggedNum.hh"
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

class EntrySetPacket
{
public:
    struct MultiplicityTag {};
    typedef TaggedNum<MultiplicityTag> Multiplicity;

    struct EdgeRankTag {};
    typedef TaggedNum<EdgeRankTag> EdgeRank;

    static const uint64_t WordBits = 64;
    static const uint64_t HighBit = 1ULL << (WordBits - 1);
    static const uint64_t RankBits = 40;
    static const uint64_t RankBitsMask = (1ULL << RankBits) - 1;
    static const uint64_t MultBits = WordBits - RankBits;

    uint64_t size() const
    {
        return mKeys.size();
    }

    std::pair<Multiplicity,EdgeRank> operator[](uint64_t pIdx) const
    {
        return std::pair<Multiplicity,EdgeRank>(
            Multiplicity((mKeys[pIdx] & ~HighBit) >> RankBits),
            EdgeRank(mKeys[pIdx] & RankBitsMask));
    }

    void push_back(const Multiplicity& pMultiplicity, const EdgeRank& pEdgeRank)
    {
        uint64_t m = pMultiplicity.value();
        uint64_t r = pEdgeRank.value();
        if (m >= (1ULL << MultBits))
        {
            m = (1ULL << MultBits) - 1;
        }
        if (r >= (1ULL << RankBits))
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::range_error("EntrySetPacket::push_back", (1ULL << RankBits), r));
        }
        mKeys.push_back((m << RankBits) | r);
        BOOST_ASSERT((mKeys.back() & HighBit) == 0);
    }

    bool operator<(const EntrySetPacket& pRhs) const
    {
        uint64_t i = 0;
        while (i < mKeys.size() && i < pRhs.mKeys.size()
                && mKeys[i] == pRhs.mKeys[i])
        {
            ++i;
        }
        if (i < mKeys.size() && i < pRhs.mKeys.size())
        {
            return mKeys[i] < pRhs.mKeys[i];
        }
        return mKeys.size() < pRhs.mKeys.size();
    }

    static bool read(std::istream& pIn, EntrySetPacket& pPacket)
    {
        pPacket.mKeys.clear();
        while (pIn.good())
        {
            uint64_t x = 0;
            pIn.read(reinterpret_cast<char*>(&x), sizeof(x));
            if (!pIn.good())
            {
                return false;
            }
            pPacket.mKeys.push_back(x & ~HighBit);
            if ((x & HighBit) == 0)
            {
                return true;
            }
        }
        return false;
    }

    static void write(std::ostream& pOut, const EntrySetPacket& pPacket)
    {
        for (uint64_t i = 0; i < pPacket.size() - 1; ++i)
        {
            uint64_t x = pPacket.mKeys[i] | HighBit;
            pOut.write(reinterpret_cast<const char*>(&x), sizeof(x));
        }
        uint64_t x = pPacket.mKeys.back();
        pOut.write(reinterpret_cast<const char*>(&x), sizeof(x));
    }

    EntrySetPacket()
    {
    }

    EntrySetPacket(const Multiplicity& pMultiplicity, const EdgeRank& pEdgeRank)
    {
        push_back(pMultiplicity, pEdgeRank);
    }

private:
    std::vector<uint64_t> mKeys;
};

#endif // ENTRYSETPACKET_HH
