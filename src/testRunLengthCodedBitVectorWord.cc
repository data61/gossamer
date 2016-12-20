// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//

#include "RunLengthCodedBitVectorWord.hh"
#include "GammaCodec.hh"
#include "DeltaCodec.hh"

#include <boost/dynamic_bitset.hpp>
#include <random>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestRunLengthCodedBitVectorWord
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(test3a)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t p = 0;
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, p, true);
    BOOST_CHECK_EQUAL(w0, 3);
    BOOST_CHECK_EQUAL(w1, 0);

    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 1), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::select(w0, 0), 0);
}

BOOST_AUTO_TEST_CASE(test3b)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t p = 0;
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, p, false);
    BOOST_CHECK_EQUAL(w0, 2);
    BOOST_CHECK_EQUAL(w1, 0);

    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 1), 0);
}

BOOST_AUTO_TEST_CASE(test3c)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t p = 0;
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, p, false);
    BOOST_CHECK_EQUAL(w0, 2);
    BOOST_CHECK_EQUAL(w1, 0);
    p = 1;
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, p, false);
    BOOST_CHECK_EQUAL(w0, 4);
    BOOST_CHECK_EQUAL(w1, 0);

    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 2);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 1), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::rank(w0, 2), 0);
}

BOOST_AUTO_TEST_CASE(test3d)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, 0, false);
    BOOST_CHECK_EQUAL(w0, 2);
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0),  1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 0);
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, 1, false);
    BOOST_CHECK_EQUAL(w0, 4);
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0),  2);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 0);
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, 2, true);
    BOOST_CHECK_EQUAL(w0, 20);
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 3);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 1);
}

BOOST_AUTO_TEST_CASE(test4a)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t j = 0;
    uint64_t n = 1024;
    for (uint64_t i = 0; i < n; ++i)
    {
        w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, i + j, false);
        BOOST_CHECK_EQUAL(w1, 0);
    }
    j += n;
    for (uint64_t i = 0; i < n; ++i)
    {
        w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, i + j, true);
        BOOST_CHECK_EQUAL(w1, 0);
    }
    j += n;
    for (uint64_t i = 0; i < n; ++i)
    {
        w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, i + j, false);
        BOOST_CHECK_EQUAL(w1, 0);
    }
    j += n;
    w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, 0 + j, true);
    BOOST_CHECK(w1 != 0);
}

BOOST_AUTO_TEST_CASE(test4b)
{
    dynamic_bitset<uint64_t> ns;
    std::vector<uint64_t> ws;
    bool s = true;
    uint64_t xs[] = { 205, 774, 1904, 217, 0};
    uint64_t w0 = 0;
    uint64_t k = 0;
    uint64_t w1 = 0;
    for (uint64_t i = 0; xs[i]; ++i, s = !s)
    {
        uint64_t v = xs[i];
        for (uint64_t j = 0; j < v; ++j)
        {
            ns.push_back(s);
            w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, k, s);
            if (w1)
            {
                ws.push_back(w0);
                w0 = w1;
                w1 = 0;
                k = RunLengthCodedBitVectorWord<GammaCodec>::size(w0);
            }
            else
            {
                ++k;
            }
        }
        uint64_t z = 0;
        uint64_t c = 0;
        for (uint64_t i = 0; i < ws.size(); ++i)
        {
            z += RunLengthCodedBitVectorWord<GammaCodec>::size(ws[i]);
            c += RunLengthCodedBitVectorWord<GammaCodec>::count(ws[i]);
        }
        z += RunLengthCodedBitVectorWord<GammaCodec>::size(w0);
        c += RunLengthCodedBitVectorWord<GammaCodec>::count(w0);
        BOOST_CHECK_EQUAL(z, ns.size());
        BOOST_CHECK_EQUAL(c, ns.count());
    }
}

#if 1
BOOST_AUTO_TEST_CASE(test4)
{
    static const double p = 1.0 / 1024.0;
    mt19937 rng(17);
    exponential_distribution<> dist(p);


    const uint64_t N = 40;
    dynamic_bitset<uint64_t> ns;
    std::vector<uint64_t> ws;
    bool s = dist(rng) * p > 0.5;
    uint64_t w0 = 0;
    uint64_t k = 0;
    uint64_t w1 = 0;
    for (uint64_t i = 0; i < N; ++i, s = !s)
    {
        uint64_t v = dist(rng);
        for (uint64_t j = 0; j < v; ++j)
        {
            ns.push_back(s);
            w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, k, s);
            if (w1)
            {
                ws.push_back(w0);
                w0 = w1;
                w1 = 0;
                k = RunLengthCodedBitVectorWord<GammaCodec>::size(w0);
            }
            else
            {
                ++k;
            }
        }
        uint64_t z = 0;
        uint64_t c = 0;
        for (uint64_t i = 0; i < ws.size(); ++i)
        {
            z += RunLengthCodedBitVectorWord<GammaCodec>::size(ws[i]);
            c += RunLengthCodedBitVectorWord<GammaCodec>::count(ws[i]);
        }
        z += RunLengthCodedBitVectorWord<GammaCodec>::size(w0);
        c += RunLengthCodedBitVectorWord<GammaCodec>::count(w0);
        BOOST_CHECK_EQUAL(z, ns.size());
        BOOST_CHECK_EQUAL(c, ns.count());
    }
    if (w0)
    {
        ws.push_back(w0);
    }
    uint64_t z = 0;
    uint64_t c = 0;
    for (uint64_t i = 0; i < ws.size(); ++i)
    {
        z += RunLengthCodedBitVectorWord<GammaCodec>::size(ws[i]);
        c += RunLengthCodedBitVectorWord<GammaCodec>::count(ws[i]);
    }
    BOOST_CHECK_EQUAL(z, ns.size());
    BOOST_CHECK_EQUAL(c, ns.count());
}
#endif

