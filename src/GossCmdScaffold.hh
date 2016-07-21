#ifndef GOSSCMDSCAFFOLD_HH
#define GOSSCMDSCAFFOLD_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdScaffold : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdScaffold(const std::string& pIn, uint64_t pMinLinkCount,
                    uint64_t pExtract)
        : mIn(pIn), mMinLinkCount(pMinLinkCount), mExtract(pExtract)
    {
    }

private:
    const std::string mIn;
    const uint64_t mMinLinkCount;
    const uint64_t mExtract;
};


class GossCmdFactoryScaffold : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryScaffold();
};

#endif // GOSSCMDSCAFFOLD_HH
