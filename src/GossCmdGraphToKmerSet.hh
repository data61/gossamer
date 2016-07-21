#ifndef GOSSCMDGRAPHTOKMERSET_HH
#define GOSSCMDGRAPHTOKMERSET_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdGraphToKmerSet : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdGraphToKmerSet(const std::string& pIn, const std::string& pOut)
        : mIn(pIn), mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
};


class GossCmdFactoryGraphToKmerSet : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryGraphToKmerSet();
};

#endif // GOSSCMDGRAPHTOKMERSET_HH
