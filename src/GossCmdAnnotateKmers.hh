#ifndef GOSSCMDANNOTATEKMERS_HH
#define GOSSCMDANNOTATEKMERS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdAnnotateKmers : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdAnnotateKmers(const std::string& pRef, const std::string& pAnnotList, const std::string& pMergeList,
                         uint64_t pNumThreads)
        : mRef(pRef), mAnnotList(pAnnotList), mMergeList(pMergeList), mNumThreads(pNumThreads)
    {
    }

private:
    const std::string mRef;
    const std::string mAnnotList;
    const std::string mMergeList;
    const uint64_t mNumThreads;
};


class GossCmdFactoryAnnotateKmers : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryAnnotateKmers();
};

#endif // GOSSCMDANNOTATEKMERS_HH
