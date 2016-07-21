#ifndef GOSSCMDFIXREADS_HH
#define GOSSCMDFIXREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdFixReads : public GossCmd
{
public:
    typedef std::vector<std::string> strings;

    void operator()(const GossCmdContext& pCxt);

    GossCmdFixReads(const std::string& pIn, const strings& pFastas, const strings& pFastqs, const strings& pLines,
                    const std::string& pOut, const uint64_t pNumThreads)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines),
          mOut(pOut), mNumThreads(pNumThreads)
    {
    }

private:
    const std::string mIn;
    const strings mFastas;
    const strings mFastqs;
    const strings mLines;
    const std::string mOut;
    const uint64_t mNumThreads;
};


class GossCmdFactoryFixReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryFixReads();
};

#endif // GOSSCMDFIXREADS_HH
