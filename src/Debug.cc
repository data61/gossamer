// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Debug.hh"

#include "GossamerException.hh"

#include <iostream>
#include <map>
#include <stdexcept>

//using namespace boost;
using namespace std;

namespace // anonymous
{

map<string,Debug*>* debugs = NULL;

} // namespace anonymous

bool
Debug::enable(const string& pName)
{
    if (!debugs)
    {
        debugs = new map<string,Debug*>;
    }

    map<string,Debug*>::iterator i = debugs->find(pName);
    if (i == debugs->end())
    {
        return false;
    }
    i->second->enable();
    return true;
}

bool
Debug::disable(const string& pName)
{
    if (!debugs)
    {
        debugs = new map<string,Debug*>;
    }

    map<string,Debug*>::iterator i = debugs->find(pName);
    if (i == debugs->end())
    {
        return false;
    }
    i->second->disable();
    return true;
}

void
Debug::print(ostream& pOut)
{
    if (!debugs)
    {
        return;
    }

    for (map<string,Debug*>::const_iterator i = debugs->begin(); i != debugs->end(); ++i)
    {
        pOut << i->first << '\t' << i->second->mMsg << endl;
    }
}

Debug::Debug(const string& pName, const string& pMsg)
    : mOn(false), mMsg(pMsg)
{
    if (!debugs)
    {
        debugs = new map<string,Debug*>;
    }

    map<string,Debug*>::iterator i = debugs->find(pName);
    if (i != debugs->end())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("duplicate debug: " + pName));
    }
    (*debugs)[pName] = this;
}
