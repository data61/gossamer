#ifndef GOSSCMDDUMPGRAPH_HH
#define GOSSCMDDUMPGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdDumpGraph : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdDumpGraph(const std::string& pIn,
                     const std::string& pOut)
        : mIn(pIn), mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
};


class GossCmdFactoryDumpGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryDumpGraph();
};

#endif // GOSSCMDDUMPGRAPH_HH
