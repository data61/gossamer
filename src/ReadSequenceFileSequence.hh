// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef READSEQUENCEFILESEQUENCE_HH
#define READSEQUENCEFILESEQUENCE_HH

#ifndef GOSSREADSEQUENCE_HH
#include "GossReadSequence.hh"
#endif

#ifndef STD_DEQUE
#include <deque>
#define STD_DEQUE
#endif

class ReadSequenceFileSequence : public GossReadSequence
{
public:
    /*
     * Returns true if there is at least one read left in the sequence.
     */
    bool valid() const
    {
        if (!mReadSequence)
        {
            return false;
        }
        return mReadSequence->valid();
    }

    /*
     * Returns the current read.
     */
    const GossRead& operator*() const
    {
        return **mReadSequence;
    }

    /*
     * Advance to the next read.
     */
    void operator++()
    {
        tick(++mNumReads);
        if (mReadSequence)
        {
            ++(*mReadSequence);
            if (mReadSequence->valid())
            {
                return;
            }
        }
        next();
    }

    ReadSequenceFileSequence(const std::deque<Item>& pItems,
            const FileFactory& pFactory, const LineSourceFactory& pLineSrcFac,
            UnboundedProgressMonitor* pMonPtr=0, Logger* pLoggerPtr=0)
        : GossReadSequence(pMonPtr), mFactory(pFactory), 
          mLineSrcFac(pLineSrcFac), mItems(pItems), mNumReads(0),
          mLoggerPtr(pLoggerPtr)
    {
        next();
    }
private:

    void next()
    {
        while (mItems.size() && (!mReadSequence || !mReadSequence->valid()))
        {
            const Item& itm = mItems.front();
            BOOST_ASSERT(itm.mSequenceFac);
            // mReadSequence = itm.second->create(itm.first, mFactory, mLoggerPtr);
            FileThunkIn in(mFactory, itm.mName);
            LineSourcePtr lineSrc = mLineSrcFac(in);
            GossReadParserPtr parser = itm.mParserFac(lineSrc);
            mReadSequence = itm.mSequenceFac->create(parser, mLoggerPtr);
            mItems.pop_front();
        }
    }

    const FileFactory& mFactory;
    LineSourceFactory mLineSrcFac;
    std::deque<GossReadSequence::Item> mItems;
    GossReadSequencePtr mReadSequence;
    uint64_t mNumReads;
    Logger* mLoggerPtr;
};

#endif // READSEQUENCEFILESEQUENCE_HH
