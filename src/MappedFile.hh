// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef MAPPEDFILE_HH
#define MAPPEDFILE_HH

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#if !defined(GOSS_LINUX_X64)
#include <boost/iostreams/device/mapped_file.hpp>
#endif

template <typename T>
class MappedFile
{
public:
    enum Permissions { ReadOnly, ReadWrite };
    enum Mode { Shared, Private };

    uint64_t size() const
    {
        return mSize;
    }

    const T& operator[](uint64_t pIdx) const
    {
        return mBase[pIdx];
    }

    T& operator[](uint64_t pIdx)
    {
        return mBase[pIdx];
    }

    const T* begin() const
    {
        return mBase;
    }

    T* begin()
    {
        return mBase;
    }

    const T* end() const
    {
        return mBase + mSize;
    }

    T* end()
    {
        return mBase + mSize;
    }

    // Extend/Shrink the file and remap it.
    void resize(uint64_t pSize);

    // Constructor
    explicit MappedFile(const std::string& pFileName, bool pPopulate, Permissions pPerms = ReadOnly, Mode pMode = Shared)
    {
        open(pFileName.c_str(), pPopulate, pPerms, pMode);
    }

    // Constructor
    explicit MappedFile(const char* pFileName, bool pPopulate, Permissions pPerms = ReadOnly, Mode pMode = Shared)
    {
        open(pFileName, pPopulate, pPerms, pMode);
    }

    // Destructor
    ~MappedFile();

private:
    void open(const char* pFileName, bool pPopulate, Permissions pPerms, Mode pMode);

    T* mBase;
    uint64_t mSize;

#if !defined(GOSS_LINUX_X64) && !defined(GOSS_MACOSX_X64)
    boost::iostreams::mapped_file mMappedFile;
#endif
};

#if defined(GOSS_LINUX_X64) || defined(GOSS_MACOSX_X64)
#include "MappedFile.tcc"
#else
#include "MappedFile_win.hh"
#endif

#endif // MAPPEDFILE_HH
