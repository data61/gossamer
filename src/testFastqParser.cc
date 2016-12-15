// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "StringFileFactory.hh"
#include "FastqParser.hh"
#include "LineSource.hh"
#include "GossReadSequenceBases.hh"
#include "ReadSequenceFileSequence.hh"
#include <memory>

#define GOSS_TEST_MODULE TestFastqParser
#include "testBegin.hh"

using namespace boost;
using namespace std;

GossReadSequencePtr
getFastqReader(StringFileFactory& fac, const char* filename)
{
    GossReadParserFactory fastqParserFac(FastqParser::create);
    GossReadSequenceFactoryPtr seqFac
        = std::make_shared<GossReadSequenceBasesFactory>();
    LineSourceFactory lineSrcFac(PlainLineSource::create);

    std::deque<GossReadSequence::Item> items;
    items.push_back(GossReadSequence::Item(filename, fastqParserFac, seqFac));
    return std::make_shared<ReadSequenceFileSequence>(items, fac, lineSrcFac);
}


BOOST_AUTO_TEST_CASE(empty_sequence)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
  "@FAKE0000\n"
  "+\n"
  "@FAKE0008\n"
  "+FAKE0008\n");

    try {
        GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
        while (rs->valid())
        {
            ++*rs;
        }
    }
    catch (Gossamer::error& err)
    {
        BOOST_FAIL(boost::diagnostic_information(err));
    }
}

BOOST_AUTO_TEST_CASE(bug_report_1)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
        "@No name\n"
        "CCCAATCTCCAATCACTCACCAACCTCTTGTCCTC\n"
        "+\n"
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
        "@No name\n"
        "GCTTAGCGTGTATACATGCATATAAAGGCATTAAA\n"
        "+\n"
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    try {
        GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
        while (rs->valid())
        {
            ++*rs;
        }
    }
    catch (Gossamer::error& err)
    {
        BOOST_FAIL(boost::diagnostic_information(err));
    }
}

BOOST_AUTO_TEST_CASE(misc_dna_as_solexa)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
  "@FAKE0007 Original version has lower case unambiguous DNA with PHRED scores from 0 to 40 inclusive (in that order)\n"
  "ACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTA\n"
  "+\n"
  ";;>@BCEFGHJKLMNOPQRSTUVWXYZ[\\]^_`abcdefgh\n"
  "@FAKE0008 Original version has mixed case unambiguous DNA with PHRED scores from 0 to 40 inclusive (in that order)\n"
  "gTcatAGcgTcatAGcgTcatAGcgTcatAGcgTcatAGcg\n"
  "+\n"
  ";;>@BCEFGHJKLMNOPQRSTUVWXYZ[\\]^_`abcdefgh\n"
  "@FAKE0009 Original version has lower case unambiguous DNA with PHRED scores from 0 to 40 inclusive (in that order)\n"
  "tcagtcagtcagtcagtcagtcagtcagtcagtcagtcagt\n"
  "+\n"
  ";;>@BCEFGHJKLMNOPQRSTUVWXYZ[\\]^_`abcdefgh\n"
  "@FAKE0010 Original version has mixed case ambiguous DNA and PHRED scores of 40, 30, 20, 10 (cycled)\n"
  "gatcrywsmkhbvdnGATCRYWSMKHBVDN\n"
  "+\n"
  "h^TJh^TJh^TJh^TJh^TJh^TJh^TJh^\n");

    try {
        GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
        while (rs->valid())
        {
            ++*rs;
        }
    }
    catch (Gossamer::error& err)
    {
        BOOST_FAIL(boost::diagnostic_information(err));
    }
}

