#ifndef GOSSCMDGROUPREADS_HH
#define GOSSCMDGROUPREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdGroupReads : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdGroupReads(const std::string& pIn,
                       const strings& pFastas, const strings& pFastqs, const strings& pLines,
                       bool pPairs, double pMaxMemory, uint64_t pNumThreads,
                       const std::string& pLhsName, const std::string& pRhsName, const std::string& pPrefix,
                       bool pDontWriteReads, bool pPreserveReadOrder)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mPairs(pPairs), mMaxMemory(pMaxMemory), mNumThreads(pNumThreads),
          mLhsName(pLhsName), mRhsName(pRhsName), mPrefix(pPrefix),
          mDontWriteReads(pDontWriteReads), mPreserveReadOrder(pPreserveReadOrder)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const bool mPairs;
    const double mMaxMemory;
    const uint64_t mNumThreads;
    const std::string mLhsName;
    const std::string mRhsName;
    const std::string mPrefix;
    const bool mDontWriteReads;
    const bool mPreserveReadOrder;
};

class GossCmdFactoryGroupReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryGroupReads();
};

#endif // GOSSCMDGROUPREADS_HH
