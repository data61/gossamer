#ifndef GOSSOPTION_HH
#define GOSSOPTION_HH

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef BOOST_PROGRAM_OPTIONS_HPP
#include <boost/program_options.hpp>
#define BOOST_PROGRAM_OPTIONS_HPP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

class GossOption
{
public:
    const std::string& longName() const
    {
        return mLong;
    }

    const std::string& shortName() const
    {
        return mShort;
    }

    const char* spec() const
    {
        return mSpec.c_str();
    }
    virtual void add(boost::program_options::options_description& pOpts,
                     boost::program_options::positional_options_description& pPos) const = 0;

    GossOption(const std::string& pLong, const std::string& pShort)
        : mLong(pLong), mShort(pShort)
    {
        BOOST_ASSERT(mShort.size() <= 1);

        mSpec = longName();
        if (shortName().size() > 0)
        {
            mSpec += ',';
            mSpec += shortName();
        }
    }

    virtual ~GossOption() {}

private:
    const std::string mLong;
    const std::string mShort;
    mutable std::string mSpec;
};
typedef std::shared_ptr<GossOption> GossOptionPtr;

class GossOptions
{
public:
    typedef std::map<std::string,GossOptionPtr> OptsMap;
    OptsMap opts;

    template <typename T>
    void addOpt(const std::string& pLong, const std::string& pShort, const std::string& pDesc);

    template <typename T>
    void addOpt(const std::string& pLong, const std::string& pShort, const T& pDefault, const std::string& pDesc);
};

#include "GossOption.tcc"

#endif // GOSSOPTION_HH
