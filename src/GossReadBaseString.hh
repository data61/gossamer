// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSREADBASESTRING_HH
#define GOSSREADBASESTRING_HH

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

class GossReadBaseString : public GossRead
{
public:

    /**
     * Returns the length of the read in bases.
     */
    virtual const uint64_t length() const
    {
        return mRead.size();
    }

    /**
     * Extracts the read as a SmallBaseVector.
     */
    virtual bool extractVector(SmallBaseVector& pVec) const
    {
        uint64_t size = mRead.size();
	pVec.clear();
        pVec.reserve(size);
        for (uint64_t i = 0; i < size; ++i)
        {
            uint16_t base;
            if (!getBase(mRead, i, base))
            {
                return false;
            }
	    pVec.push_back(base);
        }
        return true;
    }

    /**
     * Find the first valid k-mer in the read.
     * Returns true iff such a k-mer exists.
     */
    virtual bool firstKmer(uint64_t pK, Cursor& pCursor) const
    {
        if (mRead.size() < pK)
        {
            return false;
        }
        for (uint64_t i = 0; i + pK <= mRead.size(); ++i)
        {
            uint64_t fail_i;
            if (!getEdge(mRead, i, pK, fail_i, pCursor.kmer))
            {
                i = fail_i;
                continue;
            }
            pCursor.offset = i;
            return true;
        }
        return false;
    }

    /**
     * Find the next valid k-mer in the read.
     * Returns true iff such a k-mer exists.
     */
    virtual bool nextKmer(uint64_t pK, const Gossamer::edge_type& pMask, Cursor& pCursor) const
    {
        
        if (pCursor.offset + pK >= mRead.size())
        {
            return false;
        }
        uint16_t x;
        if (getBase(mRead, pCursor.offset + pK, x))
        {
            pCursor.kmer = ((pCursor.kmer << 2) | x) & pMask;
            pCursor.offset++;
            return true;
        }

        for (uint64_t i = pCursor.offset + pK; i + pK <= mRead.size(); ++i)
        {
            uint64_t fail_i;
            if (!getEdge(mRead, i, pK, fail_i, pCursor.kmer))
            {
                i = fail_i;
                continue;
            }
            pCursor.offset = i;
            return true;
        }
        return false;
    }

    virtual const std::string& read() const
    {
        return mRead;
    }

    virtual const std::string& qual() const
    {
        return mQual;
    }

    virtual void print(std::ostream& pOut) const
    {
        pOut << mRead << std::endl;
    }
    
    virtual const std::string& label() const
    {
        return mLabel;
    }

    virtual GossReadPtr clone() const;

    GossReadBaseString(const std::string& pLabel, const std::string& pRead, const std::string& pQual)
        : mLabel(pLabel), mRead(pRead), mQual(pQual)
    {
    }

private:
    static bool getBase(const std::string& pStr, uint64_t pOff, uint16_t& pRes)
    {
        BOOST_ASSERT(pOff < pStr.size());
        uint16_t x;
        switch (pStr[pOff])
        {
            case 'A':
            case 'a':
            {
                x = 0;
                break;
            }
            case 'C':
            case 'c':
            {
                x = 1;
                break;
            }
            case 'G':
            case 'g':
            {
                x = 2;
                break;
            }
            case 'T':
            case 't':
            {
                x = 3;
                break;
            }
            default:
            {
                return false;
            }
        }
        pRes = x;
        return true;
    }

    static bool getEdge(const std::string& pStr, uint64_t pOff, uint64_t pLen,
                        uint64_t& pFailOff, Gossamer::edge_type& pRes)
    {
        BOOST_ASSERT(pOff + pLen <= pStr.size());
        uint16_t x;
        pRes = Gossamer::edge_type(0ULL);
        for (uint64_t i = pOff; i < pOff + pLen; ++i)
        {
            if (!getBase(pStr, i, x))
            {
                pFailOff = i;
                return false;
            }
            pRes = (pRes << 2) | x;
        }
        return true;
    }

protected:
    const std::string& mLabel;
    const std::string& mRead;
    const std::string& mQual;
};

#endif // GOSSREADBASESTRING_HH
