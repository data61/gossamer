// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PROFILE_HH
#define PROFILE_HH

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_UNORDERED_MAP
#include <unordered_map>
#define STD_UNORDERED_MAP
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

#ifndef STD_THREAD
#include <thread>
#define STD_THREAD
#endif

#ifndef STD_MUTEX
#include <mutex>
#define STD_MUTEX
#endif

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif

#ifndef BOOST_THREAD_TSS_HPP
#include <boost/thread/tss.hpp>
#define BOOST_THREAD_TSS_HPP
#endif

#define GOSS_PROFILING_ENABLED
#undef GOSS_PROFILING_ENABLED

class Profile
{
public:
    struct Node;
    typedef std::shared_ptr<Node> NodePtr;
    typedef std::vector<NodePtr> Roots;
    typedef std::shared_ptr<Roots> RootsPtr;

    struct Node
    {
        const char* label;
        uint64_t calls;
        uint64_t time;
        std::unordered_map<const char*,NodePtr> kids;

        void print(std::ostream& pOut, uint64_t pInd) const
        {
            ind(pOut, pInd);
            pOut << label << '\n';
            ind(pOut, pInd + 1);
            pOut << calls << '\n';
            ind(pOut, pInd + 1);
            pOut << (time * 1e-9) << '\n';
            for (std::unordered_map<const char*,NodePtr>::const_iterator i = kids.begin();
                    i != kids.end(); ++i)
            {
                i->second->print(pOut, pInd + 1);
            }
        }

        Node(const char* pLabel)
            : label(pLabel), calls(0), time(0)
        {
        }

    private:
        static void ind(std::ostream& pOut, uint64_t pInd)
        {
            for (uint64_t i = 0; i < pInd; ++i)
            {
                pOut << ' ';
            }
        }
    };

    class Context
    {
    public:
        Context(const char* pLabel)
        {
#ifdef GOSS_PROFILING_ENABLED
            mParent = Profile::getCurrentNode();
            if (!mParent)
            {
                mParent = NodePtr(new Node("<root>"));
                Profile::addThreadRoot(mParent);
            }
            std::unordered_map<const char*,NodePtr>::iterator i = mParent->kids.find(pLabel);
            if (i == mParent->kids.end())
            {
                mParent->kids[pLabel] = NodePtr(new Node(pLabel));
                i = mParent->kids.find(pLabel);
            }
            mCurrent = i->second;
            mCurrent->calls++;
            Profile::setCurrentNode(mCurrent);
            mStart = now();
#endif
        }

        ~Context()
        {
#ifdef GOSS_PROFILING_ENABLED
            uint64_t finish = now();
            mCurrent->time += (finish - mStart);
            Profile::setCurrentNode(mParent);
#endif
        }

    private:
#ifdef GOSS_PROFILING_ENABLED
        static uint64_t now()
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            return ts.tv_nsec + 1000000000ULL * ts.tv_sec;
        }
#endif

#ifdef GOSS_PROFILING_ENABLED
        NodePtr mParent;
        NodePtr mCurrent;
        uint64_t mStart;
#endif
    };

    static void addThreadRoot(const NodePtr& pNodePtr)
    {
        std::unique_lock<std::mutex> lk(sMutex);
        if (!sThreadRoots)
        {
            sThreadRoots = RootsPtr(new Roots);
        }
        sThreadRoots->push_back(pNodePtr);
    }

    static NodePtr getCurrentNode()
    {
        NodePtr* np = sCurrentNodePtr.get();
        if (np == NULL)
        {
            sCurrentNodePtr.reset(np = new NodePtr);
        }
        return *np;
    }

    static void setCurrentNode(const NodePtr& pNodePtr)
    {
        NodePtr* np = sCurrentNodePtr.get();
        if (np == NULL)
        {
            sCurrentNodePtr.reset(np = new NodePtr);
        }
        *np = pNodePtr;
    }

    static void print(std::ostream& pOut)
    {
        if (!sThreadRoots)
        {
            return;
        }
        for (uint64_t i = 0; i < sThreadRoots->size(); ++i)
        {
            (*sThreadRoots)[i]->print(pOut, 0);
        }
    }

private:
    static std::mutex sMutex;
    static boost::thread_specific_ptr<NodePtr> sCurrentNodePtr;
    static RootsPtr sThreadRoots;
};

#endif // PROFILE_HH
