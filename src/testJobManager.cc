// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "JobManager.hh"

#define GOSS_TEST_MODULE TestJobManager
#include "testBegin.hh"

using namespace boost;
using namespace std;

static int x = 0;
mutex mtx;

static void foo()
{
    unique_lock<mutex> lk(mtx);
    x = 1;
}

static void bar()
{
    unique_lock<mutex> lk(mtx);
    BOOST_CHECK_EQUAL(x, 1);
    x += 1;
}

static void baz()
{
    unique_lock<mutex> lk(mtx);
    x += 1;
}

BOOST_AUTO_TEST_CASE(test1)
{
    JobManager m(4);

    x = 0;
    m.enqueue(foo);
    m.wait();
    BOOST_CHECK_EQUAL(x, 1);
}

BOOST_AUTO_TEST_CASE(test2)
{
    JobManager m(4);

    x = 0;
    JobManager::Token t = m.enqueue(foo);
    m.enqueue(bar, t);
    m.wait();
    BOOST_CHECK_EQUAL(x, 2);
}

BOOST_AUTO_TEST_CASE(test3)
{
    JobManager m(4);

    x = 0;
    JobManager::Token t = m.enqueue(foo);
    m.enqueue(baz, t);
    m.enqueue(baz, t);
    m.wait();
    BOOST_CHECK_EQUAL(x, 3);
}

BOOST_AUTO_TEST_CASE(test4)
{
    JobManager m(4);

    x = 0;
    JobManager::Token t = m.enqueue(foo);
    JobManager::Token u = m.enqueue(baz, t);
    m.enqueue(baz, u);
    m.wait();
    BOOST_CHECK_EQUAL(x, 3);
}

#include "testEnd.hh"
