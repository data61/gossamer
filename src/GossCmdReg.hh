// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDREG_HH
#define GOSSCMDREG_HH

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdReg
{
public:
    static std::map<std::string,GossCmdFactoryPtr>* cmds;

    GossCmdReg(const std::string& pName, GossCmdFactoryPtr pFactory)
    {
        if (!cmds)
        {
            cmds = new std::map<std::string,GossCmdFactoryPtr>();
        }
        std::map<std::string,GossCmdFactoryPtr>::const_iterator i = cmds->find(pName);
        BOOST_ASSERT(i == cmds->end());
        (*cmds)[pName] = pFactory;
    }
};

#endif // GOSSCMDREG_HH
