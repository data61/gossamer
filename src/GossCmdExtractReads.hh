#ifndef GOSSCMDEXTRACTREADS_HH
#define GOSSCMDEXTRACTREADS_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

class GossCmdExtractReads : public GossCmd
{
public:
    void operator()(const GossCmdContext& pCxt);

    GossCmdExtractReads(const std::string& pIn, 
                        const std::vector<std::string>& pFastas,
                        const std::vector<std::string>& pFastqs,
                        const std::vector<std::string>& pLines,
                        const std::string& pOut)
        : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), mOut(pOut)
    {
    }

private:
    const std::string mIn;
    const std::vector<std::string> mFastas;
    const std::vector<std::string> mFastqs;
    const std::vector<std::string> mLines;
    const std::string mOut;
};


class GossCmdFactoryExtractReads : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryExtractReads();
};

#endif // GOSSCMDEXTRACTREADS_HH
