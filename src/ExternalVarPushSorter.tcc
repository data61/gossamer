// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef STD_DEQUE
#include <deque>
#define STD_DEQUE
#endif


template <typename T, typename V>
ExternalVarPushSorter<T,V>::ExternalVarPushSorter(
        FileFactory& pFactory, uint64_t pMaxBufItems)
    : mFactory(pFactory),
      mBase(mFactory.tmpName()), mMaxBufItems(pMaxBufItems),
      mFileNum(0)
{
}


template<typename T, typename V>
void
ExternalVarPushSorter<T,V>::flush()
{
    std::sort(mPerm.begin(), mPerm.end(), IndirectComparator<T>(mBuf));
    std::string fn;
    genFileName(mBase, ++mFileNum, fn);
    FileFactory::TmpFileHolderPtr h(new FileFactory::TmpFileHolder(mFactory, fn));
    FileFactory::OutHolderPtr outp(mFactory.out(fn));
    std::ostream& out(**outp);
    for (uint64_t i = 0; i < mBuf.size(); ++i)
    {
        V::write(out, mBuf[mPerm[i]]);
    }
    mBuf.clear();
    mPerm.clear();
    mFiles.push_back(h);
}


template <typename T, typename V>
template <typename Dest>
void
ExternalVarPushSorter<T,V>::sort(Dest& pDest)
{
    typedef FileInWrapper<T,V> inwrapper_t;
    typedef FileOutWrapper<T,V> outwrapper_t;

    if (mBuf.size() > 0)
    {
        flush();
    }

    std::deque<FileFactory::TmpFileHolderPtr> q(mFiles.begin(), mFiles.end());
    mFiles.clear();
    while (q.size() > 2)
    {
        // merge pairs of files.
        FileFactory::TmpFileHolderPtr p0(q.front());
        q.pop_front();
        FileFactory::TmpFileHolderPtr p1(q.front());
        q.pop_front();

        inwrapper_t wlhs(p0->name(), mFactory);
        inwrapper_t wrhs(p1->name(), mFactory);

        std::string fn;
        genFileName(mBase, ++mFileNum, fn);
        FileFactory::TmpFileHolderPtr pd(new FileFactory::TmpFileHolder(mFactory, fn));
        outwrapper_t wout(fn, mFactory);
        merge(wlhs, wrhs, wout);
        q.push_back(pd);
    }

    if (q.size() == 1)
    {
        FileFactory::TmpFileHolderPtr p0(q.front());
        q.pop_front();
        inwrapper_t wlhs(p0->name(), mFactory);
        copy(wlhs, pDest);
    }
    else if (q.size() == 2)
    {
        FileFactory::TmpFileHolderPtr p0(q.front());
        q.pop_front();
        FileFactory::TmpFileHolderPtr p1(q.front());
        q.pop_front();

        inwrapper_t wlhs(p0->name(), mFactory);
        inwrapper_t wrhs(p1->name(), mFactory);
        merge(wlhs, wrhs, pDest);
    }
}


