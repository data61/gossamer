// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "EdgeAndCount.hh"
#include "StringFileFactory.hh"
#include "GossamerException.hh"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <random>

using namespace boost;
using namespace std;
using namespace Gossamer;

#define GOSS_TEST_MODULE TestEdgeAndCount
#include "testBegin.hh"

namespace {

    position_type parse(const string& pKmer)
    {
        const uint64_t len = pKmer.size();
        position_type e(0);
        for (uint64_t i = 0; i < len; ++i)
        {
            e <<= 2;
            switch (pKmer[i])
            {
                case 'C':
                    e |= 1;
                    break;
                case 'G':
                    e |= 2;
                    break;
                case 'T':
                    e |= 3;
                    break;
                default:
                    break;
            }
        }
        return e;
    }

}

BOOST_AUTO_TEST_CASE(test1)
{
    typedef Gossamer::EdgeAndCount Item;
    vector<Item> items;

    items.push_back(Item(parse("AAAAAAAAAAAAAAAAAAAAAACTTTTTTTTTTTACGTGAAGGGAACGTTCATAGG"), 1));
    items.push_back(Item(parse("AAAAAAAAAAAAAAAAAAAAAAGAAAAAAAAAAAAAAGAAAAGAAAAAAAAAGAAA"), 1));

    StringFileFactory fac;
    {
        FileFactory::OutHolderPtr outP(fac.out("tmp"));
        ostream& out(**outP);

        position_type prev(0ULL);
        for (uint64_t i = 0; i < items.size(); ++i)
        {
            const Item& item(items[i]);
            EdgeAndCountCodec::encode(out, prev, item);
            prev = item.first;
        }
    }

    {
        FileFactory::InHolderPtr inP(fac.in("tmp"));
        istream& in(**inP);
        EdgeAndCount item;

        for (uint64_t i = 0; i < items.size(); ++i)
        {
            EdgeAndCountCodec::decode(in, item);
            BOOST_ASSERT(item.first == items[i].first);
            BOOST_ASSERT(item.second == items[i].second);
        }
 
    }
}

#include "testEnd.hh"
