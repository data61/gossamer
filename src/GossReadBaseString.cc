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
