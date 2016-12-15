// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "RRRArray.hh"
#include <iostream>

using namespace std;

EnumerativeCode<RRRBase::U> RRRBase::sEnumCode;

RRRRank::Header::Header(const string& pFileName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr headerFileHolder(pFactory.in(pFileName));
    istream& headerFile(**headerFileHolder);
    headerFile.read(reinterpret_cast<char*>(this), sizeof(Header));
    if (version != RRRRank::version)
    {
        uint64_t v = RRRRank::version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(version, v)));
    }
}

void
RRRRank::Builder::end(uint64_t pN)
{
    uint64_t blkNum = pN / U;
    if (blkNum != mCurrBlkNum)
    {
        flush();
        mCurrBlkNum = blkNum;
        mCurrBlk = 0;
    }

    flush();
    mHeader.size = pN;

    {
        FileFactory::OutHolderPtr headerFileHolder(mFactory.out(mBaseName + ".header"));
        ostream& headerFile(**headerFileHolder);
        headerFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(Header));
    }

    mClassSumBuilder.end();
    mOffsetSumBuilder.end();
    mClassListBuilder.end();
    mOffsetListBuilder.end();
}


RRRRank::Builder::Builder(const string& pBaseName, FileFactory& pFactory)
    : mBaseName(pBaseName), mFactory(pFactory),
      mCurrFileBlkNum(0), mCurrBlk(0), mCurrBlkNum(0),
      mClassSum(0), mOffsetSum(0), mHeader(),
      mClassSumBuilder(pBaseName + ".class-sum", pFactory),
      mOffsetSumBuilder(pBaseName + ".offset-sum", pFactory),
      mClassListBuilder(pBaseName + ".classes", pFactory),
      mOffsetListBuilder(pBaseName + ".offsets", pFactory)
{
}


RRRRank::Builder::Builder(const string& pBaseName, FileFactory& pFactory, uint64_t pUnused)
    : mBaseName(pBaseName), mFactory(pFactory),
      mCurrFileBlkNum(0), mCurrBlk(0), mCurrBlkNum(0),
      mClassSum(0), mOffsetSum(0),
      mClassSumBuilder(pBaseName + ".class-sum", pFactory),
      mOffsetSumBuilder(pBaseName + ".offset-sum", pFactory),
      mClassListBuilder(pBaseName + ".classes", pFactory),
      mOffsetListBuilder(pBaseName + ".offsets", pFactory)
{
}


void
RRRRank::Builder::flush()
{
    while (mCurrFileBlkNum < mCurrBlkNum)
    {
        if (mCurrFileBlkNum % K == 0)
        {
            mClassSumBuilder.push_back(mClassSum);
            mOffsetSumBuilder.push_back(mOffsetSum);
        }
        mClassListBuilder.push_back(0);
        // mOffsetListBuilder.push_back(0, 0);

        // mClassSum += 0;
        // mOffsetSum += 0;

        mCurrFileBlkNum++;
    }

    if (mCurrFileBlkNum % K == 0)
    {
        mClassSumBuilder.push_back(mClassSum);
        mOffsetSumBuilder.push_back(mOffsetSum);
    }

    uint64_t c = Gossamer::popcnt(mCurrBlk);
    uint64_t b = RRRRank::sEnumCode.numCodeBits(c);
    uint64_t o = RRRRank::sEnumCode.encode(c, mCurrBlk);

    mClassListBuilder.push_back(c);
    mOffsetListBuilder.push_back(o, b);

    mClassSum += c;
    mOffsetSum += b;

    mCurrFileBlkNum++;
}

PropertyTree
RRRRank::stat() const
{
    PropertyTree t;
    t.putSub("classes", mClassList.stat());
    t.putSub("class-sum", mClassSum.stat());
    t.putSub("offsets", mOffsetList.stat());
    t.putSub("offset-sum", mOffsetSum.stat());

    t.putProp("size", size());
    t.putProp("count", count());

    uint64_t s = sizeof(Header);
    s += t("classes").as<uint64_t>("storage");
    s += t("offsets").as<uint64_t>("storage");
    s += t("class-sum").as<uint64_t>("storage");
    s += t("offset-sum").as<uint64_t>("storage");
    t.putProp("storage", s);

    return t;
}

void
RRRRank::remove(const std::string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    pFactory.remove(pBaseName + ".class-sum");
    pFactory.remove(pBaseName + ".offset-sum");
    pFactory.remove(pBaseName + ".classes");
    pFactory.remove(pBaseName + ".offsets");
}

RRRRank::RRRRank(const string& pBaseName, FileFactory& pFactory)
    : mHeader(pBaseName + ".header", pFactory),
      mClassSum(pBaseName + ".class-sum", pFactory),
      mOffsetSum(pBaseName + ".offset-sum", pFactory),
      mClassList(pBaseName + ".classes", pFactory),
      mOffsetList(pBaseName + ".offsets", pFactory)
{
}


RRRArray::Header::Header(const string& pFileName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr headerFileHolder(pFactory.in(pFileName));
    istream& headerFile(**headerFileHolder);
    headerFile.read(reinterpret_cast<char*>(this), sizeof(Header));
    if (version != RRRArray::version)
    {
        uint64_t v = RRRArray::version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(version, v)));
    }
}

