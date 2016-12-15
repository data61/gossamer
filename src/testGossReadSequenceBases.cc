// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossReadSequenceBases.hh"

#include "FastaParser.hh"
#include "LineSource.hh"
#include "StringFileFactory.hh"

#include <iostream>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestGossReadSequenceBases
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;
    string read1 = 
        ">1\n"
        "NCTTTTGATGCAATGTCAAATTCTCCNCGTCATTCGCAACTGAATACAAGNGAATTTGGAAGGAGAATNTGA\n";
    string read2 =
        ">2\n"
        "TTTTATGTACTATTATCTTATTTCTAAATATTAACTATAGTATCCCCTGGCGTTAATACAGCTCTAGAAATC"
        "TTCCATTAAAAATAGCGAATTACTCGTATTCATCAAAGATATGGTAAGTGAAAAAGTTAGAATTCACACGCC\n";
        
    fac.addFile("reads.fa", read1 + read2);

        const char* ans1[] = {
            "CTTTTGATGCAATGT",
            "TTTTGATGCAATGTC",
            "TTTGATGCAATGTCA",
            "TTGATGCAATGTCAA",
            "TGATGCAATGTCAAA",
            "GATGCAATGTCAAAT",
            "ATGCAATGTCAAATT",
            "TGCAATGTCAAATTC",
            "GCAATGTCAAATTCT",
            "CAATGTCAAATTCTC",
            "AATGTCAAATTCTCC",
            "CGTCATTCGCAACTG",
            "GTCATTCGCAACTGA",
            "TCATTCGCAACTGAA",
            "CATTCGCAACTGAAT",
            "ATTCGCAACTGAATA",
            "TTCGCAACTGAATAC",
            "TCGCAACTGAATACA",
            "CGCAACTGAATACAA",
            "GCAACTGAATACAAG",
            "GAATTTGGAAGGAGA",
            "AATTTGGAAGGAGAA",
            "ATTTGGAAGGAGAAT",
            NULL
        };

    LineSourcePtr lineSrc
        = PlainLineSource::create(FileThunkIn(fac, "reads.fa"));
    GossReadParserPtr fastaParser = FastaParser::create(lineSrc);
    GossReadSequenceBases b(fastaParser);

    BOOST_CHECK(b.valid());
    BOOST_CHECK_EQUAL((*b).print(), read1);
    {
        GossRead::Iterator i(*b, 15);
        uint64_t j = 0;
        while (i.valid())
        {
            BOOST_CHECK(ans1[j]);
            BOOST_CHECK_EQUAL(Gossamer::kmerToString(15, i.kmer()), ans1[j]);
            ++i;
            ++j;
        }
        BOOST_CHECK_EQUAL(j, 23);
    }
    ++b;
    BOOST_CHECK(b.valid());
    BOOST_CHECK_EQUAL((*b).print(), read2);
    ++b;
    BOOST_CHECK(!b.valid());
}


#include "testEnd.hh"
