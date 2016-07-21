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

