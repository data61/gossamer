// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossFastqReadBaseString.hh"

namespace // anonymous
{
    class GossFastqReadBaseStringOwning : public GossFastqReadBaseString
    {
    public:
        GossFastqReadBaseStringOwning(const std::string& pLabel, const std::string& pRead,
                                      const std::string& pQLabel, const std::string& pQual)
            : GossFastqReadBaseString(mLabelStorage, mReadStorage, mQLabelStorage, mQualStorage),
              mLabelStorage(pLabel), mReadStorage(pRead),
              mQLabelStorage(pQLabel), mQualStorage(pQual)
        {
        }
    
    private:
        std::string mLabelStorage;
        std::string mReadStorage;
        std::string mQLabelStorage;
        std::string mQualStorage;
    };

} // namespace anonymous

GossReadPtr
GossFastqReadBaseString::clone() const
{
    return GossReadPtr(new GossFastqReadBaseStringOwning(mLabel, mRead, mQLabel, mQual));
}
