#ifndef GOSSCMDDOTSUPERGRAPH_HH
#define GOSSCMDDOTSUPERGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

#ifndef STD_MATH_H
#include <math.h>
#define STD_MATH_H
#endif

class GossCmdDotSupergraph : public GossCmd
{
    typedef std::vector<std::string> strings;

public:
    
    struct Style
    {
        bool mLabelNodes;
        bool mLabelEdges;
        bool mPointNodes;
        std::string mCoreColour;
        std::string mFringeColour;
    };

    void operator()(const GossCmdContext& pCxt);

    GossCmdDotSupergraph(const std::string& pIn, const std::string& pOut,
                         const Style& pStyle)
        : mIn(pIn), mOut(pOut), 
          mStyle(pStyle)
    {
    }

private:

    const std::string mIn;
    const std::string mOut;
    const Style mStyle;
};


class GossCmdFactoryDotSupergraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryDotSupergraph();
};

#endif // GOSSCMDDOTSUPERGRAPH_HH
