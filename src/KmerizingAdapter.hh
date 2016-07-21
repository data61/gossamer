#ifndef KMERIZINGADAPTER_HH
#define KMERIZINGADAPTER_HH

#ifndef GOSSREADSEQUENCE_HH
#include "GossReadSequence.hh"
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

class KmerizingAdapter
{
public:
    bool valid()
    {
        return mKmers.valid();
    }

    Gossamer::edge_type operator*() const
    {
        BOOST_ASSERT(mKmers.valid());
        return mKmer;
    }

    void operator++()
    {
        ++mKmers;
        while (!mKmers.valid() && mReads.valid())
        {
            ++mReads;
            if (mReads.valid())
            {
                mKmers = GossRead::Iterator(*mReads, mK);
            }
        }
        if (mKmers.valid())
        {
            mKmer = mKmers.kmer();
        }
    }

    KmerizingAdapter(GossReadSequence& pReads, uint64_t pK)
        : mReads(checkValid(pReads)), mK(pK), mKmers(GossRead::Iterator(*mReads, mK)),
          mKmer(0)
    {
        while (!mKmers.valid() && mReads.valid())
        {
            ++mReads;
            if (mReads.valid())
            {
                mKmers = GossRead::Iterator(*mReads, mK);
            }
        }
        if (mKmers.valid())
        {
            mKmer = mKmers.kmer();
        }
    }

private:

    GossReadSequence& checkValid(GossReadSequence& pReads)
    {
        if (!pReads.valid())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("No valid reads."));
        }
        return pReads;
    }

    GossReadSequence& mReads;
    const uint64_t mK;
    GossRead::Iterator mKmers;
    Gossamer::edge_type mKmer;
};

#endif // KMERIZINGADAPTER_HH
