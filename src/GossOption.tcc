// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

template <typename T>
class GossOptionOptional : public GossOption
{
public:
    void add(boost::program_options::options_description& pOpts,
             boost::program_options::positional_options_description& pPos) const
    {
        pOpts.add_options()(spec(), boost::program_options::value<T>(), mDesc.c_str());
    }

    GossOptionOptional(const std::string& pLong, const std::string& pShort, const std::string& pDesc)
        : GossOption(pLong, pShort), mDesc(pDesc)
    {
    }

private:
    mutable std::string mDesc;
};

template <typename T>
class GossOptionDefaulted : public GossOption
{
public:
    void add(boost::program_options::options_description& pOpts,
             boost::program_options::positional_options_description& pPos) const
    {
        pOpts.add_options()
            (spec(), boost::program_options::value<T>(&mDefault), mDesc.c_str());
    }

    GossOptionDefaulted(const std::string& pLong, const std::string& pShort, const T& pDefault,
                        const std::string& pDesc)
        : GossOption(pLong, pShort), mDesc(pDesc), mDefault(pDefault)
    {
    }

private:
    mutable std::string mDesc;
    const T mDefault;
};

template<typename T>
void
GossOptions::addOpt(const std::string& pLong, const std::string& pShort, const std::string& pDesc)
{
    BOOST_ASSERT(opts.find(pLong) == opts.end());
    opts[pLong] = GossOptionPtr(new GossOptionOptional<T>(pLong, pShort, pDesc));
}

template<typename T>
void
GossOptions::addOpt(const std::string& pLong, const std::string& pShort, const T& pDefault, const std::string& pDesc)
{
    BOOST_ASSERT(opts.find(pLong) == opts.end());
    opts[pLong] = GossOptionPtr(new GossOptionDefaulted<T>(pLong, pShort, pDefault, pDesc));
}

template<>
void
GossOptions::addOpt<bool>(const std::string& pLong, const std::string& pShort, const std::string& pDesc);
