#ifndef GOSSCMDRESTOREGRAPH_HH
#define GOSSCMDRESTOREGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdRestoreGraph : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdRestoreGraph(const std::string& pIn,
                     const std::string& pOut)
        : mIn(pIn), mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
};


class GossCmdFactoryRestoreGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryRestoreGraph();
};

#endif // GOSSCMDRESTOREGRAPH_HH
