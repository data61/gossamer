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
