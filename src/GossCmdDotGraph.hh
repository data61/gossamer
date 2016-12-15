// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDDOTGRAPH_HH
#define GOSSCMDDOTGRAPH_HH

#ifndef GOSSCMD_HH
#include "GossCmd.hh"
#endif

#ifndef STD_MATH_H
#include <math.h>
#define STD_MATH_H
#endif

class GossCmdDotGraph : public GossCmd
{
    typedef std::vector<std::string> strings;

public:
    
    struct Style
    {
        enum EdgeScale { Const, Linear, Sqrt, Log };

        double scaleEdge(uint64_t x) const
        {
            return   mEdgeScale == Const ? 1.0
                   : mEdgeScale == Linear ? x
                   : mEdgeScale == Sqrt ? sqrt((double)x)
                   : log2((double)(1 + x));
        }

        bool mLabelNodes;
        bool mLabelEdges;
        bool mShowCounts;
        bool mCollapseLinearPaths;
        bool mPointNodes;
        std::vector<std::string> mColours;
        EdgeScale mEdgeScale;
    };

    void operator()(const GossCmdContext& pCxt);

    GossCmdDotGraph(const std::string& pIn, const std::string& pOut,
                    const strings& pFastaNames, const strings& pFastqNames, const strings& pLineNames,
                    const bool pSkipSingletons, const Style& pStyle)
        : mIn(pIn), mOut(pOut), 
          mFastaNames(pFastaNames), mFastqNames(pFastqNames), mLineNames(pLineNames),
          mSkipSingletons(pSkipSingletons), mStyle(pStyle)
    {
    }

private:

    bool haveReadFiles() const
    {
        return !(   mFastaNames.empty()
                 && mFastqNames.empty()
                 && mLineNames.empty());
    }

    const std::string mIn;
    const std::string mOut;
    const strings mFastaNames;
    const strings mFastqNames;
    const strings mLineNames;
    const bool mSkipSingletons;
    const Style mStyle;
};


class GossCmdFactoryDotGraph : public GossCmdFactory
{
public:
    GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts);

    GossCmdFactoryDotGraph();
};

#endif // GOSSCMDDOTGRAPH_HH
