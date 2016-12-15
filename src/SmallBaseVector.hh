// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef SMALLBASEVECTOR_HH
#define SMALLBASEVECTOR_HH

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef STD_ISTREAM
#include <istream>
#define STD_ISTREAM
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef RANK_SELECT_HH
#include "RankSelect.hh"
#endif

#ifndef VBYTECODEC_HH
#include "VByteCodec.hh"
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

#include <boost/assert.hpp>

class SmallBaseVector;
std::ostream& operator<<(std::ostream& pOut, const SmallBaseVector& pVec);

class SmallBaseVector
{
    static const uint64_t sBasesPerWord = 8;
public:
    uint64_t size() const
    {
        return mSize;
    }

    Gossamer::position_type kmer(const uint64_t pK, uint64_t pOff) const
    {
        Gossamer::position_type r(0);
        for (uint64_t i = 0; i < pK; ++i)
        {
            r <<= 2;
            r |= (*this)[i + pOff];
        }
        return r;
    }

    uint16_t operator[](uint64_t pIdx) const
    {
        uint64_t w = pIdx / sBasesPerWord;
        uint64_t v = pIdx % sBasesPerWord;
        return (mBases[w] >> (2 * v)) & 3;
    }

    void set(uint64_t pIdx, uint16_t pBase)
    {
        BOOST_ASSERT((pBase & 3) == pBase);
        uint64_t w = pIdx / sBasesPerWord;
        uint64_t v = pIdx % sBasesPerWord;
        mBases[w] = (mBases[w] & ~(3 << (2 * v))) | (pBase << (2 * v));
    }

    void push_back(uint16_t pBase)
    {
        BOOST_ASSERT((pBase & 3) == pBase);
        uint64_t v = mSize % sBasesPerWord;
        if (v == 0)
        {
            mBases.push_back(0);
        }
        mBases.back() |= pBase << (2 * v);
        ++mSize;
    }

    void reserve(uint64_t pBases)
    {
        mBases.reserve((pBases + sBasesPerWord - 1) / sBasesPerWord);
    }

    uint64_t ham(const SmallBaseVector& pRhs) const
    {
        // Horribly inefficient!
        uint64_t z = std::min(size(), pRhs.size());
        uint64_t d = std::max(size(), pRhs.size()) - z;
        for (uint64_t i = 0; i < z; ++i)
        {
            if ((*this)[i] != pRhs[i])
            {
                ++d;
            }
        }
        return d;
    }

    size_t editDistance(const SmallBaseVector& pRhs) const;

    void reverseComplement(SmallBaseVector& pRhs) const
    {
        pRhs.clear();
        for (uint64_t i = 0; i < size(); ++i)
        {
            pRhs.push_back(3 - (*this)[size() - 1 - i]);
        }
    }

    void clear()
    {
        mSize = 0;
        mBases.clear();
    }

    void write(std::ostream& pOut) const;

    bool read(std::istream& pIn);

    bool operator<(const SmallBaseVector& pRhs) const
    {
        const SmallBaseVector& lhs(*this);
        uint64_t z = std::min(size(), pRhs.size());
        for (uint64_t i = 0; i < z; ++i)
        {
            if (lhs[i] < pRhs[i])
            {
                return true;
            }
            if (lhs[i] > pRhs[i])
            {
                return false;
            }
        }
        return z < pRhs.size();
    }

    bool operator==(const SmallBaseVector& pRhs) const
    {
        const SmallBaseVector& lhs(*this);
        if (lhs.size() != pRhs.size())
        {
            return false;
        }
        for (uint64_t i = 0; i < size(); ++i)
        {
            if (lhs[i] < pRhs[i])
            {
                return false;
            }
            if (lhs[i] > pRhs[i])
            {
                return false;
            }
        }
        return true;
    }

    bool operator<=(const SmallBaseVector& pRhs) const
    {
        return (*this) < pRhs || (*this) == pRhs;
    }

    template<typename Dest>
    void encode(Dest& pDest) const
    {
        VByteCodec::encode(mSize, pDest);
        for (uint64_t i = 0; i < mBases.size(); ++i)
        {
            pDest.push_back(static_cast<uint8_t>((mBases[i] >> 8) & 0xff));
            pDest.push_back(static_cast<uint8_t>((mBases[i] >> 0) & 0xff));
        }
    }

    template<typename Itr>
    void decode(Itr& pItr)
    {
        mSize = VByteCodec::decode(pItr);
        mBases.resize((mSize + sBasesPerWord - 1) / sBasesPerWord);
        for (uint64_t i = 0; i < mBases.size(); ++i)
        {
            uint16_t x = 0;
            x |= static_cast<uint16_t>(*pItr++) << 8;
            x |= static_cast<uint16_t>(*pItr++) << 0;
            mBases[i] = x;
        }
    }

    static bool make(const std::string& pStr, SmallBaseVector& pRead);

    std::ostream& print(std::ostream& pOut, uint64_t pLineLength = 60) const;

    SmallBaseVector()
        : mSize(0)
    {
    }

    SmallBaseVector(const SmallBaseVector& pParent,
                    uint64_t pOffset, uint64_t pLength)
        : mSize(0)
    {
	reserve(pLength);
        for (uint64_t i = pOffset; i < pOffset + pLength; ++i)
        {
            push_back(pParent[i]);
        }
    }

private:
    uint64_t mSize;
    std::vector<uint16_t> mBases;
};


inline
std::ostream& operator<<(std::ostream& pOut, const SmallBaseVector& pVec)
{
    for (uint64_t i = 0; i < pVec.size(); ++i)
    {
        static const char* map = "ACGT";
        pOut << map[pVec[i]];
    }
    return pOut;
}

namespace boost
{
template<>
inline std::string lexical_cast<std::string,SmallBaseVector>(const SmallBaseVector& pVec)
{
    std::string r;
    for (uint64_t i = 0; i < pVec.size(); ++i)
    {
        static const char* map = "ACGT";
        r.push_back(map[pVec[i]]);
    }
    return r;
}
} // namespace boost

#endif // SMALLBASEVECTOR_HH
