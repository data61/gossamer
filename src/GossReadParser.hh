// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSREADPARSER_HH
#define GOSSREADPARSER_HH

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
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


typedef std::shared_ptr<GossReadParser> GossReadParserPtr;

typedef std::function<GossReadParserPtr (const LineSourcePtr&)>
    GossReadParserFactory;


#endif
