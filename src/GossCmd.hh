// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
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

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
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
typedef std::shared_ptr<GossCmd> GossCmdPtr;

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
typedef std::shared_ptr<GossCmdFactory> GossCmdFactoryPtr;

#endif
