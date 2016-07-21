
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