void
RRRArray::Builder::end(uint64_t pN)
{
    uint64_t blkNum = mHeader.size / U;
    uint64_t emptyBlocksSinceLast = blkNum - mPrevBlkNum - 1;
    mEmptyBlks += emptyBlocksSinceLast;


    mHeader.size = pN;
    {
        FileFactory::OutHolderPtr headerFileHolder(mFactory.out(mBaseName + ".header"));
        ostream& headerFile(**headerFileHolder);
        headerFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(Header));
    }

    mRankBuilder.end(pN);
    // mRankPBuilder.end(blkNum + 1);
    mRankQBuilder.end(mBitNum + 1);
    mRankRBuilder.end(blkNum - mEmptyBlks + 1);
    mClumpArrayBuilder.end();
}


RRRArray::Builder::Builder(const string& pBaseName, FileFactory& pFactory)
    : mBaseName(pBaseName), mFactory(pFactory),
      mStart(true), mBitNum(0), mPrevBlkNum(~0ull), mEmptyBlks(0),
      mRankBuilder(pBaseName + ".rnk", pFactory),
      // mRankPBuilder(pBaseName + ".p", pFactory),
      mRankQBuilder(pBaseName + ".q", pFactory),
      mRankRBuilder(pBaseName + ".r", pFactory),
      mClumpArrayBuilder(pBaseName + ".clump", pFactory)
{
}


RRRArray::Builder::Builder(const string& pBaseName, FileFactory& pFactory, uint64_t pUnused)
    : mBaseName(pBaseName), mFactory(pFactory),
      mStart(true), mBitNum(0), mPrevBlkNum(~0ull), mEmptyBlks(0),
      mRankBuilder(pBaseName + ".rnk", pFactory),
      // mRankPBuilder(pBaseName + ".p", pFactory),
      mRankQBuilder(pBaseName + ".q", pFactory),
      mRankRBuilder(pBaseName + ".r", pFactory),
      mClumpArrayBuilder(pBaseName + ".clump", pFactory)
{
}


PropertyTree
RRRArray::stat() const
{
    PropertyTree t;
    t.putSub("rank", mRank.stat());
    t.putSub("rank-q", mRankQ.stat());
    t.putSub("rank-r", mRankR.stat());
    t.putSub("clumps", mClumpArray.stat());

    t.putProp("size", size());
    t.putProp("count", count());

    uint64_t s = sizeof(Header);
    s += t("rank").as<uint64_t>("storage");
    s += t("rank-q").as<uint64_t>("storage");
    s += t("rank-r").as<uint64_t>("storage");
    s += t("clumps").as<uint64_t>("storage");
    t.putProp("storage", s);

    return t;
}


RRRArray::RRRArray(const string& pBaseName, FileFactory& pFactory)
    : mHeader(pBaseName + ".header", pFactory),
      mRank(pBaseName + ".rnk", pFactory),
      // mRankP(pBaseName + ".p", pFactory),
      mRankQ(pBaseName + ".q", pFactory),
      mRankR(pBaseName + ".r", pFactory),
      mClumpArray(pBaseName + ".clump", pFactory)
{
}


void
RRRRank::debug(uint64_t pN) const
{
    using namespace std;

    for (uint64_t i = 0; i < pN; ++i)
    {
        if (i % U == 0)
        {
            cerr << ' ';
        }
        cerr << (int)access(i);
    }
    cerr << '\n';
}


uint64_t
RRRRank::getBlock(const uint64_t& pBlkNum, uint64_t& pPrevRank) const
{
    uint64_t supBlkNum = pBlkNum / K;
    uint64_t bitSum = mClassSum[supBlkNum];
    uint64_t offSum = mOffsetSum[supBlkNum];
    for (uint64_t i = supBlkNum * K; i < pBlkNum; ++i)
    {
        uint64_t c = mClassList.get(i);
        bitSum += c;
        offSum += sEnumCode.numCodeBits(c);
    }

    uint64_t cls = mClassList.get(pBlkNum);
    uint64_t blkSz = sEnumCode.numCodeBits(cls);
    uint64_t blkOrd = mOffsetList.get(offSum, blkSz);

    pPrevRank = bitSum;

    uint64_t blk = sEnumCode.decode(cls, blkOrd);
    return blk;
}


#if 0
void
RRRArray::debug(uint64_t pN) const
{
    using namespace std;

    cerr << "U = " << U << '\n';
    cerr << "K = " << K << '\n';
    uint64_t blocks = (pN + U - 1)/U;
    cerr << "blocks = " << blocks << '\n';
    cerr << "N = " << pN << '\n';
    uint64_t oneBits = mRank.rank(pN);
    cerr << "M = " << oneBits << '\n';
    uint64_t nonEmptyBlocks = mRankP.rank(blocks);
    cerr << "nonEmptyBlocks = " << nonEmptyBlocks << '\n';
    uint64_t clumps = mRankR.rank(nonEmptyBlocks);
    cerr << "clumps = " << clumps << '\n';
    cerr << "A:\n";
    mRank.debug(pN);
    cerr << "P:\n";
    mRankP.debug(blocks);
    cerr << "Q:\n";
    mRankQ.debug(oneBits);
    cerr << "R:\n";
    mRankR.debug(nonEmptyBlocks);
    cerr << "Clumps:\n";
    for (uint64_t i = 0; i < clumps; ++i)
    {
        cerr << ' ' << mClumpArray[i];
    }
    cerr << '\n';
}
#endif

