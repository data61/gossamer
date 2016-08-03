#ifndef GOSSREADSEQUENCE_HH
#define GOSSREADSEQUENCE_HH

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef GOSSREADPARSER_HH
#include "GossReadParser.hh"
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef UNBOUNDEDPROGRESSMONITOR_HH
#include "UnboundedProgressMonitor.hh"
#endif

class GossReadSequence;

typedef std::shared_ptr<GossReadSequence> GossReadSequencePtr;

class GossReadSequenceFactory
{
public:
    /**
     * Create a new read sequence from a file.
     */
    virtual GossReadSequencePtr create(const GossReadParserPtr& pParserPtr,
        Logger* pLogger=0) const = 0;

    virtual ~GossReadSequenceFactory() {}
};

typedef std::shared_ptr<GossReadSequenceFactory> GossReadSequenceFactoryPtr;

class GossReadSequence
{
public:
    struct Item
    {
        std::string mName;
        GossReadParserFactory mParserFac;
        GossReadSequenceFactoryPtr mSequenceFac;

        Item(const std::string& pName, const GossReadParserFactory& pParserFac,
             const GossReadSequenceFactoryPtr& pSequenceFac)
            : mName(pName), mParserFac(pParserFac), mSequenceFac(pSequenceFac)
        {
        }
    };

    /*
     * Returns true if there is at least one read left in the sequence.
     */
    virtual bool valid() const = 0;

    /*
     * Returns the current read.
     */
    virtual const GossRead& operator*() const = 0;

    /*
     * Advance to the next read.
     */
    virtual void operator++() = 0;

    /**
     * Report progress.
     */
    void tick(uint64_t pNumReads)
    {
        if (mMonPtr)
        {
            mMonPtr->tick(pNumReads);
        }
    }

    GossReadSequence(UnboundedProgressMonitor* pMonPtr=0) 
        : mMonPtr(pMonPtr)
    {
    }

    virtual ~GossReadSequence() {}

protected:
    UnboundedProgressMonitor* mMonPtr;
};

#endif // GOSSREADSEQUENCE_HH
