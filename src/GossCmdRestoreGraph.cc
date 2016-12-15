// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdRestoreGraph.hh"

#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "Graph.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;

namespace // anonymous
{

    position_type getEdge(const string& pStr)
    {
        Gossamer::position_type x(0);
        for (uint64_t i = 0; i < pStr.size(); ++i)
        {
            switch (pStr[i])
            {
                case 'A':
                case 'a':
                {
                    x = (x << 2) | 0ULL;
                    break;
                }
                case 'C':
                case 'c':
                {
                    x = (x << 2) | 1ULL;
                    break;
                }
                case 'G':
                case 'g':
                {
                    x = (x << 2) | 2ULL;
                    break;
                }
                case 'T':
                case 't':
                {
                    x = (x << 2) | 3ULL;
                    break;
                }
                default:
                {
                    BOOST_THROW_EXCEPTION(
                        Gossamer::error()
                            << Gossamer::general_error_info("invalid sequence " + pStr));
                }
            }
        }
        return x;
    }

} // namespace anonymous

void
GossCmdRestoreGraph::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);

    FileFactory::InHolderPtr inPtr(fac.in(mIn));
    istream& in(**inPtr);

    string x;
    getline(in, x);
    if (!in.good())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << errinfo_file_name(mIn)
                << errinfo_errno(errno)
                << Gossamer::general_error_info("unexpected end of file"));
    }
    uint64_t k;
    uint64_t n;
    uint64_t flags;
    in >> k >> n >> flags;
    if (!in.good())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << errinfo_file_name(mIn)
                << errinfo_errno(errno)
                << Gossamer::general_error_info("unexpected end of file"));
    }

    bool asymmetric = flags & (1ull << Graph::Header::fAsymmetric);

    Graph::Builder bld(k, mOut, fac, n, asymmetric);
    while (in.good())
    {
        x.clear();
        uint32_t c;
        in >> x >> c;
        if (!in.good())
        {
            break;
        }
        if (x.size() != k + 1)
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_file_name(mIn)
                    << errinfo_errno(errno)
                    << Gossamer::general_error_info("sequence " + x + " has wrong length"));
        }
        bld.push_back(getEdge(x), c);
    }
    bld.end();
}


GossCmdPtr
GossCmdFactoryRestoreGraph::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string in = "-";
    chk.getOptional("input-file", in);

    string out;
    FileFactory& fac(pApp.fileFactory());
    chk.getMandatory("graph-out", out, GossOptionChecker::FileCreateCheck(fac, true));

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdRestoreGraph(in, out));
}

GossCmdFactoryRestoreGraph::GossCmdFactoryRestoreGraph()
    : GossCmdFactory("read in a graph from a robust text representation.")
{
    mCommonOptions.insert("graph-out");
    mCommonOptions.insert("input-file");
}
