// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSFASTAREADBASESTRING_HH
#define GOSSFASTAREADBASESTRING_HH

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef GOSSREADBASESTRING_HH
#include "GossReadBaseString.hh"
#endif

class GossFastaReadBaseString : public GossReadBaseString
{
public:

    void print(std::ostream& pOut) const
    {
        pOut << '>' << mLabel << std::endl;
        pOut << mRead << std::endl;
    }
    
    GossReadPtr clone() const;

    GossFastaReadBaseString(const std::string& pLabel, const std::string& pRead)
        : GossReadBaseString(pLabel, pRead, mQual), mQual()
    {
    }

private:
    std::string mQual;
};

#endif // GOSSFASTAREADBASESTRING_HH
