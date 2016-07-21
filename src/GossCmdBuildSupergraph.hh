#ifndef GOSSCMDBUILDSUPERGRAPH_HH
#define GOSSCMDBUILDSUPERGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdBuildSupergraph : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdBuildSupergraph(const std::string& pIn, bool pRemScaf)
        : mIn(pIn), mRemScaf(pRemScaf)
    {
    }

private:
    const std::string mIn;
    const bool mRemScaf;
};


class GossCmdFactoryBuildSupergraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryBuildSupergraph();
};

#endif // GOSSCMDBUILDSUPERGRAPH_HH
