// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdThreadReads.hh"
#include "GossCmdBuildEntryEdgeSet.hh"
#include "GossCmdBuildGraph.hh"
#include "StringFileFactory.hh"
#include "EntryEdgeSet.hh"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGossCmdThreadReads
#include "testBegin.hh"

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

string edge(uint64_t pK, uint64_t pX)
{
    string s;
    for (uint64_t i = 0; i < pK; ++i)
    {
        s.push_back("ACGT"[pX & 3]);
        pX >>= 2;
    }
    return string(s.rbegin(), s.rend());
}

BOOST_AUTO_TEST_CASE(testBuildEntrySets)
{
    std::mt19937 rng(17);
    std::uniform_real_distribution<> dist;

    static const uint64_t N = 1000;
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

        fac.files["reads.fa"] = R;

        vector<string> fastas;
        vector<string> fastqs;
        vector<string> lines;

        fastas.push_back("reads.fa");

        GossCmdBuildGraph cmd(15, 16, 2, "graph", fastas, fastqs, lines);

        boost::program_options::variables_map opts;
        GossCmdContext cxt(fac, log, "build-graph", opts);

        cmd(cxt);
    }

    {
        Logger log("log.txt", fac);

        vector<string> fastas;
        vector<string> fastqs;
        vector<string> lines;

        fastas.push_back("reads.fa");

        GossCmdBuildEntryEdgeSet cmd("graph");

        boost::program_options::variables_map opts;
        GossCmdContext cxt(fac, log, "build-entry-edge-set", opts);

        cmd(cxt);
    }

    {
        EntryEdgeSet es("graph-entries", fac);

        BOOST_CHECK_EQUAL(es.count(), 5);
        for (uint64_t i = 0; i < es.count(); ++i)
        {
            EntryEdgeSet::Edge e(es.select(i));
            uint64_t r = es.endRank(i);
            EntryEdgeSet::Edge f(es.select(r));
            EntryEdgeSet::Edge fp(es.reverseComplement(f));
            //cerr << edge(es.K() + 1, e.value()) << '\t' << edge(es.K() + 1, fp.value()) << endl;
        }
    }

    {
        Logger log("log.txt", fac);

        vector<string> fastas;
        vector<string> fastqs;
        vector<string> lines;

        fastas.push_back("reads.fa");

        GossCmdThreadReads cmd("graph", fastas, fastqs, lines, 1024 * 1024, 4);

        boost::program_options::variables_map opts;
        GossCmdContext cxt(fac, log, "thread-reads", opts);

        cmd(cxt);
    }

}

#include "testEnd.hh"
