// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSREAD_HH
#define GOSSREAD_HH

#ifndef GOSSAMER_HH
#include "Gossamer.hh"
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_SSTREAM
#include <sstream>
#define STD_SSTREAM
#endif

#ifndef STD_UTILITY
#include <utility>
#define STD_UTILITY
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef SMALLBASEVECTOR_HH
#include "SmallBaseVector.hh"
#endif

class GossRead
{
public:
    struct Cursor
    {
        uint64_t offset;
        Gossamer::edge_type kmer;

        Cursor()
            : offset(0), kmer(0)
        {
        }
    };

    /**
     * An iterator yielding the k-mers from the read.
     */
    class Iterator
    {
    public:
        /*
         * Returns true if the iterator may be dereferenced.
         */
        bool valid() const 
        {
            return mValid;
        }

        /*
         * Returns the current k-mer.
         */
        Gossamer::edge_type operator*() const
        {
            return kmer();
        }

        /*
         * Returns the current k-mer.
         */
        Gossamer::edge_type kmer() const
        {
            BOOST_ASSERT(valid());
            return mCurr.kmer;
        }

        /**
         * Returns the offset of the current k-mer.
         */
        uint64_t offset() const
        {
            BOOST_ASSERT(valid());
            return mCurr.offset;
        }

        /*
         * Advance the iterator to the next k-mer if it exists.
         */
        void operator++()
        {
            mValid = mRead->nextKmer(mK, mMask, mCurr);
        }

        Iterator(const GossRead& pRead, uint64_t pK)
            : mRead(&pRead), mK(pK), mMask((Gossamer::edge_type(1) << (2 * pK)) - 1)
        {
            mValid = mRead->firstKmer(mK, mCurr);
        }

    private:
        const GossRead* mRead;
        uint64_t mK;
        Gossamer::edge_type mMask;
        Cursor mCurr;
        bool mValid;
    };

    /**
     * Returns the length of the read in bases.
     */
    virtual const uint64_t length() const = 0;

    /**
     * Returns the read as a SmallBaseVector.
     * Returns true iff the read succeeds.
     */
    virtual bool extractVector(SmallBaseVector& pVec) const = 0;

    /**
     * Find the first valid k-mer in the read.
     * Returns true iff such a k-mer exists.
     */
    virtual bool firstKmer(uint64_t pK, Cursor& pCursor) const = 0;

    /**
     * Find the next valid k-mer in the read.
     * Returns true iff such a k-mer exists.
     */
    virtual bool nextKmer(uint64_t pK, const Gossamer::edge_type& pMask, Cursor& pCursor) const = 0;

    /**
     * Write the read out into a file in the same format in which it was read.
     * Implementations should, if possible, round-trip correctly.
     */
    virtual void print(std::ostream& pOut) const = 0;
    
    /**
     * Return a label for the read.
     */
    virtual const std::string& label() const = 0;

    /**
     * Return the sequence of bases in the read.
     */
    virtual const std::string& read() const = 0;


    /**
     * Return the sequence of quality scores in the read.
     */
    virtual const std::string& qual() const = 0;

    std::string print() const
    {
        std::stringstream ss;
        print(ss);
        return ss.str();
    }

    /**
     * Create a copy of the read.
     */
    virtual std::shared_ptr<GossRead> clone() const = 0;

    virtual ~GossRead() {}
};
typedef std::shared_ptr<GossRead> GossReadPtr;

#endif // GOSSREAD_HH
