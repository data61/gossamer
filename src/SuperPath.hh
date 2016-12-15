// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SUPERPATH_HH
#define SUPERPATH_HH

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef ENTRYEDGESET_HH
#include "EntryEdgeSet.hh"
#endif

#ifndef SUPERGRAPH_HH
#include "SGraph.hh"
#endif

#ifndef SUPERPATHID_HH
#include "SuperPathId.hh"
#endif

#ifndef STD_LIMITS
#include <limits>
#define STD_LIMITS
#endif

class SuperGraph;

class SuperPath
{
public:
    /**
     * A Segment is a basic path component. It is either:
     *  - a linear path identified by its rank in the EntryEdgeSet.
     *  - a gap (+ve or -ve) of n bases
     *  - an explicit sequence of bases
     */
    struct Segment 
    {
        static const uint64_t M = 0x3fffffffffffffffULL;

        bool isLinearPath() const
        {
            return mValue >> 62 == 0;
        }

        bool isGap() const
        {
            return mValue >> 62 == 1;
        }

        bool isSequence() const
        {
            return mValue >> 62 == 2;
        }

        uint64_t linearPath() const
        {
            return mValue & M;
        }

        int64_t gap() const
        {
            return int64_t(mValue & M) - int64_t(M >> 1);
        }

        int64_t length(const EntryEdgeSet& pEntries) const
        {
            if (isLinearPath())
            {
                return pEntries.length(mValue);
            }
            else if (isGap())
            {
                return gap();
            }
            return 0;
        }

        static Segment makeLinearPath(uint64_t pRank)
        {
            return Segment(pRank);
        }

        static Segment makeGap(int64_t pGap)
        {
            uint64_t x = 0x4000000000000000ULL | uint64_t(pGap + int64_t(M >> 1));
            Segment s(x);
            BOOST_ASSERT(s.gap() == pGap);
            return s;
        }

        operator uint64_t() const
        {
            return mValue;
        }

        Segment(uint64_t pValue)
            : mValue(pValue)
        {
        }

        Segment(const Segment& pSeg)
            : mValue(pSeg.mValue)
        {
        }

        uint64_t mValue;
    };

    typedef std::vector<Segment> Segments;

    /**
     * Return the starting node of the SuperPath.
     */
    EntryEdgeSet::Node start(const EntryEdgeSet& pEntries) const
    {
        return pEntries.from(firstEdge(pEntries));
    }

    /**
     * Return the ending node of the SuperPath.
     */
    EntryEdgeSet::Node end(const EntryEdgeSet& pEntries) const
    {
        return pEntries.to(lastEdge(pEntries));
    }

    /**
     * Return the first edge of the SuperPath.
     */ 
     EntryEdgeSet::Edge firstEdge(const EntryEdgeSet& pEntries) const
     {
        return pEntries.select(mSegs.front());
     }

    /**
     * Return the final edge of the SuperPath.
     */
     EntryEdgeSet::Edge lastEdge(const EntryEdgeSet& pEntries) const
     {
        uint64_t r = pEntries.endRank(mSegs.back());
        return pEntries.reverseComplement(pEntries.select(r));
     }

    /**
     * Return the length of the superpath (in edges).
     */
    uint64_t size(const EntryEdgeSet& pEntries) const
    {
        uint64_t s = 0;
        for (uint64_t i = 0; i < mSegs.size(); ++i)
        {
            s += segLength(pEntries, mSegs[i]);
        }
        return s;
    }

    /**
     * Return the length of the superpath in bases.
     */ 
     int64_t baseSize(const EntryEdgeSet& pEntries) const
     {
        const uint64_t K = pEntries.K();
        int64_t s = K;
        for (uint64_t i = 0; i < mSegs.size(); ++i)
        {
            Segment seg = mSegs[i];
            s += segLength(pEntries, seg);
            if (isGap(pEntries, seg))
            {
                s += K;
            }
        }
        return s;
     }

    /**
     * Returns the length of the given segment.
     */
    static int64_t segLength(const EntryEdgeSet& pEntries, Segment pSeg)
    {
        return pSeg.length(pEntries);
    }

    /**
     * Returns the id of the gap segment of the given length.
     */
    static Segment gapSeg(int64_t pLen)
    {
        return Segment::makeGap(pLen);    
    }

    static bool isGap(const EntryEdgeSet& pEntries, Segment pSeg)
    {
        return pSeg.isGap();
    }

    /**
     * Retrieve the set of segments that constitute this path.
     */
    const std::vector<Segment>& segments() const
    {
        return mSegs;
    }

    const SuperPathId id() const
    {
        return mId;
    }

    const SuperPathId& reverseComplement() const
    {
        return mRC;
    }

    SuperPath(const SuperGraph& pSG, const SuperPathId& pId, 
              const Segments& pSegs, SuperPathId pRC)
        : mSG(pSG), mId(pId), mSegs(pSegs), mRC(pRC)
    {
    }

private:

    const SuperGraph& mSG;
    const SuperPathId mId;
    const Segments& mSegs;
    const SuperPathId mRC;
};

namespace boost
{
template<>
inline std::string lexical_cast<std::string,SuperPath>(const SuperPath& pPath)
{
    std::ostringstream out;
    out << pPath.id().value() << " [";
    if (pPath.segments().size())
    {
        out << pPath.segments()[0];
    }
    for (uint64_t i = 1; i < pPath.segments().size(); ++i)
    {
        out << "," << pPath.segments()[i];
    }
    out << "] ";
    out << " " << pPath.reverseComplement().value();
    return out.str();
}
}


#endif  // SUPERPATH_HH
