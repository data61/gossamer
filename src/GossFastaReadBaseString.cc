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
