// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossReadBaseString.hh"

namespace // anonymous
{
    class GossReadBaseStringOwning : public GossReadBaseString
    {
    public:
        GossReadBaseStringOwning(const std::string& pLabel, const std::string& pRead, const std::string& pQual)
            : GossReadBaseString(mLabelStorage, mReadStorage, mQualStorage),
              mLabelStorage(pLabel), mReadStorage(pRead), mQualStorage(pQual)
        {
        }
    
    private:
        std::string mLabelStorage;
        std::string mReadStorage;
        std::string mQualStorage;
    };

} // namespace anonymous

GossReadPtr
GossReadBaseString::clone() const
{
    return GossReadPtr(new GossReadBaseStringOwning(mLabel, mRead, mQual));
}
