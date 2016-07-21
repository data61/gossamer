#ifndef GOSSCMDEXTRACTCOREGENOME_HH
#define GOSSCMDEXTRACTCOREGENOME_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdExtractCoreGenome : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdExtractCoreGenome(const std::vector<std::string>& pSrcs, const std::string& pDest)
        : mSrcs(pSrcs), mDest(pDest)
    {
    }

private:
    const std::vector<std::string> mSrcs;
    const std::string mDest;
};


class GossCmdFactoryExtractCoreGenome : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryExtractCoreGenome();
};

#endif // GOSSCMDEXTRACTCOREGENOME_HH
