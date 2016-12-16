// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "PhysicalFileFactory.hh"

#include "GossamerException.hh"
#include "MappedFile.hh"

#include <stdint.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <boost/algorithm/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;
using namespace boost::algorithm;
using namespace boost::iostreams;
using namespace boost::filesystem;
using namespace Gossamer;

namespace // anonymous
{

class StdCinHolder : public FileFactory::InHolder
{
public:
    virtual istream& operator*()
    {
        return cin;
    }

    StdCinHolder()
    {
    }
};

class PlainInHolder : public FileFactory::InHolder
{
public:
    virtual istream& operator*()
    {
        return mFile;
    }

    PlainInHolder(const string& pFileName)
        : mFileName(pFileName), mFile(mFileName.c_str(), ios::binary )
    {
        if (!mFile.good())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(mFileName));
        }
        mFile.exceptions(std::ifstream::badbit);
    }

private:
    string mFileName;
    std::ifstream mFile;
};

class GzippedInHolder : public FileFactory::InHolder
{
public:
    virtual istream& operator*()
    {
        return mFilter;
    }

    GzippedInHolder(const string& pFileName)
        : mFileName(pFileName), mFile(mFileName.c_str(), ios::binary )
    {
        if (!mFile.good())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(mFileName));
        }
        mFile.exceptions(std::ifstream::badbit);
        mFilter.push(gzip_decompressor());
        mFilter.push(mFile);
    }

private:
    string mFileName;
    std::ifstream mFile;
    filtering_stream<input> mFilter;
};


class BzippedInHolder : public FileFactory::InHolder
{
public:
    virtual istream& operator*()
    {
        return mFilter;
    }

    BzippedInHolder(const string& pFileName)
        : mFileName(pFileName), mFile(mFileName.c_str(), ios::binary )
    {
        if (!mFile.good())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(mFileName));
        }
        mFile.exceptions(std::ifstream::badbit);
        mFilter.push(bzip2_decompressor(false, 63493103));
        mFilter.push(mFile);
    }

private:
    string mFileName;
    std::ifstream mFile;
    filtering_stream<input> mFilter;
};


class StdCoutHolder : public FileFactory::OutHolder
{
public:
    virtual ostream& operator*()
    {
        return cout;
    }

    StdCoutHolder()
    {
    }
};

class PlainOutHolder : public FileFactory::OutHolder
{
public:
    virtual ostream& operator*()
    {
        return mFile;
    }

    PlainOutHolder(const string& pFileName, FileFactory::FileMode pMode)
        : mFileName(pFileName), mFile(mFileName.c_str(), (pMode == FileFactory::TruncMode ? ios::trunc : ios::app) | ios::binary )
    {
        if (!mFile.good())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(mFileName));
        }
        mFile.exceptions(std::ofstream::badbit);
    }

private:
    string mFileName;
    std::ofstream mFile;
};

class PlainMappedHolder : public FileFactory::MappedHolder
{
public:
    virtual uint64_t size() const
    {
        return mMapped.size();
    }

    virtual const void* data() const
    {
        return reinterpret_cast<const void*>(mMapped.begin());
    }

    PlainMappedHolder(const string& pFileName, bool pPopulate)
        : mFileName(pFileName), mMapped(mFileName.c_str(), pPopulate)
    {
    }

private:
    string mFileName;
    MappedFile<uint8_t> mMapped;
};

class GzippedOutHolder : public FileFactory::OutHolder
{
public:
    virtual ostream& operator*()
    {
        return mFilter;
    }

    GzippedOutHolder(const string& pFileName, FileFactory::FileMode pMode)
        : mFileName(pFileName), mFile(mFileName.c_str(), (pMode == FileFactory::TruncMode ? ios::trunc : ios::app) | ios::binary )
    {
        if (!mFile.good())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(mFileName));
        }
        mFile.exceptions(std::ofstream::badbit);
        mFilter.push(gzip_compressor());
        mFilter.push(mFile);
    }

