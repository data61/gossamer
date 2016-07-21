#ifndef GOSSCMDUPGRADEGRAPH_HH
#define GOSSCMDUPGRADEGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdUpgradeGraph : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdUpgradeGraph(const std::string& pIn)
        : mIn(pIn)
    {
    }

private:
    const std::string mIn;
};


class GossCmdFactoryUpgradeGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryUpgradeGraph();
};

#endif // GOSSCMDUPGRADEGRAPH_HH
