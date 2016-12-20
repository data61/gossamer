// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildGraph.hh"

#include "Graph.hh"
#include "StringFileFactory.hh"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGossCmdBuildGraph
#include "testBegin.hh"


#if 0
const char* genome =
    "ACCCCCGTCCCGGGTTCAGAGTCACGTACGGAGTGACTAATAGCCGTTGGATTATCTTACACGTGGACGA"
    "TCAGGATCTGTGATTCGTGAAGCGAATCTGACGGAAGATCGTTCACACTCACGTGGTGGGTCCCGACAAT"
    "TGCTTTTGCTTTACTTTATCTAAAGTAAAGCAAGTGGGTCTACTTTAATTATTCTTTTTTGGTGGAGTAC"
    "AGCTGTCTTTGCTTCTTCACGAAGCAAAGGACTTTCTCTCTCTTCTATAAAAAAGCTGTATGTGCAGAAG"
    "ATGAATTCGTTCATTCATCTTCTTCTACATCAAATCATCTTCTCTCATCAACATTAAGCTTATATAGCTC"
    "TATGCCTTTGGTCGATTATTTGTTGGTTGATGAGGCTACTGAAGAGTTAATAACGTCAGAGAGGAAACGT"
    "AGATCAGTCGTATGTTCTGATGATGATTCTCAGGTAATCAATGTCAAAGTTGAAGATATAGCTATTGATT"
    "TGGAAGACAGAGTTGTCGTTAAGTTGAAGTTCAGATTATGTTATAAATACAGGAGGTTGCTTGATTTAAC"
    "GCTTCTTGGCTGTCGTATGAAAGTTCATACTGTATTGAAGACCACCTCTGCTCCATCGTTGAAGAGTGTT"
    "TTGCAGAAGAGACTTAATATGATTTGTGATGGTAATCATTTAATATCAATTAGATTGTTTTTCATTAATA"
    "TTAATCAGTTGATTAATAATTGTAAATGGATATCATCTGTAGAAGATGTATATCCAATATGTACTTTGTA"
    "TCATATTCAATATTCAAATGTAATTAATGCTAATGAGATATTTAATAATACTTAATATGTATTTGTTTAT"
    "ATTGATATTGTTTTTTAATTACTCTGCGAAGCTATATGTCTCGGCCCAATAGGCCCAATAGTCTTAAGGC"
    "CCAATAAATTTTACATTAAATTGGATCAGCTGACGTCAGCTGATCCCGTGATGACGTAGGGACGGGGCTT"
    "AGTATT";

BOOST_AUTO_TEST_CASE(test1)
{
    std::mt19937 rng(17);
    std::uniform_real_distribution<> dist;

    static const uint64_t N = 100;
    static const uint64_t L = 30;

    const string G(genome);
    string R;
    for (uint64_t i = 0; i < N; ++i)
    {
        uint64_t x = dist(rng) * (G.size() - L + 1);
        string r = G.substr(x, L);
        R += ">" + lexical_cast<string>(x) + "\n";
        R += r;
        R += "\n";
    }

    StringFileFactory fac;

    {
        Logger log("log.txt", fac);

        fac.addFile("reads.fa", R);

        std::vector<string> fastas;
        std::vector<string> fastqs;
        std::vector<string> lines;

        fastas.push_back("reads.fa");

        GossCmdBuildGraph cmd(15, 16, (1ULL << 16), 2, "graph", fastas, fastqs, lines);

        boost::program_options::variables_map opts;
        GossCmdContext cxt(fac, log, "build-graph", opts);
        cmd(cxt);
    }

    BOOST_CHECK(fac.fileExists("graph-counts-hist.txt"));

    FileFactory::InHolderPtr ih(fac.in("graph-counts-hist.txt"));
    istream& in(**ih);
    uint64_t m, f;
    BOOST_CHECK(in.good());
    in >> m >> f;
    BOOST_CHECK(in.good());
    BOOST_CHECK_EQUAL(m, 1);
    BOOST_CHECK_EQUAL(f, 628);
    in >> m >> f;
    BOOST_CHECK(in.good());
    BOOST_CHECK_EQUAL(m, 2);
    BOOST_CHECK_EQUAL(f, 563);
    in >> m >> f;
    BOOST_CHECK(in.good());
    BOOST_CHECK_EQUAL(m, 3);
    BOOST_CHECK_EQUAL(f, 228);
    in >> m >> f;
    BOOST_CHECK(in.good());
    BOOST_CHECK_EQUAL(m, 4);
    BOOST_CHECK_EQUAL(f, 88);
    in >> m >> f;
    BOOST_CHECK(in.good());
    BOOST_CHECK_EQUAL(m, 5);
    BOOST_CHECK_EQUAL(f, 42);
    in >> m >> f;
    BOOST_CHECK(!in.good());
}
#endif

static const char* reads = ">\nAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";

BOOST_AUTO_TEST_CASE(polyA_tiny)
{

    StringFileFactory fac;
    {
        Logger log("log.txt", fac);

        fac.addFile("reads.fa", reads);

        std::vector<string> fastas;
        std::vector<string> fastqs;
        std::vector<string> lines;

        fastas.push_back("reads.fa");

        GossCmdBuildGraph cmd(27, 16, (1ULL << 16), 2, "graph", fastas, fastqs, lines);

        boost::program_options::variables_map opts;
        GossCmdContext cxt(fac, log, "build-graph", opts);
        cmd(cxt);
    }

    GraphPtr gPtr = Graph::open("graph", fac);
    Graph& g(*gPtr);
    BOOST_CHECK_EQUAL(g.count(), 2);
    Graph::Edge e = g.select(0);
    uint32_t m = g.multiplicity(e);
    Graph::Edge e_p = g.reverseComplement(e);
    BOOST_CHECK_EQUAL(g.rank(e_p), 1);
    uint32_t m_p = g.multiplicity(e_p);
    BOOST_CHECK_EQUAL(m, m_p);
}

static const char* readWithNs =
">\n"
"NACTTTTGATGCAATGTCAAATTCTCCNCGTCATTCGCAACTGAATACAAGNGAATTTGGAAGGAGAATNTGGTA\n";

BOOST_AUTO_TEST_CASE(test124ReadWithNs)
{

    StringFileFactory fac;
    {
        Logger log("log.txt", fac);

        fac.addFile("reads.fa", readWithNs);

        std::vector<string> fastas;
        std::vector<string> fastqs;
        std::vector<string> lines;

        fastas.push_back("reads.fa");

        GossCmdBuildGraph cmd(15, 16, (1ULL << 16), 2, "graph", fastas, fastqs, lines);

        boost::program_options::variables_map opts;
        GossCmdContext cxt(fac, log, "build-graph", opts);
        cmd(cxt);
    }

    GraphPtr gPtr = Graph::open("graph", fac);
    Graph& g(*gPtr);
    BOOST_CHECK_EQUAL(g.count(), 42);
}

#include "testEnd.hh"
