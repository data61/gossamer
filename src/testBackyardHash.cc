// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing BackyardHash.
 *
 */

#include "BackyardHash.hh"
#include <vector>
#include <random>
#include "ThreadGroup.hh"
#include <thread>

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestBackyardHash
#include "testBegin.hh"

/*
BOOST_AUTO_TEST_CASE(testMutant)
{
    Gossamer::edge_type v(42377831217986774ULL);
    BackyardHash x(16, 56, 1ULL << 16);
    x.insert(v);
    BOOST_CHECK_EQUAL(x.count(v), 1);
    x.insert(v);
    BOOST_CHECK_EQUAL(x.count(v), 2);
}

BOOST_AUTO_TEST_CASE(test1)
{
    uint64_t N = 65536;
    BackyardHash x(16, 32, 1ULL << 16);
    for (uint64_t i = 0; i < N; ++i)
    {
        x.insert(Gossamer::edge_type(i));
    }

    for (uint64_t i = 0; i < N; ++i)
    {
        BOOST_CHECK_EQUAL(x.count(Gossamer::edge_type(i)), 1);
    }
}

BOOST_AUTO_TEST_CASE(test2)
{
    BackyardHash x(16, 32, 1ULL << 16);
    x.insert(Gossamer::edge_type(123456));
    BOOST_CHECK_EQUAL(x.count(Gossamer::edge_type(123456)), 1);
    x.insert(Gossamer::edge_type(123456));
    BOOST_CHECK_EQUAL(x.count(Gossamer::edge_type(123456)), 2);
}

BOOST_AUTO_TEST_CASE(test3)
{
    BackyardHash x(16, 32, 1ULL << 16);
    for (uint64_t i = 1; i < 1024 * 1024; ++i)
    {
        x.insert(Gossamer::edge_type(123456));
        BOOST_CHECK_EQUAL(x.count(Gossamer::edge_type(123456)), i);
    }
}

BOOST_AUTO_TEST_CASE(test4)
{
    std::mt19937 rng(19);
    std::uniform_int_distribution<> dist;

    BackyardHash x(16, 32, 1ULL << 16);
    static const uint64_t N = 1024 * 1024;
    map<Gossamer::edge_type,uint64_t> m;
    for (uint64_t i = 1; i < N; ++i)
    {
        Gossamer::edge_type v(dist(rng));
        x.insert(v);
        m[v]++;
    }

    for (map<Gossamer::edge_type,uint64_t>::const_iterator i = m.begin(); i != m.end(); ++i)
    {
        if (x.count(i->first) != i->second)
        {
            cerr << i->first << '\t' << i->second << '\t' << x.count(i->first) << endl;
        }
        BOOST_CHECK_EQUAL(x.count(i->first), i->second);
    }
}

BOOST_AUTO_TEST_CASE(test5)
{
    std::mt19937 rng(19);
    std::uniform_int_distribution<> dist;

    BackyardHash x(16, 32, 1ULL << 16);
    static const uint64_t N = 1024 * 1024;
    map<Gossamer::edge_type,uint64_t> m;
    for (uint64_t i = 1; i < N; ++i)
    {
        Gossamer::edge_type v(dist(rng));
        x.insert(v);
        m[v]++;
    }

    vector<uint32_t> perm;

    x.sort(perm, 1);

    uint64_t i = 0;
    for (map<Gossamer::edge_type,uint64_t>::const_iterator j = m.begin(); j != m.end(); ++j, ++i)
    {
        BOOST_CHECK_EQUAL(j->first, x[perm[i]].first);
        BOOST_CHECK_EQUAL(j->second, x[perm[i]].second);
    }
}
*/

BOOST_AUTO_TEST_CASE(test6)
{
    const uint64_t M = (1ULL << 40) - 1;
    std::mt19937 rng(17);
    std::uniform_int_distribution<uint64_t> d(0, M);;

    BackyardHash h(16, 40, (1ULL << 16) + (1ULL << 15));

    for (uint64_t i = 0; true; ++i)
    {
        h.insert(BackyardHash::value_type(d(rng)));
        if (h.spills() > 0)
        {
            cout << i << endl;
            break;
        }
    }

    vector<uint32_t> perm;

    h.sort(perm, 1);

    pair<Gossamer::edge_type,uint64_t> prev = h[perm[0]];
    for (uint32_t i = 1; i < perm.size(); ++i)
    {
        pair<Gossamer::edge_type,uint64_t> itm = h[perm[i]];
        if (itm.first < prev.first)
        {
            cerr << "edge: " << itm.first.value() << ", count: " << itm.second << '\n';
        }
        BOOST_CHECK(itm.first >= prev.first);
        if (itm.first == prev.first)
        {
            prev.second += itm.second;
            continue;
        }
        prev = itm;
    }
}

