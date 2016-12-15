// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PACKETSORTER_HH
#define PACKETSORTER_HH

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef EXTERNALBUFFERSORT_HH
#include "ExternalBufferSort.hh"
#endif

class PacketSorter
{
public:
    typedef std::vector<uint8_t> Packet;
    typedef Packet value_type;

    void push_back(const Packet& pPacket)
    {
        mSorter.push_back(pPacket);
    }

    template <typename Dest>
    void sort(Dest& pDest)
    {
        mSorter.sort(pDest);
    }

    PacketSorter(uint64_t pBufferSize, FileFactory& pFactory)
        : mSorter(pBufferSize, pFactory)
    {
    }

private:
    ExternalBufferSort mSorter;
};

#endif // PACKETSORTER_HH
