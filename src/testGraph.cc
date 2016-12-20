// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "Graph.hh"
#include "StringFileFactory.hh"
#include "GossamerException.hh"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGraph
#include "testBegin.hh"

static const char* out0 = "ACTGGAACGCGCTTCTA";
static const char* out1 = "ACTGGAACGCGCTTCTC";
static const char* out2 = "ACTGGAACGCGCTTCTG";
static const char* out3 = "ACTGGAACGCGCTTCTT";

static const char* outs[] = { out0, out1, out2, out3, NULL };

void seqToVec(const char* pSeq, SmallBaseVector& pVec)
{
    pVec.clear();
    for (const char* s = pSeq; *s; ++s)
    {
        switch (*s)
        {
            case 'A':
            {
                pVec.push_back(0);
                break;
            }
            case 'C':
            {
                pVec.push_back(1);
                break;
            }
            case 'G':
            {
                pVec.push_back(2);
                break;
            }
            case 'T':
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

BOOST_AUTO_TEST_CASE(test1)
{
    const uint64_t K = 15;
    const uint64_t K1 = K + 1;

    StringFileFactory fac;
    map<Gossamer::position_type,uint64_t> k1mers;
    SmallBaseVector vec;
    {
        for (uint64_t i = 0; outs[i]; ++i)
        {
            seqToVec(outs[i], vec);
            for (uint64_t j = 0; j < vec.size() - K1 + 1; ++j)
            {
                k1mers[vec.kmer(K1, j)]++;
            }
        }
        Graph::Builder b(K, "x", fac, k1mers.size());
        for (map<Gossamer::position_type,uint64_t>::const_iterator i = k1mers.begin();
                i != k1mers.end(); ++i)
        {
            Gossamer::position_type k = i->first;
            uint16_t c = i->second;
            b.push_back(k, c);
        }
        b.end();
    }

    GraphPtr gPtr = Graph::open("x", fac);
    Graph& g(*gPtr);

    BOOST_CHECK_EQUAL(g.count(), 5);
    map<uint16_t,uint16_t> h;
    for (map<Gossamer::position_type,uint64_t>::const_iterator i = k1mers.begin();
            i != k1mers.end(); ++i)
    {
        Graph::Edge e(Gossamer::position_type(i->first));
        Graph::Node n(g.from(e));
        h[g.outDegree(n)]++;
    }
    BOOST_CHECK_EQUAL(h[0], 0);
    BOOST_CHECK_EQUAL(h[1], 1);
    BOOST_CHECK_EQUAL(h[2], 0);
    BOOST_CHECK_EQUAL(h[3], 0);
    BOOST_CHECK_EQUAL(h[4], 4);
}

BOOST_AUTO_TEST_CASE(test111BetterErrorMessage)
{
    StringFileFactory fac;
    try
    {
        GraphPtr g = Graph::open("snark/snark/snark", fac);
        BOOST_CHECK(false);
    }
    catch (Gossamer::error& exc)
    {
        string const* gn = get_error_info<Gossamer::open_graph_name_info>(exc);
        BOOST_CHECK(gn != NULL);
        BOOST_CHECK_EQUAL(*gn, "snark/snark/snark");
    }
}

BOOST_AUTO_TEST_CASE(test111Builder1)
{
    StringFileFactory fac;
    try
    {
        Graph::Builder b(12345678LL, "qux", fac, 1024ULL * 1024ULL);
        BOOST_CHECK(false);
    }
    catch (Gossamer::error& exc)
    {
        string const* msg = get_error_info<Gossamer::general_error_info>(exc);
        BOOST_CHECK(msg != NULL);
        BOOST_CHECK_EQUAL(*msg, "unable to build a graph with k=12345678");
    }
}

#include "testEnd.hh"
