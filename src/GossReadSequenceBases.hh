#ifndef GOSSREADSEQUENCEBASES_HH
#define GOSSREADSEQUENCEBASES_HH

#ifndef GOSSREADSEQUENCE_HH
#include "GossReadSequence.hh"
#endif

#ifndef GOSSREADBASESTRING_HH
#include "GossReadBaseString.hh"
#endif

#ifndef UNBOUNDEDPROGRESSMONITOR_HH
#include "UnboundedProgressMonitor.hh"
#endif


class GossReadSequenceBases : public GossReadSequence
{
public:
    /*
     * Returns true if there is at least one read left in the sequence.
     */
    bool valid() const
    {
        return mParser.valid();
    }

    /*
     * Returns the current read.
     */
    const GossRead& operator*() const
    {
        return mParser.read();
    }

    /*
     * Advance to the next read.
     */
    void operator++()
    {
        mParser.next();
    }

    GossReadSequenceBases(const GossReadParserPtr& pParserPtr,
                          UnboundedProgressMonitor* pMonPtr=0)
        : GossReadSequence(pMonPtr), 
          mParserPtr(pParserPtr), mParser(*mParserPtr)
    {
        mParser.next();
    }

private:

    GossReadParserPtr mParserPtr;
    GossReadParser& mParser;
};


class GossReadSequenceBasesFactory : public GossReadSequenceFactory
{
public:

    GossReadSequencePtr create(const GossReadParserPtr& pParserPtr,
        Logger* pLogger=0) const
    {
        using namespace boost;
        if (pLogger)
        {
            LOG(*pLogger, info) << "parsing sequences from "
                << pParserPtr->in().filename();
        }
        return make_shared<GossReadSequenceBases>(pParserPtr);
    }
};


#endif // GOSSREADSEQUENCEBASES_HH
