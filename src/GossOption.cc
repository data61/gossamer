#include "GossOption.hh"

class GossOptionBool : public GossOption
{
public:
    void add(boost::program_options::options_description& pOpts,
             boost::program_options::positional_options_description& pPos) const
    {
        pOpts.add_options()(spec(), mDesc.c_str());
    }

    GossOptionBool(const std::string& pLong, const std::string& pShort, const std::string& pDesc)
        : GossOption(pLong, pShort), mDesc(pDesc)
    {
    }

private:
    mutable std::string mDesc;
};

template<>
void
GossOptions::addOpt<bool>(const std::string& pLong, const std::string& pShort, const std::string& pDesc)
{
    BOOST_ASSERT(opts.find(pLong) == opts.end());
    opts[pLong] = GossOptionPtr(new GossOptionBool(pLong, pShort, pDesc));
}
