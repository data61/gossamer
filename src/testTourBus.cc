// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "TourBus.hh"

#include "Graph.hh"
#include "StringFileFactory.hh"
#include "Utils.hh"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

using namespace boost;
using namespace std;

#undef DUMP_GRAPHS
#ifdef DUMP_GRAPHS
#include "DumpGraph.hh"
#endif

#define GOSS_TEST_MODULE TestTourBus
#include "testBegin.hh"

static const char* genome2 =
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT";

static const char* reads2[] = {
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCA",
    "GCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGC",
    "AGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT",
    "CTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTcGGGAGTCCTGCATCCCGGCATTAGGCGTGCACA", // error
    NULL
};

static const char* genome3 =
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT";

static const char* reads3[] = {
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCA",
    "GCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGC",
    "AGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT",
    "CTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTcGGGAGTCCTGCATCCCGGCATTAGGCGTGCACA", // error
    "CTTCTATTAGGTAGTGCATCTATTTACATCTCTTAtTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACA", // error
    NULL
};

static const char* genome4 =
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT";

static const char* reads4[] = {
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCA",
    "GCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGC",
    "AGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT",
    "CTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTcGGGAGTCCTGCATCCCGGCATTAGGCGTGCACA", // error
    "CTTCTATTAGGTAGTGCATCTATTTACATCTCTTtGTGCCTAGGGAGTCCTGCATCCCGGCATTAGGCGTGCACA", // error
    NULL
};

static const char* genome5 =
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCAAGGTAGTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT";

static const char* reads5[] = {
    "GTTCTGGAACGCGCTTCTATTAGGTAGTGCATCTATTTACATCTCTTAGT",
    "AGTGCATCTATTTACATCTCTTAGTGCCTAGGGAGTCCTGCAAGGTAGTG",
    "TTAGTGCCTAGGGAGTCCTGCAAGGTAGTGCATCCCGGCATTAGGCGTGC",
    "TCCTGCAAGGTAGTGCATCCCGGCATTAGGCGTGCACAAATGTTTATATT",
    NULL
};

static const char* genome6 =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAGCAGACTGCCAGGT";

static const char* reads6[] = {
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAGCAGACTGCCAGGT",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAGCAGACTGCCAGG",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGAAAAAA",
    "AAAAAAAAAAAAAAAAAAAAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "AAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATA",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAGCAGACTGCCAGGT",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATAGCAGACTGCCAGG",
    NULL
};

void seqToVec(const char* pSeq, SmallBaseVector& pVec)
{
    pVec.clear();
    for (const char* s = pSeq; *s; ++s)
    {
        switch (*s)
        {
            case 'A':
            case 'a':
            {
                pVec.push_back(0);
                break;
            }
            case 'C':
            case 'c':
            {
                pVec.push_back(1);
                break;
            }
            case 'G':
            case 'g':
            {
                pVec.push_back(2);
                break;
            }
            case 'T':
            case 't':
            {
                pVec.push_back(3);
                break;
            }
        }
    }
}

void writeKmer(ostream& pOut, uint64_t pK, uint64_t pKmer)
{
    uint64_t x = 0;
    for (uint64_t i = 0; i < pK; ++i)
    {
        x = (x << 2) | (pKmer & 3);
        pKmer >>= 2;
    }
    for (uint64_t i = 0; i < pK; ++i)
    {
        static const char map[] = { 'A', 'C', 'G', 'T' };
        pOut << map[x & 3];
        x >>= 2;
    }
}

void
doTest(uint64_t pK, const char* pGenome, const char* pReads[])
{
    const uint64_t K1 = pK + 1;

    StringFileFactory fac;
    Logger log("log.txt", fac);
    map<Gossamer::position_type,uint64_t> k1mers;
    SmallBaseVector vec;
    {
        for (uint64_t i = 0; pReads[i]; ++i)
        {
            seqToVec(pReads[i], vec);
            for (uint64_t j = 0; j < vec.size() - K1; ++j)
            {
                Gossamer::position_type x = vec.kmer(K1, j);
                ++k1mers[x];
                x.reverseComplement(K1);
                ++k1mers[x];
            }
        }
        Graph::Builder b(pK, "x", fac, k1mers.size());
        for (map<Gossamer::position_type,uint64_t>::const_iterator i = k1mers.begin();
                i != k1mers.end(); ++i)
        {
            Gossamer::position_type k = i->first;
            uint32_t c = i->second;
            b.push_back(k, c);
        }
        b.end();
    }

    GraphPtr gPtr = Graph::open("x", fac);
    Graph& g(*gPtr);
#ifdef DUMP_GRAPHS
    {
        std::ofstream out("graph.before.dot");
        DumpGraph dump(g);
        dump.generateDot(out);
    }
#endif // DUMP_GRAPHS
    TourBus tourBus(g, log);
    tourBus.pass();

    {
        Graph::Builder b(pK, "y", fac, k1mers.size() - tourBus.removedEdgesCount());
        tourBus.writeModifiedGraph(b);
    }

    GraphPtr goutPtr = Graph::open("y", fac);
    Graph& gout(*gPtr);
#ifdef DUMP_GRAPHS
    {
        std::ofstream out("graph.after.dot");
        DumpGraph dump(gout);
        dump.generateDot(out);
    }
#endif // DUMP_GRAPHS

    vec.clear();
    seqToVec(pGenome, vec);
    for (uint64_t j = 0; j < vec.size() - K1; ++j)
    {
        Gossamer::position_type x = vec.kmer(K1, j);
        BOOST_CHECK(gout.access(Graph::Edge(x)));
    }
}

BOOST_AUTO_TEST_CASE(test_reads2)
{
    doTest(7, genome2, reads2);
}

BOOST_AUTO_TEST_CASE(test_reads3)
{
    doTest(7, genome3, reads3);
}

BOOST_AUTO_TEST_CASE(test_reads4)
{
    doTest(7, genome4, reads4);
}

BOOST_AUTO_TEST_CASE(test_reads5)
{
    doTest(7, genome5, reads5);
}

BOOST_AUTO_TEST_CASE(test_reads6)
{
    doTest(11, genome6, reads6);
}

#include "testEnd.hh"


