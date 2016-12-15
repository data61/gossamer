// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
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
