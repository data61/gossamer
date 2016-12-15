// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PHYSICALFILEFACTORY_HH
#define PHYSICALFILEFACTORY_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef STD_IOSTREAM
#include <iostream>
#define STD_IOSTREAM
#endif

class PhysicalFileFactory : public FileFactory
{
public:
    // Open a file for reading
    virtual InHolderPtr in(const std::string& pFileName) const;

    // Open a file for writing
    virtual OutHolderPtr out(const std::string& pFileName, FileMode pMode = TruncMode) const;

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

    // Create a unique temprorary file name.
    virtual std::string tmpName();

    virtual void populate(bool pPopulate)
    {
        mPopulate = pPopulate;
    }

    void turnOffSpecialFileHandling()
    {
        mSpecialFileHandling = false;
    }

    void turnOnSpecialFileHandling()
    {
        mSpecialFileHandling = true;
    }

    explicit PhysicalFileFactory(const std::string& pTmpDir = "/tmp",
                                 bool pSpecialFileHandling = true)
        : mSpecialFileHandling(pSpecialFileHandling), mPopulate(true),
          mTmpDir(pTmpDir)
    {
    }

private:
    bool mSpecialFileHandling;
    bool mPopulate;
    std::string mTmpDir;
};

#endif // PHYSICALFILEFACTORY_HH
