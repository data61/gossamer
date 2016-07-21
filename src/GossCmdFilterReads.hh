#ifndef GOSSCMDFILTERREADS_HH
#define GOSSCMDFILTERREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdFilterReads : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdFilterReads(const std::string& pIn,
                       const strings& pFastas, const strings& pFastqs, const strings& pLines,
                       bool pPairs, bool pCount, uint64_t pNumThreads,
                       const std::string& pMatch, const std::string& pNonMatch)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
          mPairs(pPairs), mCount(pCount), mNumThreads(pNumThreads),
          mMatch(pMatch), mNonMatch(pNonMatch)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const bool mPairs;
    const bool mCount;
    const uint64_t mNumThreads;
    const std::string mMatch;
    const std::string mNonMatch;
};

class GossCmdFactoryFilterReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryFilterReads();
};

#endif // GOSSCMDFILTERREADS_HH
