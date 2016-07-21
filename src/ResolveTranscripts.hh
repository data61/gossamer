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

#ifndef BOOST_SCOPED_PTR_HPP
#include <boost/scoped_ptr.hpp>
#define BOOST_SCOPED_PTR_HPP
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
    class Impl;
    boost::scoped_ptr<Impl> mPImpl;
};


#endif
