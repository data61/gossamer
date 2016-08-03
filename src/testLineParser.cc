#include "GossReadSequenceBases.hh"
#include "LineParser.hh"
#include "LineSource.hh"
#include "ReadSequenceFileSequence.hh"
#include "StringFileFactory.hh"

#define GOSS_TEST_MODULE TestLineParser
#include "testBegin.hh"

using namespace boost;
using namespace std;

GossReadSequencePtr
getLinesReader(StringFileFactory& fac, const char* filename)
{
    GossReadParserFactory lineParserFac(LineParser::create);
    GossReadSequenceFactoryPtr seqFac
        = std::make_shared<GossReadSequenceBasesFactory>();
    LineSourceFactory lineSrcFac(PlainLineSource::create);

    std::deque<GossReadSequence::Item> items;
    items.push_back(GossReadSequence::Item(filename, lineParserFac, seqFac));
    return std::make_shared<ReadSequenceFileSequence>(items, fac, lineSrcFac);
}

BOOST_AUTO_TEST_CASE(test1)
{
    StringFileFactory fac;

    string reads[] = {"AAAAAAAAAAAAAAAAAAAAAAAAA\n",
                      "AAAAAAAAAAAAAAAAAAAACGCCG\n",
                      "AAAAAAAAAAAAAAAAAAAATTTCG\n",
                      "AAAAAAAAAAAAAAAAAACGTTCTG\n"};

    {
        std::string rs;
        for (uint64_t i = 0; i < sizeof(reads) / sizeof(string); ++i)
        {
            rs += reads[i];
        }
        fac.addFile("test.txt", rs);
    }

    GossReadSequencePtr seqPtr = getLinesReader(fac, "test.txt");
    GossReadSequence& seq = *seqPtr;
    for (uint64_t i = 0; seq.valid(); ++seq, ++i)
    {
        BOOST_CHECK_EQUAL((*seq).print(), reads[i]);
    }
}

#include "testEnd.hh"
