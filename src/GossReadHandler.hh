// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSREADHANDLER_HH
#define GOSSREADHANDLER_HH

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

class GossReadHandler
{
public:

    virtual void operator()(const GossRead& pRead) = 0;

    virtual void operator()(const GossRead& pLhs, const GossRead& pRhs) = 0;

    virtual void operator()(const GossReadPtr pReadPtr)
    {
        (*this)(*pReadPtr);
    }

    virtual void operator()(const std::pair<GossReadPtr, GossReadPtr>& pPair)
    {
        (*this)(*pPair.first, *pPair.second);
    }

    virtual void startFile(const std::string& pFileName) {}

    virtual void endFile() {}

    virtual void end() {}

    virtual ~GossReadHandler() {}
};

typedef std::shared_ptr<GossReadHandler> GossReadHandlerPtr;

#endif // GOSSREADHANDLER_HH
