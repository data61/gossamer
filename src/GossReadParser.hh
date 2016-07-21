#ifndef GOSSREADPARSER_HH
#define GOSSREADPARSER_HH

#ifndef BOOST_SHARED_PTR_HPP
#include <boost/shared_ptr.hpp>
#define BOOST_SHARED_PTR_HPP
#endif

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef LINESOURCE_HH
#include "LineSource.hh"
#endif

class GossReadParser
{
private:
    FileThunkIn mIn;

protected:
    GossReadParser(const FileThunkIn& pIn)
        : mIn(pIn)
    {
    }

public:
    virtual bool valid() const = 0;
    virtual const GossRead& read() const = 0;
    virtual void next() = 0;

    const FileThunkIn& in() const
    {
        return mIn;
    }

    virtual ~GossReadParser() { }
};


typedef boost::shared_ptr<GossReadParser> GossReadParserPtr;

typedef boost::function<GossReadParserPtr (const LineSourcePtr&)>
    GossReadParserFactory;


#endif
