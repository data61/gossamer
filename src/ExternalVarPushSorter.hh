#ifndef EXTERNALVARPUSHSORTER_HH
#define EXTERNALVARPUSHSORTER_HH

#ifndef EXTERNALSORTUTILS_HH
#include "ExternalSortUtils.hh"
#endif

#if defined(__GNUC__)
#ifndef UNISTD_H
#include <unistd.h>
#define UNISTD_H
#endif
#endif

#ifndef STD_ALGORITHM
#include <algorithm>
#define STD_ALGORITHM
#endif

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif


template <typename T, typename V = ExternalSortCodec<T> >
class ExternalVarPushSorter : public ExternalSortBase
{
public:
    void push_back(const T& pItem)
    {
        mBuf.push_back(pItem);
        mPerm.push_back(mBuf.size() - 1);
        if (mBuf.size() == mMaxBufItems)
        {
            flush();
        }
    }

    template <typename Dest>
    void sort(Dest& pDest);

    ExternalVarPushSorter(FileFactory& pFactory, uint64_t pMaxBufItems);

private:
    void flush();

    FileFactory& mFactory;
    const std::string mBase;
    const uint64_t mMaxBufItems;
    std::vector<FileFactory::TmpFileHolderPtr> mFiles;
    uint64_t mFileNum;
    std::vector<T> mBuf;
    std::vector<uint64_t> mPerm;
};

#include "ExternalVarPushSorter.tcc"

#endif // EXTERNALVARPUSHSORTER_HH
