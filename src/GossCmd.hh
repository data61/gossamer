#ifndef GOSSCMD_HH
#define GOSSCMD_HH

#ifndef APP_HH
#include "App.hh"
#endif

#ifndef GOSSOPTION_HH
#include "GossOption.hh"
#endif

#ifndef GOSSCMDCONTEXT_HH
#include "GossCmdContext.hh"
#endif

#ifndef STD_SET
#include <set>
#define STD_SET
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef BOOST_SHARED_PTR_HPP
#include <boost/shared_ptr.hpp>
#define BOOST_SHARED_PTR_HPP
#endif

#ifndef BOOST_OPTIONAL_HPP
#include <boost/optional.hpp>
#define BOOST_OPTIONAL_HPP
#endif

class GossCmd
{
public:
    virtual void operator()(const GossCmdContext& pCxt) = 0;

    virtual ~GossCmd() {}
};
typedef boost::shared_ptr<GossCmd> GossCmdPtr;

class GossCmdCompound : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt)
    {
        for (uint64_t i = 0; i < mCmds.size(); ++i)
        {
            (*mCmds[i])(pCxt);
        }
    }

    GossCmdCompound(const std::vector<GossCmdPtr>& pCmds)
        : mCmds(pCmds)
    {
    }

private:
    std::vector<GossCmdPtr> mCmds;
};

class GossCmdFactory
{
public:
    const std::string& desc() const
    {
        return mDesc;
    }

    const std::set<std::string>& commonOptions() const
    {
        return mCommonOptions;
    }

    const GossOptions& specificOptions() const
    {
        return mSpecificOptions;
    }

    virtual GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts) = 0;

    GossCmdFactory(const std::string& pDesc)
        : mDesc(pDesc)
    {
    }

protected:
    std::set<std::string> mCommonOptions;
    GossOptions mSpecificOptions;

private:
    const std::string mDesc;
};
typedef boost::shared_ptr<GossCmdFactory> GossCmdFactoryPtr;

#endif
