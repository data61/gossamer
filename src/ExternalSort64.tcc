// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#if defined(__GNUC__)
#ifndef UNISTD_H
#include <unistd.h>
#define UNISTD_H
#endif
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef STD_DEQUE
#include <deque>
#define STD_DEQUE
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

template <typename T, typename V>
template <typename SrcItr, typename DestItr>
void
ExternalSort<T,V>::sort(SrcItr& pSrc, DestItr& pDest, FileFactory& pFactory,
                        uint64_t pBufSpace, Logger* pLogger)
{
    using namespace boost;
    using namespace std;

    typedef FileInWrapper<T,V> MergeAtom;
    typedef FileOutWrapper<T,V> DestAtom;

    // number of items to buffer internally.
    //static const uint64_t N = 256UL * 1024UL;
    const uint64_t N = pBufSpace / sizeof(T);

    vector<T> buf;
    buf.reserve(N);

    string fn;
    vector<FileFactory::TmpFileHolderPtr> files;

    string base(pFactory.tmpName());
    uint64_t j = 0;
    uint64_t tot = 0;
    while (pSrc.valid())
    {
        buf.push_back(*pSrc);
        if (buf.size() == N)
        {
            tot += N;
            std::sort(buf.begin(), buf.end());
            //RadixSort64::sort(32, buf);
            genFileName(base, ++j, fn);
            FileFactory::TmpFileHolderPtr h(new FileFactory::TmpFileHolder(pFactory, fn));
            if (pLogger)
            {
                (*pLogger)(info, "writing " + fn);
            }
            FileFactory::OutHolderPtr outp(pFactory.out(fn));
            ostream& out(**outp);
            out.write(reinterpret_cast<const char*>(&buf[0]),
                      sizeof(T) * buf.size());
            buf.clear();
            files.push_back(h);
        }
        ++pSrc;
    }
    if (buf.size() > 0)
    {
        tot += buf.size();
        std::sort(buf.begin(), buf.end());
        //RadixSort64::sort(32, buf);
        genFileName(base, ++j, fn);
        FileFactory::TmpFileHolderPtr h(new FileFactory::TmpFileHolder(pFactory, fn));
        if (pLogger)
        {
            (*pLogger)(info, "writing " + fn);
        }
        FileFactory::OutHolderPtr outp(pFactory.out(fn));
        ostream& out(**outp);
        out.write(reinterpret_cast<const char*>(&buf[0]),
                  sizeof(T) * buf.size());
        buf.clear();
        files.push_back(h);
    }
    {
        vector<T> x;
        buf.swap(x);
    }

    static const uint64_t J = 1024 * 1024;

    deque<FileFactory::TmpFileHolderPtr> q(files.begin(), files.end());
    files.clear();
    while (q.size() > 4)
    {
        FileFactory::TmpFileHolderPtr p0(q.front());
        q.pop_front();
        FileFactory::TmpFileHolderPtr p1(q.front());
        q.pop_front();
        MergeAtom a0(p0->name(), pFactory);
        MergeAtom a1(p1->name(), pFactory);
        Merger<MergeAtom,T> m0(J, a0, a1);

        FileFactory::TmpFileHolderPtr p2(q.front());
        q.pop_front();
        FileFactory::TmpFileHolderPtr p3(q.front());
        q.pop_front();
        MergeAtom a2(p2->name(), pFactory);
        MergeAtom a3(p3->name(), pFactory);
        Merger<MergeAtom,T> m1(J, a2, a3);

        genFileName(base, ++j, fn);
        FileFactory::TmpFileHolderPtr pd(new FileFactory::TmpFileHolder(pFactory, fn));
        DestAtom d(fn, pFactory);
        if (pLogger)
        {
            (*pLogger)(info, "merging into " + fn);
        }
        merge(m0, m1, d);
        q.push_back(pd);
    }

    if (pLogger)
    {
        (*pLogger)(info, "commencing final merge and graph construction");
        for (uint64_t i = 0; i < q.size(); ++i)
        {
            (*pLogger)(info, "  " + q[i]->name());
        }
    }
    pDest.prepare(tot);
    switch (q.size())
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            FileFactory::TmpFileHolderPtr p0(q.front());
            q.pop_front();
            MergeAtom a0(p0->name(), pFactory);
            copy(a0, pDest);
            break;
        }
        case 2:
        {
            FileFactory::TmpFileHolderPtr p0(q.front());
            q.pop_front();
            FileFactory::TmpFileHolderPtr p1(q.front());
            q.pop_front();
            MergeAtom a0(p0->name(), pFactory);
            MergeAtom a1(p1->name(), pFactory);
            merge(a0, a1, pDest);
            break;
        }
        case 3:
        {
            FileFactory::TmpFileHolderPtr p0(q.front());
            q.pop_front();
            FileFactory::TmpFileHolderPtr p1(q.front());
            q.pop_front();
            MergeAtom a0(p0->name(), pFactory);
            MergeAtom a1(p1->name(), pFactory);
            Merger<MergeAtom,T> m0(J, a0, a1);

            FileFactory::TmpFileHolderPtr p2(q.front());
            q.pop_front();
            MergeAtom a2(p2->name(), pFactory);

            merge(m0, a2, pDest);
            break;
        }
        case 4:
        {
            FileFactory::TmpFileHolderPtr p0(q.front());
            q.pop_front();
            FileFactory::TmpFileHolderPtr p1(q.front());
            q.pop_front();
            MergeAtom a0(p0->name(), pFactory);
            MergeAtom a1(p1->name(), pFactory);
            Merger<MergeAtom,T> m0(J, a0, a1);

            FileFactory::TmpFileHolderPtr p2(q.front());
            q.pop_front();
            FileFactory::TmpFileHolderPtr p3(q.front());
            q.pop_front();
            MergeAtom a2(p2->name(), pFactory);
            MergeAtom a3(p3->name(), pFactory);
            Merger<MergeAtom,T> m1(J, a2, a3);

            merge(m0, m1, pDest);
            break;
        }
        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("q.size() has nonsensical value "
                                                    + boost::lexical_cast<std::string>(q.size())));
        }
    }
}