BOOST_AUTO_TEST_CASE(test5)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t j = 0;
    uint64_t n = 1024;
    for (uint64_t i = 0; i < n; ++i)
    {
        w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, i + j, false);
    }
    j += n;
    for (uint64_t i = 0; i < n; ++i)
    {
        w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, i + j, true);
    }
    j += n;
    for (uint64_t i = 0; i < n; ++i)
    {
        w1 = RunLengthCodedBitVectorWord<GammaCodec>::insert(w0, i + j, false);
    }
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 3072);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 1024);

    RunLengthCodedBitVectorWord<GammaCodec>::erase(w0, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 3071);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 1024);

    RunLengthCodedBitVectorWord<GammaCodec>::erase(w0, 155);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 3070);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 1024);

    RunLengthCodedBitVectorWord<GammaCodec>::erase(w0, 1440);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::size(w0), 3069);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<GammaCodec>::count(w0), 1023);
}

BOOST_AUTO_TEST_CASE(test6a)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t p = 0;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, p, true);
    BOOST_CHECK_EQUAL(w1, 0);

    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(w0), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(w0), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 1), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::select(w0, 0), 0);
}

BOOST_AUTO_TEST_CASE(test6b)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t p = 0;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, p, false);
    BOOST_CHECK_EQUAL(w1, 0);

    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(w0), 1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(w0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 1), 0);
}

BOOST_AUTO_TEST_CASE(test6c)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    uint64_t p = 0;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, p, false);
    BOOST_CHECK_EQUAL(w1, 0);
    p = 1;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, p, false);
    BOOST_CHECK_EQUAL(w1, 0);

    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(w0), 2);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(w0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 0), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 1), 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::rank(w0, 2), 0);
}

BOOST_AUTO_TEST_CASE(test6d)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, 0, false);
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(w0),  1);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(w0), 0);
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, 1, false);
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(w0),  2);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(w0), 0);
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::insert(w0, 2, true);
    BOOST_CHECK_EQUAL(w1, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(w0), 3);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(w0), 1);
}

BOOST_AUTO_TEST_CASE(testAppend1)
{
    uint64_t w0 = 0;
    uint64_t w1 = 0;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::append(w0, 1, true);
    BOOST_CHECK_EQUAL(w0, 3);
    BOOST_CHECK_EQUAL(w1, 0);

    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::append(w0, 1, true);
    BOOST_CHECK_EQUAL(w0, 5);
    BOOST_CHECK_EQUAL(w1, 0);

    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::append(w0, 1, true);
    BOOST_CHECK_EQUAL(w0, 21);
    BOOST_CHECK_EQUAL(w1, 0);

    w0 = 0;
    w1 = RunLengthCodedBitVectorWord<DeltaCodec>::append(w0, 3, true);
    BOOST_CHECK_EQUAL(w0, 21);
    BOOST_CHECK_EQUAL(w1, 0);
}

BOOST_AUTO_TEST_CASE(testUnion1)
{
    uint64_t l = 0;
    uint64_t r = 0;
    uint64_t w = 0;
    w = RunLengthCodedBitVectorWord<DeltaCodec>::merge(l, r);
    BOOST_CHECK_EQUAL(l, 0);
    BOOST_CHECK_EQUAL(r, 0);
    BOOST_CHECK_EQUAL(w, 0);
}

BOOST_AUTO_TEST_CASE(testUnion2)
{
    uint64_t l = 0;
    uint64_t r = 0;
    uint64_t w = 0;
    RunLengthCodedBitVectorWord<DeltaCodec>::append(l, 10, true);
    BOOST_CHECK_EQUAL(w, 0);
    RunLengthCodedBitVectorWord<DeltaCodec>::append(r, 10, false);
    BOOST_CHECK_EQUAL(w, 0);
    RunLengthCodedBitVectorWord<DeltaCodec>::append(r, 10, true);
    BOOST_CHECK_EQUAL(w, 0);
    w = RunLengthCodedBitVectorWord<DeltaCodec>::merge(l, r);
    BOOST_CHECK_EQUAL(w, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(l), 20);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(l), 20);
}

BOOST_AUTO_TEST_CASE(testUnion3)
{
    uint64_t l = 0;
    uint64_t r = 0;
    uint64_t w = 0;
    RunLengthCodedBitVectorWord<DeltaCodec>::append(l, 10, true);
    BOOST_CHECK_EQUAL(w, 0);
    RunLengthCodedBitVectorWord<DeltaCodec>::append(r, 20, false);
    BOOST_CHECK_EQUAL(w, 0);
    RunLengthCodedBitVectorWord<DeltaCodec>::append(r, 10, true);
    BOOST_CHECK_EQUAL(w, 0);
    w = RunLengthCodedBitVectorWord<DeltaCodec>::merge(l, r);
    BOOST_CHECK_EQUAL(w, 0);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::size(l), 30);
    BOOST_CHECK_EQUAL(RunLengthCodedBitVectorWord<DeltaCodec>::count(l), 20);
}

#include "testEnd.hh"
