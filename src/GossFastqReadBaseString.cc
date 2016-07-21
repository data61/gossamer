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
