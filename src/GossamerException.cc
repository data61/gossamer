// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossamerException.hh"

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

Gossamer::general_error_info
Gossamer::range_error(const std::string& pWhere, uint64_t pMax, uint64_t pVal)
{
    string s("out of range access on " + pWhere
                + ". size  = " + lexical_cast<string>(pMax)
                + " index = " + lexical_cast<string>(pVal));

    return Gossamer::general_error_info(s);
}
