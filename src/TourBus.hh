#ifndef TOURBUS_HH
#define TOURBUS_HH

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

class TourBus
{
public:
    TourBus(Graph& pGraph, Logger& pLog);

    void setNumThreads(size_t pNumThreads);
    void setMaximumSequenceLength(size_t pMaxSequenceLength);
    void setMaximumEditDistance(size_t pMaxEditDistance);
    void setMaximumRelativeErrors(double pMaxRelativeErrors);
    void setCoverageCutoff(uint64_t pCutoff);
    void setCoverageRelativeCutoff(double pRelCutoff);

    bool pass();
    bool singleNode(const Graph::Node& pNode);

    uint64_t removedEdgesCount() const;

    void writeModifiedGraph(Graph::Builder& pBuilder) const;

private:
    struct Impl;
    std::shared_ptr<Impl> mPImpl;
};

#endif
