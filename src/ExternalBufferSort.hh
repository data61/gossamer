// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef EXTERNALBUFFERSORT_HH
#define EXTERNALBUFFERSORT_HH

#ifndef FILEFACTORY_HH
#include "FileFactory.hh"
#endif

#ifndef MAPPEDARRAY_HH
#include "MappedArray.hh"
#endif

#ifndef TRIVIALVECTOR_HH
#include "TrivialVector.hh"
#endif

#ifndef VBYTECODEC_HH
#include "VByteCodec.hh"
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

class ExternalBufferSort
{
public:
    static const uint64_t Radix = 256;
    
    class Cmp
    {
    public:
        bool operator()(uint8_t const* pLhs, uint8_t const* pRhs)
        {
            uint8_t const* lp = pLhs;
            uint64_t l = VByteCodec::decode(lp);
            uint8_t const* rp = pRhs;
            uint64_t r = VByteCodec::decode(rp);
            uint64_t i = 0;
            uint64_t j = 0;
            while (i < l && j < r)
            {
                if (*lp != *rp)
                {
                    return *lp < *rp;
                }
                ++i; ++j;
                ++lp; ++rp;
            }
            return l < r;
        }
    };

    class InAdapter
    {
    public:
        uint8_t operator*()
        {
            int c = mFile.peek();
            return (c != EOF ? c : 0);
        }

        void operator++()
        {
            mFile.get();
            return;
        }

        InAdapter(std::istream& pFile)
            : mFile(pFile)
        {
        }

    private:
        std::istream& mFile;
    };

    class BufferedFile
    {
    public:
        template <typename Vec>
        void push_back(const Vec& pItem)
        {
            TrivialVector<uint8_t,16> tmp;
            VByteCodec::encode(pItem.size(), tmp);
            uint64_t z = tmp.size() + pItem.size();
            if (mBuffer.size() + z > mBufferSize)
            {
                flush();
            }
            mBuffer.insert(mBuffer.end(), tmp.begin(), tmp.end());
            mBuffer.insert(mBuffer.end(), pItem.begin(), pItem.end());
        }

        void flush()
        {
            if (mBuffer.empty())
            {
                return;
            }
            //std::cerr << "flushing " << mFileName << std::endl;

            FileFactory::OutHolderPtr outp(mFactory.out(mFileName, FileFactory::AppendMode));
            std::ostream& out(**outp);
            out.write(reinterpret_cast<const char*>(&mBuffer[0]), mBuffer.size());
            mBuffer.clear();
        }

        BufferedFile(uint64_t pBufferSize, const std::string& pFileName, FileFactory& pFactory)
            : mBufferSize(pBufferSize), mFileName(pFileName), mFactory(pFactory)
        {
        }

    private:
        const uint64_t mBufferSize;
        const std::string mFileName;
        FileFactory& mFactory;
        std::vector<uint8_t> mBuffer;
    };
    typedef boost::shared_ptr<BufferedFile> BufferedFilePtr;

    template <typename Vec>
    void push_back(const Vec& pItem)
    {
        mRoot->push_back(pItem);
    }

    template <typename Dest>
    void sort(Dest& pDest)
    {
        mRoot->flush();
        mRoot = BufferedFilePtr();
        sort(mFileName, 0, pDest);
        pDest.end();
    }

    ExternalBufferSort(uint64_t pBufferSize, FileFactory& pFactory)
        : mFactory(pFactory), mBufferSize(pBufferSize), mFileName(pFactory.tmpName()),
          mRoot(new BufferedFile(mBufferSize, mFileName, mFactory))
    {
    }

private:

    template <typename Dest>
    void sort(const std::string& pFileName, uint64_t pDepth, Dest& pDest)
    {
        if (!mFactory.exists(pFileName))
        {
            return;
        }

        if (mFactory.size(pFileName) <= mBufferSize)
        {
            {
                MappedArray<uint8_t> a(pFileName, mFactory);
                std::vector<uint8_t const*> perm;
                for (uint8_t const* p = a.begin(); p != a.end();)
                {
                    perm.push_back(p);
                    uint64_t z = VByteCodec::decode(p, a.end());
                    p += z;
                }

                std::sort(perm.begin(), perm.end(), Cmp());
                std::vector<uint8_t> itm;
                for (uint64_t i = 0; i < perm.size(); ++i)
                {
                    uint8_t const * p = perm[i];
                    uint64_t z = VByteCodec::decode(p);
                    itm.resize(z);
                    for (uint64_t j = 0; j < z; ++j)
                    {
                        itm[j] = *p++;
                    }
                    pDest.push_back(itm);
                }
            }
            mFactory.remove(pFileName);
            return;
        }

        //std::cerr << "recursing on " << pFileName << std::endl;

        std::vector<BufferedFilePtr> kids;
        std::vector<uint8_t> used(Radix, false);
        for (uint64_t i = 0; i < Radix; ++i)
        {
            std::string fn = pFileName + "-" + boost::lexical_cast<std::string>(i);
            kids.push_back(BufferedFilePtr(new BufferedFile(mBufferSize / Radix, fn, mFactory)));
        }

        {
            FileFactory::InHolderPtr inp(mFactory.in(pFileName));
            std::istream& in(**inp);
            InAdapter itr(in);
            std::vector<uint8_t> item;
            while (in.good())
            {
                uint64_t z = VByteCodec::decode(itr);
                if (!in.good())
                {
                    break;
                }
                item.resize(z);
                if( z != 0 ) {
                    // if z is zero, &item[0] throws an out of range exception
                    // in debug mode.
                    in.read(reinterpret_cast<char*>(&item[0]), z);
                }
                if (item.size() == pDepth)
                {
                    pDest.push_back(item);
                }
                else
                {
                    kids[item[pDepth]]->push_back(item);
                    used[item[pDepth]] = true;
                }
            }
        }

        for (uint64_t i = 0; i < Radix; ++i)
        {
            kids[i]->flush();
        }
        kids.clear();

        mFactory.remove(pFileName);

        for (uint64_t i = 0; i < Radix; ++i)
        {
            if (!used[i])
            {
                continue;
            }
            std::string fn = pFileName + "-" + boost::lexical_cast<std::string>(i);
            sort(fn, pDepth + 1, pDest);
        }
    }

    FileFactory& mFactory;
    const uint64_t mBufferSize;
    const std::string mFileName;
    BufferedFilePtr mRoot;
};

#endif // EXTERNALBUFFERSORT_HH
