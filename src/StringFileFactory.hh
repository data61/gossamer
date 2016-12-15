// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef STRINGFILEFACTORY_HH
#define STRINGFILEFACTORY_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef LOGGER_HH
#include "Logger.hh"
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

class StringFileFactory : public FileFactory
{
public:
    // Open a file for reading
    virtual InHolderPtr in(const std::string& pFileName) const;

    // Open a file for writing
    virtual OutHolderPtr out(const std::string& pFileName, FileMode = TruncMode) const;

    // Open a file for mapping
    virtual MappedHolderPtr map(const std::string& pFileName) const;

    // Remove a file.
    virtual void remove(const std::string& pFileName) const;

    // Copy a file.
    virtual void copy(const std::string& pFrom, const std::string& pTo) const;

    // Return true iff a file exists.
    virtual bool exists(const std::string& pFileName) const;

    // Return the size of a file, or throw an exception if it does not exist.
    virtual uint64_t size(const std::string& pFileName) const;

    // Log an event.
    virtual const Logger& logger() const
    {
        return mLogger;
    }

    virtual void populate(bool pPopulate)
    {
        // do nothing.
    }

    StringFileFactory();

    void addFile(const std::string& pName, const std::string& pContents)
    {
        mFiles[pName] = pContents;
    }

    const bool
    fileExists(const std::string& pName) const
    {
        return mFiles.find(pName) != mFiles.end();
    }

    const std::string&
    readFile(const std::string& pName) const
    {
        return mFiles.find(pName)->second;
    }

private:
    mutable std::map<std::string,std::string> mFiles;
    Logger mLogger;
};

#endif // STRINGFILEFACTORY_HH
