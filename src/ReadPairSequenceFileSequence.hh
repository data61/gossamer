// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef READPAIRSEQUENCEFILESEQUENCE_HH
#define READPAIRSEQUENCEFILESEQUENCE_HH

#ifndef GOSSREADSEQUENCE_HH
#include "GossReadSequence.hh"
#endif

#ifndef STD_DEQUE
#include <deque>
#define STD_DEQUE
#endif

class ReadPairSequenceFileSequence
{
public:
    /*
     * Returns true if there is at least one read left in the sequence.
     */
    bool valid() const
    {
        if (!mLhsReadSequence || !mRhsReadSequence)
        {
            return false;
        }
        return mLhsReadSequence->valid() && mRhsReadSequence->valid();
    }

    /*
     * Returns the current read from the lhs.
     */
    const GossRead& lhs() const
    {
        return **mLhsReadSequence;
    }

    /*
     * Returns the current read from the rhs.
     */
    const GossRead& rhs() const
    {
        return **mRhsReadSequence;
    }

    /*
     * Advance to the next read.
     */
    void operator++()
    {
        if (mMonPtr)
        {
            mMonPtr->tick(++mNumPairs);   
        }
        if (mLhsReadSequence && mRhsReadSequence)
        {
            ++(*mLhsReadSequence);
            ++(*mRhsReadSequence);
            if (mLhsReadSequence->valid() && mRhsReadSequence->valid())
            {
                return;
            }
        }
        while (mItems.size()
                && (!mLhsReadSequence || !mLhsReadSequence->valid())
                && (!mRhsReadSequence || !mRhsReadSequence->valid()))
        {
            mLhsReadSequence = createReadSequence(mItems.front());
            mItems.pop_front();
            mRhsReadSequence = createReadSequence(mItems.front());
            mItems.pop_front();
        }
    }

    ReadPairSequenceFileSequence(
            const std::deque<GossReadSequence::Item>& pItems,
            const FileFactory& pFactory, const LineSourceFactory& pLineSrcFac,
            UnboundedProgressMonitor* pMonPtr=0, Logger* pLoggerPtr=0)
        : mFactory(pFactory), mLineSrcFac(pLineSrcFac),
          mItems(pItems), mNumPairs(0), mMonPtr(pMonPtr), mLoggerPtr(pLoggerPtr)
    {
        if (mItems.size() % 2)
        {
            throw 42;
        }

        while (mItems.size()
                && (!mLhsReadSequence || !mLhsReadSequence->valid())
                && (!mRhsReadSequence || !mRhsReadSequence->valid()))
        {
            mLhsReadSequence = createReadSequence(mItems.front());
            mItems.pop_front();
            mRhsReadSequence = createReadSequence(mItems.front());
            mItems.pop_front();
        }
    }

private:
    GossReadSequencePtr createReadSequence(const GossReadSequence::Item& pItem)
    {
        BOOST_ASSERT(pItem.mSequenceFac);
        FileThunkIn in(mFactory, pItem.mName);
        LineSourcePtr lineSrc = mLineSrcFac(in);
        GossReadParserPtr parser = pItem.mParserFac(lineSrc);
        return pItem.mSequenceFac->create(parser, mLoggerPtr);
    }

    const FileFactory& mFactory;
    LineSourceFactory mLineSrcFac;
    std::deque<GossReadSequence::Item> mItems;
    GossReadSequencePtr mLhsReadSequence;
    GossReadSequencePtr mRhsReadSequence;
    uint64_t mNumPairs;
    UnboundedProgressMonitor* mMonPtr;
    Logger* mLoggerPtr;
};

#endif // READPAIRSEQUENCEFILESEQUENCE_HH
