// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "VByteCodec.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>
#include <vector>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestVByteCodec
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test1a)
{
    uint64_t x = 0;
    vector<uint8_t> bytes;

    VByteCodec::encode(x, bytes);
    BOOST_CHECK_EQUAL(bytes.size(), 1);
    BOOST_CHECK_EQUAL(bytes[0], 0);

    vector<uint8_t>::iterator itr(bytes.begin());
    uint64_t y = VByteCodec::decode(itr, bytes.end());
    BOOST_CHECK_EQUAL(x, y);
}

BOOST_AUTO_TEST_CASE(test1b)
{
    uint64_t x = 1;
    vector<uint8_t> bytes;

    VByteCodec::encode(x, bytes);
    BOOST_CHECK_EQUAL(bytes.size(), 1);
    BOOST_CHECK_EQUAL(bytes[0], 1);

    vector<uint8_t>::iterator itr(bytes.begin());
    uint64_t y = VByteCodec::decode(itr, bytes.end());
    BOOST_CHECK_EQUAL(x, y);
}

BOOST_AUTO_TEST_CASE(test1c)
{
    uint64_t x = 128;
    vector<uint8_t> bytes;

    VByteCodec::encode(x, bytes);
    BOOST_CHECK_EQUAL(bytes.size(), 2);
    BOOST_CHECK_EQUAL(bytes[0], 0x80);
    BOOST_CHECK_EQUAL(bytes[1], 0x80);

    vector<uint8_t>::iterator itr(bytes.begin());
    uint64_t y = VByteCodec::decode(itr, bytes.end());
    BOOST_CHECK_EQUAL(x, y);
}

        
BOOST_AUTO_TEST_CASE(test1d_alt)
{           
    for (uint64_t i = 0; i < 64; ++i)
    {
        uint64_t x = 1ull << i;
        vector<uint8_t> bytes;
            
        VByteCodec::encode(x, bytes);
        vector<uint8_t>::iterator itr(bytes.begin());
        uint64_t y = VByteCodec::decode(itr, bytes.end());
        BOOST_CHECK_EQUAL(x, y);
    }
}               
             

BOOST_AUTO_TEST_CASE(test2)
{
    for (uint64_t x = 0; x < 1024 * 1024; ++x)
    {
        vector<uint8_t> bytes;

        VByteCodec::encode(x, bytes);
        vector<uint8_t>::iterator itr(bytes.begin());
        uint64_t y = VByteCodec::decode(itr, bytes.end());
        BOOST_CHECK_EQUAL(x, y);
    }
}

BOOST_AUTO_TEST_CASE(tryme)
{
    vector<uint8_t> bytes;
    VByteCodec::encode(1051466, bytes);
    VByteCodec::encode(1089606, bytes);
    VByteCodec::encode(1082820, bytes);
    VByteCodec::encode(1070359, bytes);
    VByteCodec::encode(1097879, bytes);
    VByteCodec::encode(3, bytes);
    VByteCodec::encode(30, bytes);
    VByteCodec::encode(226534, bytes);
    VByteCodec::encode(503445, bytes);
    VByteCodec::encode(19, bytes);
    VByteCodec::encode(21778, bytes);
    VByteCodec::encode(1101788, bytes);
    VByteCodec::encode(0, bytes);
}

#include "testEnd.hh"
