// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdUpgradeGraph.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "RRRArray.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

namespace // anonymous
{
    static const uint64_t sOldGraphVersion = 2010072301ULL;
    static const uint64_t sNewGraphVersion = 2011071101ULL;

    struct GraphHeader
    {
        uint64_t version;
        uint64_t K;
    };

    static const uint64_t sRRRRankHeaderVersion = 2011032901ULL;

    struct RRRRankHeader
    {
        uint64_t version;
        RRRArray::position_type size;
        RRRArray::rank_type count;
    };

    template<class HeaderType>
    void
    loadAndCheckHeader(FileFactory& pFactory, const std::string& pFilename,
                       uint64_t pVersion, HeaderType& pHeader)
    {
        FileFactory::InHolderPtr ip(pFactory.in(pFilename));
        istream& i(**ip);
        i.read(reinterpret_cast<char*>(&pHeader), sizeof(pHeader));
        if (pHeader.version != pVersion)
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::version_mismatch_info(
                       pair<uint64_t,uint64_t>(pHeader.version, pVersion)));
        }
    }

    template<class HeaderType>
    void
    writeHeader(FileFactory& pFactory, const std::string& pFilename,
                const HeaderType& pHeader)
    {
        FileFactory::OutHolderPtr op(pFactory.out(pFilename));
        ostream& o(**op);
        o.write(reinterpret_cast<const char*>(&pHeader), sizeof(pHeader));
    }

    void
    convertRRRRankToSparseArray(FileFactory& pFactory,
                                const RRRRankHeader& pHeader,
                                const std::string& pName)
    {
        RRRRank::LazyIterator in(RRRRank::lazyIterator(pName, pFactory));
        SparseArray::Builder out(
            SparseArray::Builder(pName, pFactory, 
                    Gossamer::position_type(pHeader.size), pHeader.count));
        while (in.valid())
        {
            out.push_back(Gossamer::position_type(*in));
            ++in;
        }
        out.end(Gossamer::position_type(pHeader.size));
    }
} // namespace anonymous

void
GossCmdUpgradeGraph::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);

    string headerName = mIn + ".header";
    string headerBackupName = mIn + ".header.bak";
    string ord1pName = mIn + "-counts.ord1p";
    string ord1pHdrName = ord1pName + ".header";
    string ord1pHdrBackupName = ord1pHdrName + ".bak";
    string ord2pName = mIn + "-counts.ord2p";
    string ord2pHdrName = ord2pName + ".header";
    string ord2pHdrBackupName = ord2pHdrName + ".bak";

    // Read the headers and check version numbers.
    log(info, "Checking version numbers");
    GraphHeader graphHeader;
    loadAndCheckHeader<GraphHeader>(fac, headerName, sOldGraphVersion,
                                    graphHeader);

    RRRRankHeader ord1pHeader;
    loadAndCheckHeader<RRRRankHeader>(fac, ord1pHdrName, sRRRRankHeaderVersion,
                                      ord1pHeader);

    RRRRankHeader ord2pHeader;
    loadAndCheckHeader<RRRRankHeader>(fac, ord2pHdrName, sRRRRankHeaderVersion,
                                      ord2pHeader);

    // Back up the headers.
    log(info, "Backing up old headers");
    fac.copy(headerName, headerBackupName);
    fac.copy(ord1pHdrName, ord1pHdrBackupName);
    fac.copy(ord2pHdrName, ord2pHdrBackupName);

    try {
        // Convert the two sparse bitmaps.
        log(info, "Converting over ord1p");
        convertRRRRankToSparseArray(fac, ord1pHeader, ord1pName);
        log(info, "Converting over ord2p");
        convertRRRRankToSparseArray(fac, ord2pHeader, ord2pName);

        // Finally, write the new graph header.
        log(info, "Writing new header");
        graphHeader.version = sNewGraphVersion;
        writeHeader<GraphHeader>(fac, headerName, graphHeader);
        log(info, "Done");
    }
    catch (...)
    {
        log(fatal, "Could not upgrade the graph.");
        log(fatal, "Please check for backup files and restore them if necessary.");
        log(fatal, "The commands that you may need to run are:");
        log(fatal, "    mv " + headerBackupName + " " + headerName);
        log(fatal, "    mv " + ord1pHdrBackupName + " " + ord1pHdrName);
        log(fatal, "    mv " + ord2pHdrBackupName + " " + ord2pHdrName);
        throw;
    }
}


GossCmdPtr
GossCmdFactoryUpgradeGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in;
    chk.getMandatory("graph", in);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdUpgradeGraph(in));
}

GossCmdFactoryUpgradeGraph::GossCmdFactoryUpgradeGraph()
    : GossCmdFactory("upgrade a graph to the current binary format in-place")
{
    mSpecificOptions.addOpt<std::string>("graph", "",
            "graph to upgrade");
}
