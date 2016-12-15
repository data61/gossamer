// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSFASTQREADBASESTRING_HH
#define GOSSFASTQREADBASESTRING_HH

#ifndef GOSSREAD_HH
#include "GossRead.hh"
#endif

#ifndef GOSSREADBASESTRING_HH
#include "GossReadBaseString.hh"
#endif

class GossFastqReadBaseString : public GossReadBaseString
{
public:

    void print(std::ostream& pOut) const
    {
        pOut << '@' << mLabel << std::endl;
        pOut << mRead << std::endl;
        pOut << '+' << mQLabel << std::endl;
        pOut << mQual << std::endl;
    }
    
    GossReadPtr clone() const;

    GossFastqReadBaseString(const std::string& pLabel, const std::string& pRead, 
                            const std::string& pQLabel, const std::string& pQual)
        : GossReadBaseString(pLabel, pRead, pQual), mQLabel(pQLabel)
    {
    }

protected:

    const std::string& mQLabel;
};

#endif // GOSSFASTQREADBASESTRING_HH
