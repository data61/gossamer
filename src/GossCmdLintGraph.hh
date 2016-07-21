#ifndef GOSSCMDLINTGRAPH_HH
#define GOSSCMDLINTGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdLintGraph : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdLintGraph(const std::string& pIn, bool pDumpProperties = false)
        : mIn(pIn), mDumpProperties(pDumpProperties)
    {
    }

private:
    const std::string mIn;
    const bool mDumpProperties;
};

class GossCmdFactoryLintGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryLintGraph();
};

#endif // GOSSCMDLINTGRAPH_HH
