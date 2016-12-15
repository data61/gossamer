// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

class Closer
{
public:
    Closer(int pFd)
        : mFd(pFd)
    {
    }

    ~Closer()
    {
        close(mFd);
    }

private:
    int mFd;
};

// Constructor
//
template <typename T>
void
MappedFile<T>::open(const char* pFileName, bool pPopulate, Permissions pPerms, Mode pMode)
{
    using namespace boost;

    int fflags;
    int mflags;
    int pflags;
    if (pPerms == ReadOnly)
    {
        fflags = O_RDONLY;
        mflags = PROT_READ;
    }
    else
    {
        fflags = O_RDWR;
        mflags = PROT_READ | PROT_WRITE;
    }
    if (pMode == Private)
    {
        pflags = MAP_PRIVATE;
    }
    else
    {
        pflags = MAP_SHARED;
    }
#ifdef MAP_HUGETLB
    pflags |= MAP_HUGETLB;
#endif
#ifdef MAP_POPULATE
    if (pPopulate)
    {
        pflags |= MAP_POPULATE;
    }
#endif
    int fd = ::open(pFileName, fflags);
    if (!fd)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << boost::errinfo_errno(errno));
    }
    Closer closer(fd);
    struct stat buf;
    int err = fstat(fd, &buf);
    if (err != 0)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << boost::errinfo_errno(errno));
    }
    uint64_t z = sizeof(T) * (buf.st_size / sizeof(T));
    if (z == 0)
    {
        mSize = 0;
        mBase = NULL;
        return;
    }
    void* res = mmap(NULL, z, mflags, pflags, fd, 0);
#ifdef MAP_HUGETLB
    if (res == MAP_FAILED && (pflags & MAP_HUGETLB) )
    {
        // try again with out huge table support
        pflags &= ~MAP_HUGETLB;
        res = mmap(NULL, z, mflags, pflags, fd, 0);
    }
#endif

    if (res == MAP_FAILED )
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << boost::errinfo_errno(errno));
    }
    mBase = reinterpret_cast<T*>(res);
    mSize = z / sizeof(T);
}


// Destructor
//
template <typename T>
MappedFile<T>::~MappedFile()
{
    if (mBase)
    {
        munmap(mBase, mSize * sizeof(T));
    }
}