#if 0
class Inserter
{
public:
    void operator()()
    {
        const uint64_t M = (1ULL << 40) - 1;
        const uint64_t N = 50;
        std::mt19937 rng(mSeed);
        std::uniform_int_distribution<uint64_t> d(0, M);;
        vector<uint64_t> xs;
        for (uint64_t i = 0; i < N; ++i)
        {
            xs.push_back(dist(rng));
            mHash.insert(BackyardHash::value_type(xs.back()));
            BOOST_CHECK(mHash.count(BackyardHash::value_type(xs.back())) > 0);
            if (xs.size() > 997)
            {
                unique_lock<mutex> lk(mMutex);
                for (uint64_t j = 0; j < xs.size(); ++j)
                {
                    mMap[xs[j]]++;
                }
                xs.clear();
            }
        }
        unique_lock<mutex> lk(mMutex);
        for (uint64_t j = 0; j < xs.size(); ++j)
        {
            mMap[xs[j]]++;
        }
    }

    Inserter(uint64_t pSeed, BackyardHash& pHash, map<uint64_t,uint64_t>& pMap, mutex& pMutex)
        : mSeed(pSeed), mHash(pHash), mMap(pMap), mMutex(pMutex)
    {
    }

private:
    const uint64_t mSeed;
    BackyardHash& mHash;
    map<uint64_t,uint64_t>& mMap;
    mutex& mMutex;
};
#endif

class Vis
{
public:
    void operator()(uint64_t pSlot, const BackyardHash::value_type& pVal, uint64_t pCount)
    {
        cerr << pSlot << '\t' << lexical_cast<string>(pVal) << '\t' << pCount << endl;
    }

};

class Inserter
{
public:
    void operator()()
    {
        const uint64_t M = (1ULL << 40) - 1;
        const uint64_t N = 50;
        std::mt19937 rng(mSeed);
        std::uniform_int_distribution<uint64_t> d(0, M);;
        for (uint64_t i = 0; i < N; ++i)
        {
            uint64_t x = d(rng);
            BOOST_CHECK(mHash.count(BackyardHash::value_type(x)) == 0);
            mHash.insert(BackyardHash::value_type(x));
            BOOST_CHECK(mHash.count(BackyardHash::value_type(x)) > 0);
            mMap[x]++;
            for (map<uint64_t,uint64_t>::const_iterator i = mMap.begin(); i != mMap.end(); ++i)
            {
                BackyardHash::value_type v(i->first);
                //cerr << "j = " << j << endl;
                //cerr << "v = " << i->first << '\t' << i->second << endl;
                //if (mHash.count(v) != i->second)
                //{
                //    cerr << mMap.size() << '\t' << mHash.size() << endl;
                //    Vis v;
                //    mHash.visit(v);
                //}
                BOOST_CHECK_EQUAL(mHash.count(v), i->second);
            }
        }
    }

    Inserter(uint64_t pSeed, BackyardHash& pHash, map<uint64_t,uint64_t>& pMap, mutex& pMutex)
        : mSeed(pSeed), mHash(pHash), mMap(pMap), mMutex(pMutex)
    {
    }

private:
    const uint64_t mSeed;
    BackyardHash& mHash;
    map<uint64_t,uint64_t>& mMap;
    mutex& mMutex;
};
typedef std::shared_ptr<Inserter> InserterPtr;

BOOST_AUTO_TEST_CASE(test7)
{
    BackyardHash h(8, 40, (1ULL << 8) + (1ULL << 7));
    map<uint64_t,uint64_t> m;

    const uint64_t T = 1;
    vector<InserterPtr> is;
    ThreadGroup g;
    mutex mtx;
    for (uint64_t i = 0; i < T; ++i)
    {
        is.push_back(InserterPtr(new Inserter(i + 17, h, m, mtx)));
        g.create(*is.back());
    }
    g.join();

    uint64_t j = 0;
    for (map<uint64_t,uint64_t>::const_iterator i = m.begin(); i != m.end(); ++i, ++j)
    {
        BackyardHash::value_type v(i->first);
        //cerr << "j = " << j << endl;
        //cerr << "v = " << i->first << '\t' << i->second << endl;
        BOOST_CHECK_EQUAL(h.count(v), i->second);
    }

    vector<uint32_t> perm;

    h.sort(perm, 1);

    //cerr << "perm[0] = " << perm[0] << endl;
    //cerr << lexical_cast<string>(h[perm[0]].first) << '\t' << lexical_cast<string>(h[perm[0]].second) << endl;
    pair<Gossamer::edge_type,uint64_t> prev = h[perm[0]];
    map<uint64_t,uint64_t>::const_iterator itr = m.begin();
    BOOST_CHECK_EQUAL(itr->first, prev.first.value().asUInt64());
    BOOST_CHECK_EQUAL(itr->second, prev.second);
    ++itr;
    for (uint32_t i = 1; i < perm.size(); ++i)
    {
        //cerr << "perm[i] = " << perm[i] << endl;
        pair<Gossamer::edge_type,uint64_t> itm = h[perm[i]];
        //cerr << lexical_cast<string>(itm.first) << '\t' << lexical_cast<string>(itm.second) << endl;
        if (itm.first < prev.first)
        {
            cerr << "edge: " << itm.first.value() << ", count: " << itm.second << '\n';
        }
        BOOST_CHECK(itm.first >= prev.first);
        if (itm.first == prev.first)
        {
            prev.second += itm.second;
            continue;
        }
        BOOST_CHECK(itr != m.end());
        BOOST_CHECK_EQUAL(itr->first, itm.first.value().asUInt64());
        BOOST_CHECK_EQUAL(itr->second, itm.second);
        ++itr;
        prev = itm;
    }
}

#include "testEnd.hh"
