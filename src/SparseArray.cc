// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "SparseArray.hh"

SparseArray::Header::Header(uint64_t pD)
    : version(SparseArray::version), D(pD), quantizedD(8 * ((pD + 7) / 8)), DMask((position_type(1) << D) - 1),
      size(0), count(0)
{
}


SparseArray::Header::Header(const std::string& pFileName, FileFactory& pFactory)
    : size(0), count(0)
{
    FileFactory::InHolderPtr headerFileHolder(pFactory.in(pFileName));
    std::istream& headerFile(**headerFileHolder);
    headerFile.read(reinterpret_cast<char*>(this), sizeof(Header));
    if (version != SparseArray::version)
    {
        uint64_t v = SparseArray::version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << Gossamer::version_mismatch_info(std::pair<uint64_t,uint64_t>(version, v)));
    }
}


SparseArray::Iterator::Iterator(const SparseArray& pArray)
    : mArray(&pArray), mHiItr(mArray->mHighBits.iterator1()),
      mI(0), mValid(true)
{
    if (!pArray.count())
    {
        mValid = false;
        return;
    }
}


uint64_t
SparseArray::Builder::d(const position_type& pN, rank_type pM)
{
    double n = pN.asDouble();
    double m = pM;
    double d0 = log2(n/((1 + m) * 1.4426950408889634));
    uint64_t d = ceil(d0);
    if (d < 8)
    {
        d = 8;
    }
    else if (d > 128)
    {
        d = 128;
    }
#if 0
    else
    {
        while (Gossamer::popcnt(d) > 2)
        {
            d += 8;
        }
    }
#endif
    return d;
}


void
SparseArray::Builder::end(const position_type& pN)
{
    mHeader.size = pN;
    position_type nd = pN >> mHeader.D;
    if (!nd.fitsIn64Bits())
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("Internal error in SparseArray; nd = "
                                                + boost::lexical_cast<std::string>(nd)));
    }

    // Make sure there is a zero for every possible
    // value of i >> D.
    rank_type h = nd.asUInt64() + mHeader.count + 2;
    while (mLastHighBit < h)
    {
        mD0File.push_back(mLastHighBit);
        ++mLastHighBit;
    }
    mHighBitsFile.pad(mLastHighBit);

    mHighBitsFile.end();
    mLowBitsFile.end();
    mD0File.end();
    mD1File.end();
    mHeaderFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(mHeader));
}


SparseArray::Builder::Builder(const std::string& pBaseName,
                              FileFactory& pFactory, const position_type& pN, rank_type pM)
    : mHeader(d(pN, pM)), mBitNum(0), mLastHighBit(0),
      mHighBitsFile(pBaseName + ".high-bits", pFactory),
      mD0File(pBaseName + "-d0", pFactory, true),
      mD1File(pBaseName + "-d1", pFactory, false),
      mLowBitsFileHolder(IntegerArray::builder(mHeader.quantizedD, pBaseName + ".low-bits", pFactory)),
      mLowBitsFile(*mLowBitsFileHolder),
      mHeaderFileHolder(pFactory.out(pBaseName + ".header")),
      mHeaderFile(**mHeaderFileHolder)
{
}


SparseArray::Builder::Builder(const std::string& pBaseName, FileFactory& pFactory,
                              uint64_t pD)
    : mHeader(pD), mBitNum(0), mLastHighBit(0),
      mHighBitsFile(pBaseName + ".high-bits", pFactory),
      mD0File(pBaseName + "-d0", pFactory, true),
      mD1File(pBaseName + "-d1", pFactory, false),
      mLowBitsFileHolder(IntegerArray::builder(mHeader.quantizedD, pBaseName + ".low-bits", pFactory)),
      mLowBitsFile(*mLowBitsFileHolder),
      mHeaderFileHolder(pFactory.out(pBaseName + ".header")),
      mHeaderFile(**mHeaderFileHolder)
{
}

SparseArray::LazyIterator::LazyIterator(const std::string& pBaseName, FileFactory& pFactory)
    : mHeader(pBaseName + ".header", pFactory),
      mHiItr(WordyBitVector::lazyIterator1(pBaseName + ".high-bits", pFactory)),
      mLowBitsIteratorHolder(IntegerArray::lazyIterator(mHeader.quantizedD, pBaseName + ".low-bits", pFactory)),
      mLowBitsIterator(*mLowBitsIteratorHolder),
      mI(0)
{
}

PropertyTree
SparseArray::stat() const
{
    PropertyTree t;
    t.putSub("high-bits", mHighBits.stat());
    t.putSub("low-bits", mLowBits.stat());
    t.putSub("D0", mD0.stat());
    t.putSub("D1", mD1.stat());

    t.putProp("size", size());
    t.putProp("count", count());

    uint64_t s = sizeof(Header);
    s += t("high-bits").as<uint64_t>("storage");
    s += t("low-bits").as<uint64_t>("storage");
    s += t("D0").as<uint64_t>("storage");
    s += t("D1").as<uint64_t>("storage");
    t.putProp("storage", s);

    return t;
}

void
SparseArray::remove(const std::string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    pFactory.remove(pBaseName + ".high-bits");
    pFactory.remove(pBaseName + "-d0");
    pFactory.remove(pBaseName + "-d1");
    IntegerArray::remove(pBaseName + ".low-bits", pFactory);
}


SparseArray::SparseArray(const std::string& pBaseName, FileFactory& pFactory)
    : mHeader(pBaseName + ".header", pFactory),
      mHighBits(pBaseName + ".high-bits", pFactory),
      mD0(mHighBits, pBaseName + "-d0", pFactory, true),
      mD1(mHighBits, pBaseName + "-d1", pFactory, false),
      mLowBitsHolder(IntegerArray::create(mHeader.quantizedD, pBaseName + ".low-bits", pFactory)),
      mLowBits(*mLowBitsHolder)
{
    FileFactory::InHolderPtr headerFileHolder(pFactory.in(pBaseName + ".header"));
    std::istream& headerFile(**headerFileHolder);
    headerFile.read(reinterpret_cast<char*>(&mHeader), sizeof(Header));
    if (mHeader.version != version)
    {
        uint64_t v = version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pBaseName)
                << Gossamer::version_mismatch_info(std::pair<uint64_t,uint64_t>(mHeader.version, v)));
    }
}


SparseArray::~SparseArray()
{
}


