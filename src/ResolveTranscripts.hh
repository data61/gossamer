// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef RESOLVETRANSCRIPTS_HH
#define RESOLVETRANSCRIPTS_HH

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

#ifndef BOOST_UTILITY_HPP
#include <boost/utility.hpp>
#define BOOST_UTILITY_HPP
#endif

#ifndef BOOST_NONCOPYABLE_HPP
#include <boost/noncopyable.hpp>
#define BOOST_NONCOPYABLE_HPP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif


class ResolveTranscripts
        : private boost::noncopyable
{
public:
    enum { sMinReads = 4 };

    ResolveTranscripts(const std::string& pName, Graph& pGraph,
                        Logger& pLog, std::ostream& pOut,
                        uint64_t pMinLength, uint64_t pMappableReads);

    ~ResolveTranscripts();

    struct ReadInfo
    {
        SmallBaseVector mRead;
    };

    void addContig(const SmallBaseVector& pVec);

    void addReadPair(const ReadInfo& pLhs, const ReadInfo& pRhs);

    void processComponent();

private:
    struct Impl;
    std::unique_ptr<Impl> mPImpl;
};


#endif
