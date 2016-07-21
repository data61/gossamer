#ifndef GOSSCMDPRINTPATHS_HH
#define GOSSCMDPRINTPATHS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdPrintContigs : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdPrintContigs(const std::string& pIn,
                        uint64_t pC, uint64_t pL, bool pPrintRcs,
                        bool pOmitSequence, bool pPrintLinearSegments,
                        bool pVerboseHeaders, bool pNoLineBreaks,
                        bool pPrintEntailed, uint64_t pNumThreads,
                        const std::string& pOut)
        : mIn(pIn), mC(pC), mL(pL), mPrintRcs(pPrintRcs),
          mOmitSequence(pOmitSequence), mPrintLinearSegments(pPrintLinearSegments),
          mVerboseHeaders(pVerboseHeaders), mNoLineBreaks(pNoLineBreaks),
          mPrintEntailed(pPrintEntailed), mNumThreads(pNumThreads),
          mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const uint64_t mC;
    const uint64_t mL;
    const bool mPrintRcs;
    const bool mOmitSequence;
    const bool mPrintLinearSegments;
    const bool mVerboseHeaders;
    const bool mNoLineBreaks;
    const bool mPrintEntailed;
    const uint64_t mNumThreads;
    const std::string mOut;
};


class GossCmdFactoryPrintContigs : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryPrintContigs();
};

#endif // GOSSCMDPRINTPATHS_HH