BOOST_AUTO_TEST_CASE(wrapping_as_illumina)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
  "@SRR014849.50939 EIXKN4201BA2EC length=135\n"
  "GAAATTTCAGGGCCACCTTTTTTTTGATAGAATAATGGAGAAAATTAAAAGCTGTACATATACCAATGAACA\n"
  "ATAAATCAATACATAAAAAAGGAGAAGTTGGAACCGAAAGGGTTTGAATTCAAACCCTTTCGG\n"
  "+\n"
  "Zb^Ld`N\\[d`NaZ[aZc]UOKHDA[\\YT[_W[aZ\\aZ[Zd`SF_WeaUI[Y\\[[\\\\\\[\\Z\\aY`X[[aZ\\a\n"
  "Z\\d`OY[aY[[\\[[e`WPJC^UZ[`X\\[R]T_V_W[`[Ga\\I`\\H[[Q^TVa\\Ia\\Ic^LY\\S\n"
  "@SRR014849.110027 EIXKN4201APUB0 length=131\n"
  "CTTCAAATGATTCCGGGACTGTTGGAACCGAAAGGGTTTGAATTCAAACCCTTTTCGGTTCCAACTCGCCGT\n"
  "CCGAATAATCCGTTCAAAATCTTGGCCTGTCAAAACGACTTTACGACCAGAACGATCCG\n"
  "+\n"
  "\\aYY_[FY\\T`X^Vd`OY\\[[^U_V[R^T[_ZDc^La\\HYYO\\S[c^Ld`Nc_QAZaZaYaY`XZZ\\[aZZ[\n"
  "aZ[aZ[aZY`Z[`ZWeaVJ\\[aZaY`X[PY\\eaUG[\\[[d`OXTUZ[Q\\\\`W\\\\\\Y_W\\\n"
  "@SRR014849.203935 EIXKN4201B4HU6 length=144\n"
  "AACCCGTCCCATCAAAGATTTTGGTTGGAACCCGAAAGGGTTTTGAATTCAAACCCCTTTCGGTTCCAACTA\n"
  "TTCAATTGTTTAACTTTTTTTAAATTGATGGTCTGTTGGACCATTTGTAATAATCCCCATCGGAATTTCTTT\n"
  "+\n"
  "`Z_ZDVT^YB[[Xd`PZ\\d`RDaZaZ`ZaZ_ZDXd`Pd`Pd`RD[aZ`ZWd`Oc_RCd`P\\aZ`ZaZaZY\\Y\n"
  "aZYaY`XYd`O`X[e`WPJEAc^LaZS[YYN[Z\\Y`XWLT^U\\b]JW[[RZ\\SYc`RD[Z\\WLXM`\\HYa\\I\n");

    try {
        GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
        while (rs->valid())
        {
            ++*rs;
        }
    }
    catch (Gossamer::error& err)
    {
        BOOST_FAIL(boost::diagnostic_information(err));
    }

}

