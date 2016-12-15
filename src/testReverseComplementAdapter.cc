// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "ReverseComplementAdapter.hh"

#include "FastaParser.hh"
#include "GossReadSequenceBases.hh"
#include "LineSource.hh"
#include "ReadSequenceFileSequence.hh"
#include "StringFileFactory.hh"

#include <iostream>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestReverseComplementAdapter
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    fac.addFile("x.fa",
        ">1\n"
        "TTTT\n"
        ">2\n"
        "TTTTATGTACTATTATCTTATTTCTAAATATTAACTATAGTATCCCCTGGCGTTAATACAGCTCTAGAAATC\n");

    GossReadParserFactory fastaParserFac(FastaParser::create);
    GossReadSequenceFactoryPtr seqFac
        = std::make_shared<GossReadSequenceBasesFactory>();
    LineSourceFactory lineSrcFac(PlainLineSource::create);

    std::deque<GossReadSequence::Item> items;
    items.push_back(GossReadSequence::Item("x.fa", fastaParserFac, seqFac));
    ReadSequenceFileSequence seq(items, fac, lineSrcFac);

    ReverseComplementAdapter adp(seq, 15);

    uint64_t j = 0;
    while (adp.valid())
    {
        ++adp;
        ++j;
    }
    BOOST_CHECK_EQUAL(j, 116);
}

#include "testEnd.hh"

