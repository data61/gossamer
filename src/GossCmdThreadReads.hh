#ifndef GOSSCMDTHREADREADS_HH
#define GOSSCMDTHREADREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdThreadReads : public GossCmd
{
public:

    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdThreadReads(const std::string& pIn,
                       const strings& pFastas, const strings& pFastqs, const strings& pLines,
                       uint64_t pMinLinkCount, bool pInferCoverage, uint64_t pExpectedCoverage, 
                       uint64_t pNumThreads, uint64_t pCacheRate, bool pRemScaf)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mMinLinkCount(pMinLinkCount), mInferCoverage(pInferCoverage), mExpectedCoverage(pExpectedCoverage), 
          mNumThreads(pNumThreads), mCacheRate(pCacheRate), mRemScaf(pRemScaf)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const uint64_t mMinLinkCount;
    const bool mInferCoverage;
    const uint64_t mExpectedCoverage;
    const uint64_t mNumThreads;
    const uint64_t mCacheRate;
    const bool mRemScaf;
};

class GossCmdFactoryThreadReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryThreadReads();
};

#endif // GOSSCMDTHREADREADS_HH