BOOST_AUTO_TEST_CASE(longreads_original_sanger)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
  "@FSRRS4401EQLIK [length=411] [gc=34.31] [flows=800] [phred_min=0] [phred_max=40] [trimmed_length=374]\n"
  "tcagTTTAATTTGGTGCTTCCTTTCAATTCCTTAGTTTAAACTTGGCACTGAAGTCTCGCATTTATAACTAGAGCCCGGA\n"
  "TTTTAGAGGCTAAAAAGTTTTCCAGATTTCAAAATTTATTTCGAAACTATTTTTCTGATTGTGATGTGACGGATTTCTAA\n"
  "ATTAAATCGAAATGATGTGTATTGAACTTAACAAGTGATTTTTATCAGATTTTGTCAATGAATAAATTTTAATTTAAATC\n"
  "TCTTTCTAACACTTTCATGATTAAAATCTAACAAAGCGCGACCAGTATGTGAGAAGAGCAAAAACAACAAAAAGTGCTAG\n"
  "CACTAAAGAAGGTTCGAACCCAACACATAACGTAAGAGTTACCGGGAAGAAAACCACTctgagactgccaaggcacacag\n"
  "ggggataggnn\n"
  "+FSRRS4401EQLIK [length=411] [gc=34.31] [flows=800] [phred_min=0] [phred_max=40] [trimmed_length=374]\n"
  "III?666?""?HHHIIIIIIIIIGGGIIIIIIIIIIIGGGHHHIIIIIIIIIIIIIIIIIIIIGGGIIIIIIIIIIHHHIII\n"
  "@@@@IIIIEIE111100----22?=8---:-------,,,,33---5:3,----:1BBEEEHIIIIIIIIIIIB?""?A122\n"
  "000...:?=024GIIIIIIIIIIIIIIIIIIECCHHB=//-,,21?""?<5-002=6FBB?:9<=11/4444//-//77?""?G\n"
  "EIEEHIACCIIIHHHIIIIIIICCCAIIIHHHHHHIIIIIIIIIIIIIIIIIIIIIIEE1//--822;----.777@EII\n"
  "IIII?""??IIIIIIIIIIIHHHIIIIIIIIIIIIIIIIIIII994227775555AE;IEEEEEIIIII?""?9755>@==:3,\n"
  ",,,,33336!!\n"
  "@FSRRS4401AOV6A [length=309] [gc=22.98] [flows=800] [phred_min=0] [phred_max=40] [trimmed_length=273]\n"
  "tcagTTTTCAAATTTTCCGAAATTTGCTGTTTGGTAGAAGGCAAATTATTTGATTGAATTTTGTATTTATTTAAAACAAT\n"
  "TTATTTTAAAATAATAATTTTCCATTGACTTTTTACATTTAATTGATTTTATTATGCATTTTATATTTGTTTTCTAAATA\n"
  "TTCGTTTGCAAACTCACGTTGAAATTGTATTAAACTCGAAATTAGAGTTTTTGAAATTAATTTTTATGTAGCATAATATT\n"
  "TTAAACATATTGGAATTTTATAAAACATTATATTTTTctgagactgccaaggcacacagggggataggn\n"
  "+FSRRS4401AOV6A [length=309] [gc=22.98] [flows=800] [phred_min=0] [phred_max=40] [trimmed_length=273]\n"
  "IIIICCCCI;;;CCCCIII?""??HHHIIIIHHHIIIIIIIIIIHHHIIIHHHIIIIIII@@@@IFICCCICAA;;;;ED?B\n"
  "@@D66445555<<<GII>>AAIIIIIIII;;;::III?""??CCCIII;;;;IFFIIIIICCCBIBIEEDC4444?4BBBE?\n"
  "EIIICHHII;;;HIIIIIIHH;;;HHIIIII;;;IIIIHHHIIIIII>>?""?>IEEBGG::1111/46FBFBB?=;=A?97\n"
  "771119:EAAADDBD7777=/111122DA@@B68;;;I8HHIIIII;;;;?>IECCCB/////;745=!\n");

    try {
        GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
        while (rs->valid())
        {
            ++*rs;
        }
    }
    catch (Gossamer::error& err)
    {
        BOOST_FAIL(boost::diagnostic_information(err));
    }

}

BOOST_AUTO_TEST_CASE(error_trunc_in_title)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
  "@SLXA-B3_649_FC8437_R1_1_1_610_79\n"
  "GATGTGCAATACCTTTGTAGAGGAA\n"
  "+SLXA-B3_649_FC8437_R1_1_1_610_79\n"
  "YYYYYYYYYYYYYYYYYYWYWYYSU\n"
  "@SLXA-B3_649_FC8437_R1_1_1_397_389\n"
  "GGTTTGAGAAAGAGAAATGAGATAA\n"
  "+SLXA-B3_649_FC8437_R1_1_1_397_389\n"
  "YYYYYYYYYWYYYYWWYYYWYWYWW\n"
  "@SLXA-B3_649_FC8437_R1_1_1_850_123\n"
  "GAGGGTGTTGATCATGATGATGGCG\n"
  "+SLXA-B3_649_FC8437_R1_1_1_850_123\n"
  "YYYYYYYYYYYYYWYYWYYSYYYSY\n"
  "@SLXA-B3_649_FC8437_R1_1_1_362_549\n"
  "GGAAACAAAGTTTTTCTCAACATAG\n"
  "+SLXA-B3_649_FC8437_R1_1_1_362_549\n"
  "YYYYYYYYYYYYYYYYYYWWWWYWY\n"
  "@SLXA-B3_649_FC8437_R1_1_1_\n");

    GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
    try {
        while (rs->valid())
        {
            ++*rs;
        }
        BOOST_FAIL("FASTQ parse should have failed; actually succeeded");
    }
    catch (Gossamer::error& err)
    {
    }
}


