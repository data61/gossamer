// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossFastaReadBaseString.hh"

namespace // anonymous
{
    class GossFastaReadBaseStringOwning : public GossFastaReadBaseString
    {
    public:
        GossFastaReadBaseStringOwning(const std::string& pLabel, const std::string& pRead)
            : GossFastaReadBaseString(mLabelStorage, mReadStorage),
              mLabelStorage(pLabel), mReadStorage(pRead)
        {
        }
    
    private:
        std::string mLabelStorage;
        std::string mReadStorage;
    };

} // namespace anonymous

GossReadPtr
GossFastaReadBaseString::clone() const
{
    return GossReadPtr(new GossFastaReadBaseStringOwning(mLabel, mRead));
}