private:
    string mFileName;
    std::ofstream mFile;
    filtering_stream<output> mFilter;
};


class BzippedOutHolder : public FileFactory::OutHolder
{
public:
    virtual ostream& operator*()
    {
        return mFilter;
    }

    BzippedOutHolder(const string& pFileName, FileFactory::FileMode pMode)
        : mFileName(pFileName), mFile(mFileName.c_str(), (pMode == FileFactory::TruncMode ? ios::trunc : ios::app) | ios::binary )
    {
        if (!mFile.good())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(mFileName));
        }
        mFile.exceptions(std::ofstream::badbit);
        mFilter.push(bzip2_compressor());
        mFilter.push(mFile);
    }

private:
    string mFileName;
    std::ofstream mFile;
    filtering_stream<output> mFilter;
};


} // namespace anonymous


// Open a file for reading
//
FileFactory::InHolderPtr
PhysicalFileFactory::in(const string& pFileName) const
{
    if (mSpecialFileHandling && pFileName == "-")
    {
        return InHolderPtr(new StdCinHolder);
    }
    if (mSpecialFileHandling && ends_with(pFileName, ".gz"))
    {
        return InHolderPtr(new GzippedInHolder(pFileName));
    }
    if (mSpecialFileHandling && ends_with(pFileName, ".bz2"))
    {
        return InHolderPtr(new BzippedInHolder(pFileName));
    }
    return InHolderPtr(new PlainInHolder(pFileName));
}


// Open a file for writing
//
FileFactory::OutHolderPtr
PhysicalFileFactory::out(const string& pFileName, FileMode pMode) const
{
    if (mSpecialFileHandling && pFileName == "-")
    {
        return OutHolderPtr(new StdCoutHolder);
    }
    if (mSpecialFileHandling && ends_with(pFileName, ".gz"))
    {
        return OutHolderPtr(new GzippedOutHolder(pFileName, pMode));
    }
    if (mSpecialFileHandling && ends_with(pFileName, ".bz2"))
    {
        return OutHolderPtr(new BzippedOutHolder(pFileName, pMode));
    }
    return OutHolderPtr(new PlainOutHolder(pFileName, pMode));
}


// Open a file for mapping
//
FileFactory::MappedHolderPtr
PhysicalFileFactory::map(const string& pFileName) const
{
    return MappedHolderPtr(new PlainMappedHolder(pFileName, mPopulate));
}

// Remove a file
//
void
PhysicalFileFactory::remove(const string& pFileName) const
{
    if (pFileName != "-")
    {
        string s(pFileName);
        int err = unlink(s.c_str());
        if (err)
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << errinfo_errno(errno)
                    << errinfo_file_name(pFileName));
        }
    }
}

// Copy a file
//
void
PhysicalFileFactory::copy(const string& pFrom, const string& pTo) const
{
    if (pFrom != "-")
    {
        copy_file(pFrom, pTo);
    }
}

// True iff the file exists.
//
bool
PhysicalFileFactory::exists(const string& pFileName) const
{
    return boost::filesystem::exists(pFileName);
}

// Find the size of a file.
//
uint64_t
PhysicalFileFactory::size(const string& pFileName) const
{
    uintmax_t size = boost::filesystem::file_size(pFileName);
    if( size == static_cast<uintmax_t>(-1) )
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << boost::errinfo_errno(errno));
    }
    return size;
}

// Create a unique temprorary file name.
//
std::string
PhysicalFileFactory::tmpName()
{
    static uint64_t serial = 0;
    std::string nm = mTmpDir;
    nm += "/";
    //boost::uuids::uuid u(boost::uuids::ranomd_generator()());
    //nm += boost::lexical_cast<std::string>(u);
    struct timeval t;
    gettimeofday(&t, NULL);
    nm += boost::lexical_cast<std::string>(t.tv_sec);
    nm += "-";
    nm += boost::lexical_cast<std::string>(t.tv_usec);
    nm += "-";
    nm += boost::lexical_cast<std::string>(serial++);
    return nm;
}

