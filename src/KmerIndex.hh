// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef KMERINDEX_HH
#define KMERINDEX_HH

#ifndef KMERSET_HH
#include "KmerSet.hh"
#endif

#ifndef VWORD32CODEC_HH
#include "VWord32Codec.hh"
#endif

#ifndef TRIVIALVECTOR_HH
#include "TrivialVector.hh"
#endif

#ifndef RANKSELECT_HH
#include "RankSelect.hh"
#endif

class KmerIndex
{
public:
    static constexpr uint64_t version = 2012030101ULL;

    struct Header
    {
        uint64_t version;
    };

    class Builder
    {
    public:
        void push_back(const Gossamer::position_type& pKmer, uint64_t pSeq, uint64_t pOffset)
        {
            if (pKmer != mCurrKmer)
            {
                mKmersBuilder.push_back(pKmer);
                mCurrKmer = pKmer;
                mPostingsStarts.push_back(Gossamer::position_type(mGlobPos));
                mCurrSeq = 0;
                mCurrPos = 0;
            }

            TrivialVector<uint32_t,6> ws;

            if (pSeq != mCurrSeq)
            {
                BOOST_ASSERT(pSeq > mCurrSeq);
                uint64_t d = pSeq - mCurrSeq;
                VWord32Codec::encode(d << 1, ws);
                mCurrSeq = pSeq;
                mCurrPos = 0;
            }

            BOOST_ASSERT(pOffset >= mCurrPos);
            uint64_t d = pOffset - mCurrPos;
            mCurrPos = pOffset;
            VWord32Codec::encode((d << 1) | 1, ws);

            BOOST_ASSERT(ws.size() > 0);
            mPostings.push_back(ws[0]);
            for (uint64_t i = 1; i < ws.size(); ++i)
            {
                mPostings.push_back(ws[i]);
            }
            mGlobPos += ws.size();
        }

        void end()
        {
            mPostingsStarts.push_back(Gossamer::position_type(mGlobPos));
            mKmersBuilder.end();
            mPostingsStarts.end(Gossamer::position_type(mGlobPos));
            mPostings.end();
        }

        Builder(uint64_t pK, const std::string& pBaseName, FileFactory& pFactory,
                const uint64_t pM, const uint64_t pT)
            : mK(pK), mKmersBuilder(pK, pBaseName + ".kmer-toc", pFactory, pM),
              mPostingsStarts(pBaseName + ".postings-toc", pFactory, Gossamer::position_type(pT), pM),
              mPostings(pBaseName + ".postings", pFactory),
              mCurrKmer(Gossamer::position_type(1) << (2 * pK)),
              mGlobPos(0), mCurrSeq(0), mCurrPos(0)
        {
        }

    private:
        const uint64_t mK;
        KmerSet::Builder mKmersBuilder;
        SparseArray::Builder mPostingsStarts;
        MappedArray<uint32_t>::Builder mPostings;
        Gossamer::position_type mCurrKmer;
        uint64_t mGlobPos;
        uint64_t mCurrSeq;
        uint64_t mCurrPos;
    };

    class PostingsVector
    {
    public:
        bool valid() const
        {
            return mValid;
        }

        const std::pair<uint64_t,uint64_t>& operator*() const
        {
            return mCurr;
        }

        void  operator++()
        {
            if (mCursor == mEnd)
            {
                mValid = false;
                return;
            }
            uint64_t x = VWord32Codec::decode(mCursor);
            if ((x & 1) == 1)
            {
                mCurr.second += x >> 1;
                return;
            }
            else
            {
                mCurr.first += x >> 1;
                mCurr.second = VWord32Codec::decode(mCursor) >> 1;
            }
        }

        PostingsVector(const uint32_t* pBegin, const uint32_t* pEnd)
            : mValid(true), mCurr(0, 0), mCursor(pBegin), mEnd(pEnd)
        {
            ++(*this);
        }

    private:
        bool mValid;
        std::pair<uint64_t,uint64_t> mCurr;
        const uint32_t* mCursor;
        const uint32_t* mEnd;
    };

    PostingsVector operator[](const Gossamer::position_type& pKmer) const
    {
        uint64_t r = 0;
        if (mKmers.accessAndRank(KmerSet::Edge(pKmer), r))
        {
            uint64_t b = mPostingsStarts.select(r).value().asUInt64();
            uint64_t e = mPostingsStarts.select(r + 1).value().asUInt64();
            return PostingsVector(mPostings.begin() + b, mPostings.begin() + e);
        }
        return PostingsVector(NULL, NULL);
    }

    KmerIndex(const std::string& pBaseName, FileFactory& pFactory)
        : mKmers(pBaseName + ".kmer-toc", pFactory),
          mPostingsStarts(pBaseName + ".postings-toc", pFactory),
          mPostings(pBaseName + ".postings", pFactory)
    {
    }

private:
    KmerSet mKmers;
    SparseArray mPostingsStarts;
    MappedArray<uint32_t> mPostings;
};


#endif // KMERINDEX_HH
