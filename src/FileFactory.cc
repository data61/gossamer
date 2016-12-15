// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "FileFactory.hh"

FileFactory::TmpFileHolder::~TmpFileHolder()
{
    mFactory.remove(mName);
}

FileFactory::~FileFactory()
{
}

std::string
FileFactory::tmpName()
{
    static uint64_t serial = 0;
    //std::string nm = "/tmp/"; //???
    std::string nm = Gossamer::defaultTmpDir() + "/";

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

