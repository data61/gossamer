// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef FILEFACTORY_HH
#define FILEFACTORY_HH

#ifndef STD_ISTREAM
#include <istream>
#define STD_ISTREAM
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STDINT_H
#include <stdint.h>
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

//#ifndef BOOST_UUID_HPP
//#include <boost/uuid/uuid.hpp>
//#define BOOST_UUID_HPP
//#endif
//
//#ifndef BOOST_UUID_GENERATORS_HPP
//#include <boost/uuid/uuid_generators.hpp>
//#define BOOST_UUID_GENERATORS_HPP
//#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

#ifndef BOOST_DATE_TIME_LOCAL_TIME_LOCAL_TIME_HPP
#include <boost/date_time/local_time/local_time.hpp>
#define BOOST_DATE_TIME_LOCAL_TIME_LOCAL_TIME_HPP
#endif

#ifndef UTILS_HH
#include "Utils.hh"
#endif


#if defined(__GNUC__)
#include <sys/time.h>
#endif
#include <time.h>

enum Severity { trace, debug, info, warning, error, fatal };

namespace boost
{
template<>
inline const char* lexical_cast<const char*,Severity>(const Severity& pSev)
{
    static const char* strs[] = {
        "trace", "debug", "info", "warning", "error", "fatal"
    };
    return strs[pSev];
}
} // namespace boost

class FileFactory
{
public:
    enum FileMode { TruncMode, AppendMode };

    class InHolder
    {
    public:
        virtual std::istream& operator*() = 0;

        virtual ~InHolder() {}
    };
    typedef std::shared_ptr<InHolder> InHolderPtr;
    
    class OutHolder
    {
    public:
        virtual std::ostream& operator*() = 0;

        virtual ~OutHolder() {}
    };
    typedef std::shared_ptr<OutHolder> OutHolderPtr;

    class MappedHolder
    {
    public:
        virtual uint64_t size() const = 0;

        virtual const void* data() const = 0;

        virtual ~MappedHolder() {}
    };
    typedef std::shared_ptr<MappedHolder> MappedHolderPtr;

    class TmpFileHolder
    {
    public:
        const std::string& name() const
        {
            return mName;
        }

        TmpFileHolder(FileFactory& pFactory, const std::string& pName)
            : mFactory(pFactory), mName(pName)
        {
        }

        ~TmpFileHolder();

    private:
        FileFactory& mFactory;
        const std::string mName;
    };
    typedef std::shared_ptr<TmpFileHolder> TmpFileHolderPtr;

    // Open a file for reading
    virtual InHolderPtr in(const std::string& pFileName) const = 0;

    // Open a file for writing
    virtual OutHolderPtr out(const std::string& pFileName, FileMode pMode = TruncMode) const = 0;

    // Map a file.
    virtual MappedHolderPtr map(const std::string& pFileName) const = 0;

    // Remove a file.
    virtual void remove(const std::string& pFileName) const = 0;

    // Copy a file.
    virtual void copy(const std::string& pFileNameFrom, const std::string& pFileNameTo) const = 0;

    // Return true iff a file exists.
    virtual bool exists(const std::string& pFileName) const = 0;

    // Return the size of a file, or throw an exception if it does not exist.
    virtual uint64_t size(const std::string& pFileName) const = 0;

    // Create a unique temprorary file name.
    virtual std::string tmpName();

    // Set the flag to populate mmappings at map time.
    virtual void populate(bool pPopulate) = 0;

    // virtual Destructor
    virtual ~FileFactory();
};

class FileThunkBase
{
private:
    const FileFactory& mFileFactory;
    std::string mFileName;

public:
    const FileFactory& factory() const
    {
        return mFileFactory;
    }

    const std::string& filename() const
    {
        return mFileName;
    }

protected:
    FileThunkBase(const FileFactory& pFileFactory, const std::string& pFileName)
        : mFileFactory(pFileFactory), mFileName(pFileName)
    {
    }
};


class FileThunkIn : public FileThunkBase
{
public:
    FileThunkIn(const FileFactory& pFileFactory, const std::string& pFileName)
        : FileThunkBase(pFileFactory, pFileName)
    {
    }

    FileFactory::InHolderPtr operator()() const
    {
        return factory().in(filename());
    }
};

class FileThunkMap : public FileThunkBase
{
public:
    FileThunkMap(const FileFactory& pFileFactory, const std::string& pFileName)
        : FileThunkBase(pFileFactory, pFileName)
    {
    }
    
    FileFactory::MappedHolderPtr operator()() const
    {
        return factory().map(filename());
    }
};


class FileThunkOut : public FileThunkBase
{
private:
    FileFactory::FileMode mMode;

public:
    FileFactory::FileMode mode() const
    {
        return mMode;
    }

    FileThunkOut(const FileFactory& pFileFactory, const std::string& pFileName,
                 FileFactory::FileMode pMode = FileFactory::TruncMode)
        : FileThunkBase(pFileFactory, pFileName), mMode(pMode)
    {
    }
    
    FileFactory::OutHolderPtr operator()() const
    {
        return factory().out(filename(), mode());
    }
};

typedef std::shared_ptr<FileFactory> FileFactoryPtr;

#endif // FILEFACTORY_HH
