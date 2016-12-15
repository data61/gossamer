// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "StringFileFactory.hh"
#include "GossamerException.hh"

#include <iostream>
#include <sstream>

using namespace std;
using namespace boost;
using namespace Gossamer;

namespace // anonymous
{

class StringInHolder : public FileFactory::InHolder
{
public:
    virtual istream& operator*()
    {
        return mFile;
    }

    StringInHolder(const string& pContent)
        : mFile(pContent)
    {
    }

private:
    istringstream mFile;
};

class StringOutHolder : public FileFactory::OutHolder
{
private:
    typedef std::shared_ptr<ostringstream> OutPtr;

public:
    virtual ostream& operator*()
    {
        if (mFileHolder)
        {
            return *mFileHolder;
        }
        return mFileHere;
    }

    StringOutHolder(const string& pFileName, FileFactory::FileMode pMode, map<string,string>& pStore)
        : mFileName(pFileName), mStore(pStore)
    {
        if (pMode == FileFactory::TruncMode)
        {
            return;
        }

        map<string,string>::iterator i = mStore.find(pFileName);
        if (i == mStore.end())
        {
            return;
        }
#if defined(GOSS_WINDOWS_X64)
        mFileHolder = OutPtr(new ostringstream(i->second, ios::ate)); // for some reason ios::app doesn't work in windows
#else
        mFileHolder = OutPtr(new ostringstream(i->second, ios::app));
#endif
    }

    ~StringOutHolder()
    {
        if (mFileHolder)
        {
            mStore[mFileName] = mFileHolder->str();
        }
        else
        {
            mStore[mFileName] = mFileHere.str();
        }
    }

private:
    string mFileName;
    ostringstream mFileHere;
    OutPtr mFileHolder;
    map<string,string>& mStore;
};

class StringMappedHolder : public FileFactory::MappedHolder
{
public:
    virtual uint64_t size() const
    {
        return mContent.size();
    }

    virtual const void* data() const
    {
        return reinterpret_cast<const void*>(mContent.data());
    }

    StringMappedHolder(const string& pFileName, const string& pContent)
        : mFileName(pFileName), mContent(pContent)
    {
    }

private:
    string mFileName;
    string mContent;
};

} // namespace anonymous


// Open a file for reading
//
FileFactory::InHolderPtr
StringFileFactory::in(const string& pFileName) const
{
    std::map<string,string>::const_iterator i;
    i = mFiles.find(pFileName);
    if (i == mFiles.end())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << errinfo_file_name(pFileName));
    }
    return InHolderPtr(new StringInHolder(i->second));
}


// Open a file for writing
//
FileFactory::OutHolderPtr
StringFileFactory::out(const string& pFileName, FileMode pMode) const
{
    return OutHolderPtr(new StringOutHolder(pFileName, pMode, mFiles));
}


// Open a file for mapping
//
FileFactory::MappedHolderPtr
StringFileFactory::map(const string& pFileName) const
{
    std::map<string,string>::const_iterator i;
    i = mFiles.find(pFileName);
    if (i == mFiles.end())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << errinfo_file_name(pFileName));
    }
    return MappedHolderPtr(new StringMappedHolder(pFileName, i->second));
}

// Remove a file.
//
void
StringFileFactory::remove(const std::string& pFileName) const
{
    mFiles.erase(pFileName);
}

// Copy a file.
//
void
StringFileFactory::copy(const std::string& pFrom, const std::string& pTo) const
{
    std::map<string,string>::const_iterator i;
    i = mFiles.find(pFrom);
    if (i == mFiles.end())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << errinfo_file_name(pFrom));
    }
    mFiles[pTo] = i->second;
}


// True iff the file exists.
//
bool
StringFileFactory::exists(const string& pFileName) const
{
    std::map<string,string>::const_iterator i;
    i = mFiles.find(pFileName);
    return i != mFiles.end();
}

// Find the size of a file.
//
uint64_t
StringFileFactory::size(const string& pFileName) const
{
    std::map<string,string>::const_iterator i;
    i = mFiles.find(pFileName);
    if (i == mFiles.end())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << errinfo_file_name(pFileName));
    }
    return i->second.size();
}

// Constructor
//
StringFileFactory::StringFileFactory()
    : mLogger(std::cerr)
{
}