BOOST_AUTO_TEST_CASE(error_long_qual)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
        "@SLXA-B3_649_FC8437_R1_1_1_610_79\n"
        "GATGTGCAATACCTTTGTAGAGGAA\n"
        "+SLXA-B3_649_FC8437_R1_1_1_610_79\n"
        "YYYYYYYYYYYYYYYYYYWYWYYSU\n"
        "@SLXA-B3_649_FC8437_R1_1_1_397_389\n"
        "GGTTTGAGAAAGAGAAATGAGATAA\n"
        "+SLXA-B3_649_FC8437_R1_1_1_397_389\n"
        "YYYYYYYYYWYYYYWWYYYWYWYWW\n"
        "@SLXA-B3_649_FC8437_R1_1_1_850_123\n"
        "GAGGGTGTTGATCATGATGATGGCG\n"
        "+SLXA-B3_649_FC8437_R1_1_1_850_123\n"
        "YYYYYYYYYYYYYWYYWYYSYYYSY\n"
        "@SLXA-B3_649_FC8437_R1_1_1_362_549\n"
        "GGAAACAAAGTTTTTCTCAACATAG\n"
        "+SLXA-B3_649_FC8437_R1_1_1_362_549\n"
        "YYYYYYYYYYYYYYYYYYWWWWYWYY\n"
        "@SLXA-B3_649_FC8437_R1_1_1_183_714\n"
        "GTATTATTTAATGGCATACACTCAA\n"
        "+SLXA-B3_649_FC8437_R1_1_1_183_714\n"
        "YYYYYYYYYYWYYYYWYWWUWWWQQ\n");

    GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
    try {
        while (rs->valid())
        {
            ++*rs;
        }
        BOOST_FAIL("FASTQ parse should have failed; actually succeeded");
    }
    catch (Gossamer::error& err)
    {
    }
}

BOOST_AUTO_TEST_CASE(error_short_qual)
{
    StringFileFactory fac;
    fac.addFile("x.fq",
        "@SLXA-B3_649_FC8437_R1_1_1_610_79\n"
        "GATGTGCAATACCTTTGTAGAGGAA\n"
        "+SLXA-B3_649_FC8437_R1_1_1_610_79\n"
        "YYYYYYYYYYYYYYYYYYWYWYYSU\n"
        "@SLXA-B3_649_FC8437_R1_1_1_397_389\n"
        "GGTTTGAGAAAGAGAAATGAGATAA\n"
        "+SLXA-B3_649_FC8437_R1_1_1_397_389\n"
        "YYYYYYYYYWYYYYWWYYYWYWYWW\n"
        "@SLXA-B3_649_FC8437_R1_1_1_850_123\n"
        "GAGGGTGTTGATCATGATGATGGCG\n"
        "+SLXA-B3_649_FC8437_R1_1_1_850_123\n"
        "YYYYYYYYYYYYYWYYWYYSYYYS\n"
        "@SLXA-B3_649_FC8437_R1_1_1_362_549\n"
        "GGAAACAAAGTTTTTCTCAACATAG\n"
        "+SLXA-B3_649_FC8437_R1_1_1_362_549\n"
        "YYYYYYYYYYYYYYYYYYWWWWYWY\n"
        "@SLXA-B3_649_FC8437_R1_1_1_183_714\n"
        "GTATTATTTAATGGCATACACTCAA\n"
        "+SLXA-B3_649_FC8437_R1_1_1_183_714\n"
        "YYYYYYYYYYWYYYYWYWWUWWWQQ\n");

    GossReadSequencePtr rs = getFastqReader(fac, "x.fq");
    try {
        while (rs->valid())
        {
            ++*rs;
        }
        BOOST_FAIL("FASTQ parse should have failed; actually succeeded");
    }
    catch (Gossamer::error& err)
    {
    }
}

#include "testEnd.hh"

