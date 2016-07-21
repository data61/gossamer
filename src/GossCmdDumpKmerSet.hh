#ifndef GOSSCMDDUMPKMERSET_HH
#define GOSSCMDDUMPKMERSET_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdDumpKmerSet : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdDumpKmerSet(const std::string& pIn,
                       const std::string& pOut)
        : mIn(pIn), mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const std::string mOut;
};


class GossCmdFactoryDumpKmerSet : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryDumpKmerSet();
};

#endif // GOSSCMDDUMPKMERSET_HH
