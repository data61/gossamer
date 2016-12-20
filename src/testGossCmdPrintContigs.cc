// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdPrintContigs.hh"

#include "Debug.hh"
#include "Graph.hh"
#include "StringFileFactory.hh"
#include "GossCmdBuildGraph.hh"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGossCmdPrintContigs
#include "testBegin.hh"


static const char* reads =
">1\n"
"CCCCAAGCTGACCATTTTTGTCCACTTATTTTTGCATGATGGTTGCCCACTTCTTTCCCT\n"
"TCTGTGTTGGAACTA\n"
">2\n"
"TCGATGGTATGCGCTCGGTCAAAGCCTTTGCCAGGTCCTCACCGAGTGGAGCTGCACCGG\n"
"AAGACACATCCTCCATGGAGGATGTGTCTTCCGGTGCAGCTCCACTCGGTGAGGACCTGG\n"
"CAAAGGCTTTGACCGAGCGCATACCATCGA\n"
">3\n"
"TTTTTGAGAAATAATTAAGCTTCAATTTGAGAAAGAACGCCATACATTGCATGCTTTGTA\n"
"TTTTAAAGCAAAAAA\n";

BOOST_AUTO_TEST_CASE(test122palindrome)
{

    StringFileFactory fac;
    {
        Logger log("log.txt", fac);
        {
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
        {
            //Debug::enable("dump-graph-on-open");
            GossCmdPrintContigs cmd("graph", 0, 0, false, false, true, false, false, false, 1, "out.fa");

            boost::program_options::variables_map opts;
            GossCmdContext cxt(fac, log, "build-graph", opts);
            cmd(cxt);
        }
    }
    BOOST_CHECK(fac.fileExists("out.fa"));
    BOOST_CHECK_EQUAL(fac.readFile("out.fa"), reads);
}

static const char* longReadOnly =
">1\n"
"TCGATGGTATGCGCTCGGTCAAAGCCTTTGCCAGGTCCTCACCGAGTGGAGCTGCACCGG\n"
"AAGACACATCCTCCATGGAGGATGTGTCTTCCGGTGCAGCTCCACTCGGTGAGGACCTGG\n"
"CAAAGGCTTTGACCGAGCGCATACCATCGA\n";

BOOST_AUTO_TEST_CASE(test123MinLength)
{

    StringFileFactory fac;
    {
        Logger log("log.txt", fac);
        {
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
        {
            GossCmdPrintContigs cmd("graph", 0, 100, false, false, true, false, false, false, 1, "out.fa");

            boost::program_options::variables_map opts;
            GossCmdContext cxt(fac, log, "build-graph", opts);
            cmd(cxt);
        }
    }
    BOOST_CHECK(fac.fileExists("out.fa"));
    BOOST_CHECK_EQUAL(fac.readFile("out.fa"), longReadOnly);
}

#include "testEnd.hh"
