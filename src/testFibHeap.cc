// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing FibHeap.
 *
 */

#define TEST_FIB_HEAP
#include "FibHeap.hh"

#include <vector>
#include <deque>
#include <queue>
#include <string>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestFibHeap
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(testBasics)
{
    FibHeap<double,string> h;
    BOOST_CHECK(h.empty());
    BOOST_CHECK(h.checkHeapInvariant());
    h.insert(0.4, "hello");
    BOOST_CHECK(!h.empty());
    BOOST_CHECK(h.checkHeapInvariant());
    h.clear();
    BOOST_CHECK(h.empty());
    BOOST_CHECK(h.checkHeapInvariant());
    h.insert(0.4, "hello");
    BOOST_CHECK(!h.empty());
    BOOST_CHECK(h.checkHeapInvariant());
    h.removeMinimum();
    BOOST_CHECK(h.empty());
    BOOST_CHECK(h.checkHeapInvariant());
}


template<typename K, typename V>
struct BasicTest
{
    FibHeap<K,V> h;
    deque<pair<K,V> > values;

    typename FibHeap<K,V>::iterator insert(const K& pK, const V& pV)
    {
        typename FibHeap<K,V>::iterator it = h.insert(pK, pV);
        values.push_back(pair<K,V>(pK, pV));
        BOOST_CHECK(!h.empty());
        BOOST_CHECK(h.checkHeapInvariant());
        return it;
    }

    void doTest()
    {
        sort(values.begin(), values.end());
        while (!values.empty())
        {
            BOOST_CHECK(!h.empty());
            FibHeap<uint64_t,string>::const_iterator min = h.minimum();
            BOOST_CHECK_EQUAL(min->mKey, values.front().first);
            BOOST_CHECK_EQUAL(min->mValue, values.front().second);
            h.removeMinimum();
            BOOST_CHECK(h.checkHeapInvariant());
            values.pop_front();
        }

        BOOST_CHECK(h.empty());
    }
};

BOOST_AUTO_TEST_CASE(testPQueue)
{
    BasicTest<uint64_t,string> tester;
    tester.insert(4, "a");
    tester.insert(2, "b");
    tester.insert(7, "c");
    tester.insert(5, "d");
    tester.insert(1, "e");
    tester.insert(8, "f");
    tester.doTest();
}

BOOST_AUTO_TEST_CASE(testDecreaseKey)
{
    FibHeap<uint64_t,string> h;

    FibHeap<uint64_t,string>::iterator a = h.insert(400, "a");
    FibHeap<uint64_t,string>::iterator b = h.insert(200, "b");
    FibHeap<uint64_t,string>::iterator c = h.insert(70, "c");
    FibHeap<uint64_t,string>::iterator d = h.insert(50, "d");
    FibHeap<uint64_t,string>::iterator e = h.insert(10, "e");
    FibHeap<uint64_t,string>::iterator f = h.insert(80, "f");
    BOOST_CHECK(h.checkHeapInvariant());
    // (10,e) (50,d) (70,c) (80,f) (200,b) (400,a)
    BOOST_CHECK_EQUAL(h.minimum(), e);
    h.decreaseKey(d, 5);
    // (5,d) (10,e) (70,c) (80,f) (200,b) (400,a)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(d->mKey, 5);
    BOOST_CHECK_EQUAL(h.minimum(), d);
    h.removeMinimum();
    // (10,e) (70,c) (80,f) (200,b) (400,a)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(h.minimum(), e);
    h.decreaseKey(a, 7);
    // (7,a) (10,e) (70,c) (80,f) (200,b)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(a->mKey, 7);
    BOOST_CHECK_EQUAL(h.minimum(), a);
    h.removeMinimum();
    BOOST_CHECK(h.checkHeapInvariant());
    // (10,e) (70,c) (80,f) (200,b)
    BOOST_CHECK_EQUAL(h.minimum(), e);
    h.decreaseKey(f, 20);
    // (10,e) (20, f) (70,c) (200,b)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(h.minimum(), e);
    h.remove(c);
    // (10,e) (20, f) (200,b)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(h.minimum(), e);
    h.removeMinimum();
    // (20, f) (200,b)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(h.minimum(), f);
    h.removeMinimum();
    // (200,b)
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK_EQUAL(h.minimum(), b);
    h.removeMinimum();
    // empty
    BOOST_CHECK(h.checkHeapInvariant());
    BOOST_CHECK(h.empty());
}

BOOST_AUTO_TEST_CASE(testEqualKeyBug)
{
    FibHeap<int,string> h;

    h.insert(0, "");
    h.insert(10, "a");
    h.insert(20, "b");
    h.insert(20, "c");
    h.insert(30, "d");

    h.removeMinimum();
    BOOST_CHECK(h.checkHeapInvariant());

    h.insert(15, "e");
    h.insert(50, "f");
    BOOST_CHECK(h.checkHeapInvariant());

    h.removeMinimum();
    BOOST_CHECK(h.checkHeapInvariant());
}

BOOST_AUTO_TEST_CASE(testPromoteRootChildrenBug)
{
    FibHeap<int,string> h;

    h.insert(0, "");
    h.insert(30, "C");
    h.insert(60, "F");
    h.insert(20, "B");
    h.insert(50, "E");
    h.removeMinimum();

    FibHeap<int,string>::iterator iic = h.insert(30, "_C");
    FibHeap<int,string>::iterator iif = h.insert(60, "_F");
    FibHeap<int,string>::iterator iib = h.insert(20, "_B");
    FibHeap<int,string>::iterator iie = h.insert(50, "_E");
    h.insert(10, "A");
    h.remove(iic);
    h.remove(iif);
    h.remove(iib);
    h.remove(iie);

    h.insert(40, "D");

    h.removeMinimum();
    BOOST_CHECK(h.checkHeapInvariant());
}

#include "testEnd.hh"
