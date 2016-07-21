#ifndef GOSSCMDCLASSIFYREADS_HH
#define GOSSCMDCLASSIFYREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdClassifyReads : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdClassifyReads(const std::string& pIn,
                       const strings& pFastas, const strings& pFastqs, const strings& pLines,
                       uint64_t pBufferSize, uint64_t pNumThreads, bool pPairs)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mBufferSize(pBufferSize), mNumThreads(pNumThreads), mPairs(pPairs)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const uint64_t mBufferSize;
    const uint64_t mNumThreads;
    const bool mPairs;
};

class GossCmdFactoryClassifyReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryClassifyReads();
};

#endif // GOSSCMDCLASSIFYREADS_HH
