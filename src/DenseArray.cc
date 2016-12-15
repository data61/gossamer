// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "DenseArray.hh"

#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace std;


constexpr uint64_t DenseSelect::version;


inline
DenseSelect::Header::Header(const bool pInvertSense)
    : version(DenseSelect::version),
      indexArrayOffset(0), rankArrayOffset(0),
      logBlockSize(sLogDefBlockSize), blockSize(sDefBlockSize),
      logSampleRate(sLogDefSampleRate), sampleRate(sDefSampleRate),
      numBlocks(0), indexSize(0),
      smallBlocks(0), smallBlocksSize(0),
      intermediateBlocks(0), intermediateBlocksSize(0),
      largeBlocks(0), largeBlocksSize(0)
{
    flags.reset();
    flags[kInvertSense] = pInvertSense;
}


DenseSelect::DenseSelect(const WordyBitVector& pBitVector,
            const string& pBaseName, FileFactory& pFactory,
            bool pInvertSense)
    : mBitVector(pBitVector),
      mFileHolder(pFactory.map(pBaseName)),
      mHeader(pInvertSense)
{
    mData = reinterpret_cast<const uint8_t*>(mFileHolder->data());
    mHeader = *reinterpret_cast<const Header*>(mData);

    if (mHeader.version != version)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
            << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(version, mHeader.version)));
    }

    // The header has a bunch of redundancy in it. Sanity check just
    // to be sure.
    if ((1ull << mHeader.logBlockSize) != mHeader.blockSize
        || (1ull << mHeader.logSampleRate) != mHeader.sampleRate
        || mHeader.smallBlocks + mHeader.intermediateBlocks
               + mHeader.largeBlocks != mHeader.numBlocks)
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("Corrupt DenseSelect index header"));
    }

    // Check that the sense matches.
    if (pInvertSense != mHeader.flags[Header::kInvertSense])
    {
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << Gossamer::general_error_info("DenseSelect index does not have the expected sense"));
    }

    // All other flags are reserved. If any are set, we don't understand
    // this index.
    for (uint64_t i = Header::kFirstUnusedFlag; i < Header::kLastFlag; ++i)
    {
        if (mHeader.flags[i])
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Reserved DenseSelect flag "
                        + lexical_cast<string>(i)
                        + " appears to be set. It is likely that this index "
                          "was produced with a more recent version of the "
                          "application."));
        }
    }

    mIndex = reinterpret_cast<const uint64_t*>(mData + mHeader.indexArrayOffset);
    mRank = reinterpret_cast<const uint64_t*>(mData + mHeader.rankArrayOffset);
}


PropertyTree
DenseSelect::stat() const
{
    PropertyTree t;
    uint64_t s = sizeof(Header);
    s +=  mFileHolder->size();
    t.putProp("storage", s);
    t.putProp("invertSense", mHeader.flags[Header::kInvertSense]);

    {
        PropertyTree index;
        index.putProp("entries", mHeader.numBlocks);
        index.putProp("size", mHeader.indexSize);
        t.putSub("index", index);
    }

    {
        PropertyTree small;
        small.putProp("entries", mHeader.smallBlocks);
        small.putProp("size", mHeader.smallBlocksSize);
        t.putSub("smallBlocks", small);
    }

    {
        PropertyTree intermediate;
        intermediate.putProp("entries", mHeader.intermediateBlocks);
        intermediate.putProp("size", mHeader.intermediateBlocksSize);
        t.putSub("intermediateBlocks", intermediate);
    }

    {
        PropertyTree large;
        large.putProp("entries", mHeader.largeBlocks);
        large.putProp("size", mHeader.largeBlocksSize);
        t.putSub("largeBlocks", large);
    }

    return t;
}


uint64_t
DenseSelect::lookupSubBlock(const uint8_t* pBlockStart, uint64_t pStartRank,
                            internal_pointer_t pSubBlock, uint64_t pI) const
{
    pBlockStart += pSubBlock & ~sBlockTypeMask;
    uint64_t r = pI & (mHeader.sampleRate - 1);

    if (!pSubBlock)
    {
        // Null pointer. This means that we fall back to bit scan.
        return mHeader.flags[Header::kInvertSense]
            ? mBitVector.select0(pStartRank, r) 
            : mBitVector.select1(pStartRank, r);
    }

    switch (static_cast<block_type_t>(pSubBlock & sBlockTypeMask))
    {
        case tFullSpill32:
        {
            const uint32_t* b = reinterpret_cast<const uint32_t*>(pBlockStart);
            return pStartRank + b[r];
        }

        case tFullSpill16:
        {
            const uint16_t* b = reinterpret_cast<const uint16_t*>(pBlockStart);
            return pStartRank + b[r];
        }

        case tFullSpill8:
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(pBlockStart);
            return pStartRank + b[r];
        }

        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Corrupt DenseSelect index (intermediate-level)"));
        }
    }
}


uint64_t
DenseSelect::select(uint64_t i) const
{
    uint64_t blockNum = i >> mHeader.logBlockSize;
    uint64_t startRank = mRank[blockNum];
    uint64_t il = mIndex[blockNum];
    const uint8_t* block = mData + (il & ~sBlockTypeMask);

    uint64_t subBlockOffset
        = (i & (mHeader.blockSize - 1)) >> mHeader.logSampleRate;

    switch (static_cast<block_type_t>(il & sBlockTypeMask))
    {
        case tSmall:
        {
            const uint16_t* b = reinterpret_cast<const uint16_t*>(block);
            startRank += b[subBlockOffset];
            uint64_t r = i & (mHeader.sampleRate - 1);
            return mHeader.flags[Header::kInvertSense]
                ? mBitVector.select0(startRank, r)
                : mBitVector.select1(startRank, r);
        }

        case tFullSpill64:
        {
            // Top-level FullSpill64 is NOT relative to startRank!
            // This is for entirely historical reasons.
            const uint64_t* b = reinterpret_cast<const uint64_t*>(block);
            return b[i & (mHeader.blockSize - 1)];
        }

        case tFullSpill32:
        {
            const uint32_t* b = reinterpret_cast<const uint32_t*>(block);
            return startRank + b[i & (mHeader.blockSize - 1)];
        }

        case tFullSpill16:
        {
            const uint16_t* b = reinterpret_cast<const uint16_t*>(block);
            return startRank + b[i & (mHeader.blockSize - 1)];
        }

        case tFullSpill8:
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(block);
            return startRank + b[i & (mHeader.blockSize - 1)];
        }

        case tIntermediate:
        {
            const uint32_t* b = reinterpret_cast<const uint32_t*>(block);
            const internal_pointer_t* sbs
                = reinterpret_cast<const internal_pointer_t*>(
                    block + (sizeof(*b) <<
                        (mHeader.logBlockSize - mHeader.logSampleRate))
                );
            return lookupSubBlock(block, startRank + b[subBlockOffset],
                                  sbs[subBlockOffset], i);
        }

        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Corrupt DenseSelect index (top-level)"));
        }
    }
}


pair<uint64_t,uint64_t>
DenseSelect::select(uint64_t i, uint64_t j) const
{
    BOOST_ASSERT(i < j);

    uint64_t idxi = i >> mHeader.logBlockSize;
    uint64_t idxj = j >> mHeader.logBlockSize;

    if (idxi != idxj)
    {
        // If the indexes fall in different blocks, we can't optimise.
        return make_pair(select(i), select(j));
    }

    uint64_t startRank = mRank[idxi];
    uint64_t index = mIndex[idxi];
    const uint8_t* block = mData + (index & ~sBlockTypeMask);

    uint64_t sbi = (i & (mHeader.blockSize - 1)) >> mHeader.logSampleRate;
    uint64_t sbj = (j & (mHeader.blockSize - 1)) >> mHeader.logSampleRate;

    internal_pointer_t subBlock = 0;

    switch (static_cast<block_type_t>(index & sBlockTypeMask))
    {
        case tSmall:
        {
            const uint16_t* b = reinterpret_cast<const uint16_t*>(block);

            uint64_t pi = startRank + b[sbi];
            uint64_t pj = startRank + b[sbj];
            uint64_t ri = i & (mHeader.sampleRate - 1);
            uint64_t rj = j & (mHeader.sampleRate - 1);

            uint64_t rnki
                = mHeader.flags[Header::kInvertSense]
                ? mBitVector.select0(pi, ri)
                : mBitVector.select1(pi, ri);

            uint64_t rnkj;
            if (pi != pj)
            {
                rnkj = mHeader.flags[Header::kInvertSense]
                      ? mBitVector.select0(pj, rj)
                      : mBitVector.select1(pj, rj);
            }
            else
            {
                rnkj = mHeader.flags[Header::kInvertSense]
                      ? mBitVector.select0(rnki, rj-ri)
                      : mBitVector.select1(rnki, rj-ri);
            }

            return make_pair(rnki, rnkj);
        }

        case tFullSpill64:
        {
            // Top-level FullSpill64 is NOT relative to startRank!
            // This is for entirely historical reasons.
            const uint64_t* b = reinterpret_cast<const uint64_t*>(block);
            return make_pair(b[i & (mHeader.blockSize - 1)],
                             b[j & (mHeader.blockSize - 1)]);;
        }

        case tFullSpill32:
        {
            const uint32_t* b = reinterpret_cast<const uint32_t*>(block);
            return make_pair(startRank + b[i & (mHeader.blockSize - 1)],
                             startRank + b[j & (mHeader.blockSize - 1)]);;
        }

        case tFullSpill16:
        {
            const uint16_t* b = reinterpret_cast<const uint16_t*>(block);
            return make_pair(startRank + b[i & (mHeader.blockSize - 1)],
                             startRank + b[j & (mHeader.blockSize - 1)]);;
        }

        case tFullSpill8:
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(block);
            return make_pair(startRank + b[i & (mHeader.blockSize - 1)],
                             startRank + b[j & (mHeader.blockSize - 1)]);;
        }

        case tIntermediate:
        {
            const uint32_t* b = reinterpret_cast<const uint32_t*>(block);
            const internal_pointer_t* sbs
                = reinterpret_cast<const internal_pointer_t*>(
                    block + (sizeof(*b) <<
                        (mHeader.logBlockSize - mHeader.logSampleRate))
                );

            if (sbi != sbj)
            {
                return make_pair(
                    lookupSubBlock(block, startRank + b[sbi], sbs[sbi], i),
                    lookupSubBlock(block, startRank + b[sbj], sbs[sbj], j));
            }

            startRank += b[sbi];
            subBlock = sbs[sbi];
            break;
        }

        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Corrupt DenseSelect index (top-level)"));
        }
    }

    if (!subBlock)
    {
        uint64_t ri = i & (mHeader.sampleRate - 1);
        uint64_t rj = j & (mHeader.sampleRate - 1);

        // Null pointer. This means that we fall back to bit scan.
        if (mHeader.flags[Header::kInvertSense])
        {
            uint64_t rnki = mBitVector.select0(startRank, ri);
            uint64_t rnkj = mBitVector.select0(rnki, rj-ri);
            return make_pair(rnki, rnkj);
        }
        else
        {
            uint64_t rnki = mBitVector.select1(startRank, ri);
            uint64_t rnkj = mBitVector.select1(rnki, rj-ri);
            return make_pair(rnki, rnkj);
        }
    }

    // Optimised version of lookupSubBlock for the case where we're
    // looking up two values.
    block += subBlock & ~sBlockTypeMask;

    switch (static_cast<block_type_t>(subBlock & sBlockTypeMask))
    {
        case tFullSpill32:
        {
            const uint32_t* b = reinterpret_cast<const uint32_t*>(block);
            return make_pair(
                startRank + b[i & (mHeader.sampleRate - 1)],
                startRank + b[j & (mHeader.sampleRate - 1)]);
        }

        case tFullSpill16:
        {
            const uint16_t* b = reinterpret_cast<const uint16_t*>(block);
            return make_pair(
                startRank + b[i & (mHeader.sampleRate - 1)],
                startRank + b[j & (mHeader.sampleRate - 1)]);
        }

        case tFullSpill8:
        {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(block);
            return make_pair(
                startRank + b[i & (mHeader.sampleRate - 1)],
                startRank + b[j & (mHeader.sampleRate - 1)]);
        }

        default:
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Corrupt DenseSelect index (intermediate-level)"));
        }
    }
}


inline
DenseIndexBuilderBase::DenseIndexBuilderBase(const string& pBaseName,
                                             FileFactory& pFactory)
    : mBaseName(pBaseName),
      mFileHolder(pFactory.out(pBaseName)),
      mFile(**mFileHolder)
{
}


inline void
DenseIndexBuilderBase::alignFilePos(uint64_t pMask)
{
    for (uint64_t pos = mFile.tellp(); (pos & pMask) != 0; ++pos)
    {
        mFile.put(0);
    }
}


void
DenseSelect::Builder::flush()
{
    if (mCurrBlock.size() == 0)
    {
        return;
    }

    Gossamer::ensureCapacity(mIndex);
    Gossamer::ensureCapacity(mRank);

    uint64_t fileposition = mFile.tellp();
    BOOST_ASSERT(!(fileposition & sBlockTypeMask));

    uint64_t pp = mCurrBlock.front();
    uint64_t p = mCurrBlock.back();
    uint64_t blockSize = p - pp;

    mRank.push_back(pp);
    if (blockSize >= sIntermediateBlock
        || mCurrBlock.size() < mHeader.blockSize)
    {
        // Large block, or last block.

        if (blockSize < (1ull << 32))
        {
            for (uint64_t i = 0; i < mCurrBlock.size(); ++i)
            {
                uint32_t pos = static_cast<uint32_t>(mCurrBlock[i] - pp);
                mFile.write(reinterpret_cast<const char*>(&pos), sizeof(pos));
                mHeader.largeBlocksSize += sizeof(pos);
            }
            mIndex.push_back(fileposition | tFullSpill32);
            ++mHeader.largeBlocks;
        }
        else
        {
            for (uint64_t i = 0; i < mCurrBlock.size(); ++i)
            {
                // Do not subtract pp here. tFullSpill64 must be absolute
                // for backwards compatibility reasons.
                uint64_t pos = mCurrBlock[i];
                mFile.write(reinterpret_cast<const char*>(&pos), sizeof(pos));
                mHeader.largeBlocksSize += sizeof(pos);
            }
            mIndex.push_back(fileposition | tFullSpill64);
            ++mHeader.largeBlocks;
        }
    }
    else if (blockSize >= sSmallBlock)
    {
        // Intermediate block.

        mSubRankStart.clear();
        mSubBlockRange.clear();
        mInternalPtr.clear();

        // Write out the intermediate block rank index.
        for (uint64_t is = 0; is < mCurrBlock.size(); is += mHeader.sampleRate)
        {
            mSubRankStart.push_back(mCurrBlock[is]);
            uint32_t subBlockRange
                = static_cast<uint32_t>(mCurrBlock[is + mHeader.sampleRate - 1]
                    - mCurrBlock[is]);
            mSubBlockRange.push_back(subBlockRange);

            uint32_t s = static_cast<uint32_t>(mCurrBlock[is] - pp);
            mFile.write(reinterpret_cast<const char*>(&s), sizeof(s));
            mHeader.intermediateBlocksSize += sizeof(s);
        }

        // Compute offset to where the sub-blocks start.
        uint64_t subBlockBase
            = mSubRankStart.size() * (sizeof(uint32_t)
                + sizeof(internal_pointer_t));
        subBlockBase = (subBlockBase + sBlockTypeMask) & ~sBlockTypeMask;

        // Choose what kinds of sub-blocks, and write their pointers.
        for (uint64_t i = 0; i < mSubRankStart.size(); ++i)
        {
            internal_pointer_t internalPtr = 0;

            if (mSubBlockRange[i] <= (mHeader.blockSize >> mHeader.logSampleRate))
            {
                // Small enough to use bit scanning.
                internalPtr = tSmall;
            }
            else if (mSubBlockRange[i] < (1ull << 8))
            {
                internalPtr = ((internal_pointer_t)subBlockBase) | tFullSpill8;
                subBlockBase += mHeader.sampleRate * sizeof(uint8_t);
            }
            else if (mSubBlockRange[i] < (1ull << 16))
            {
                internalPtr = ((internal_pointer_t)subBlockBase) | tFullSpill16;
                subBlockBase += mHeader.sampleRate * sizeof(uint16_t);
            }
            else if (mSubBlockRange[i] < (1ull << 32))
            {
                internalPtr = ((internal_pointer_t)subBlockBase) | tFullSpill32;
                subBlockBase += mHeader.sampleRate * sizeof(uint32_t);
            }
            else
            {
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << Gossamer::general_error_info("Intermediate block too large"));
            }
            mInternalPtr.push_back(internalPtr);
            mFile.write(reinterpret_cast<const char*>(&internalPtr),
                    sizeof(internalPtr));
            mHeader.intermediateBlocksSize += sizeof(internalPtr);
            subBlockBase = (subBlockBase + sBlockTypeMask) & ~sBlockTypeMask;
        }

        if (subBlockBase > (1ull << numeric_limits<internal_pointer_t>::digits))
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("Intermediate sub-blocks too large"));
        }

        // Write out the sub-blocks.
        for (uint64_t i = 0; i < mInternalPtr.size(); ++i)
        {
            if (!mInternalPtr[i])
            {
                continue;
            }

            uint64_t startRank = mSubRankStart[i];

            alignFilePos(sBlockTypeMask);

#if 0
            if ((uint64_t)(mFile.tellp() - fileposition)
                 != (uint64_t)(mInternalPtr[i] & ~sBlockTypeMask))
            {
                cerr << "Expected block offset " <<
                    (uint64_t)(mInternalPtr[i] & ~sBlockTypeMask) << '\n';
                cerr << "Found " <<
                    (uint64_t)(mFile.tellp() - fileposition) << '\n';
            }
#endif

            switch (mInternalPtr[i] & sBlockTypeMask)
            {
                case tFullSpill8:
                {
                    for (uint64_t j = i << mHeader.logSampleRate;
                         j < (i+1) << mHeader.logSampleRate; ++j)
                    {
                        uint8_t s = static_cast<uint8_t>(mCurrBlock[j] - startRank);
                        mFile.write(reinterpret_cast<const char*>(&s), sizeof(s));
                        mHeader.intermediateBlocksSize += sizeof(s);
                    }
                    break;
                }
                case tFullSpill16:
                {
                    for (uint64_t j = i << mHeader.logSampleRate;
                         j < (i+1) << mHeader.logSampleRate; ++j)
                    {
                        uint16_t s = static_cast<uint16_t>(mCurrBlock[j] - startRank);
                        mFile.write(reinterpret_cast<const char*>(&s), sizeof(s));
                        mHeader.intermediateBlocksSize += sizeof(s);
                    }
                    break;
                }
                case tFullSpill32:
                {
                    for (uint64_t j = i << mHeader.logSampleRate;
                         j < (i+1) << mHeader.logSampleRate; ++j)
                    {
                        uint32_t s = static_cast<uint32_t>(mCurrBlock[j] - startRank);
                        mFile.write(reinterpret_cast<const char*>(&s), sizeof(s));
                        mHeader.intermediateBlocksSize += sizeof(s);
                    }
                    break;
                }
            }
        }

        mIndex.push_back(fileposition | tIntermediate);
        ++mHeader.intermediateBlocks;
    }
    else
    {
        // Small block.
        for (uint64_t is = 0; is < mCurrBlock.size(); is += mHeader.sampleRate)
        {
            uint16_t s = mCurrBlock[is] - pp;
            mFile.write(reinterpret_cast<const char*>(&s), sizeof(s));
            mHeader.smallBlocksSize += sizeof(s);
        }
        mIndex.push_back(fileposition | tSmall);
        ++mHeader.smallBlocks;
    }
    mCurrBlock.clear();
    alignFilePos(sBlockTypeMask);
    ++mHeader.numBlocks;
}


void
DenseSelect::Builder::end()
{
    flush();

    alignFilePos((1ull << 4) - 1);

    mHeader.indexArrayOffset = mFile.tellp();
    if( mIndex.size() != 0 )
    {
        mFile.write(reinterpret_cast<const char*>(&mIndex[0]),
                    mIndex.size() * sizeof(mIndex[0]));
        mHeader.indexSize += mIndex.size() * sizeof(mIndex[0]);
    }

    mHeader.rankArrayOffset = mFile.tellp();
    if( mRank.size() != 0 )
    {
        mFile.write(reinterpret_cast<const char*>(&mRank[0]),
                    mRank.size() * sizeof(mRank[0]));
        mHeader.indexSize += mRank.size() * sizeof(mRank[0]);
    }

    mFile.seekp(0, ios_base::beg);
    mFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(Header));    
}


DenseSelect::Builder::Builder(const string& pBaseName,
                              FileFactory& pFactory, bool pInvertSense)
    : DenseIndexBuilderBase(pBaseName, pFactory),
      mHeader(pInvertSense)
{
    static const uint64_t MAX_HEADER_SIZE = 4096;

    BOOST_STATIC_ASSERT(sizeof(Header) < MAX_HEADER_SIZE);
    mFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(Header));    

    alignFilePos(MAX_HEADER_SIZE - 1);

    mCurrBlock.reserve(mHeader.blockSize);
    mSubRankStart.reserve(mHeader.blockSize >> mHeader.logSampleRate);
    mSubBlockRange.reserve(mHeader.blockSize >> mHeader.logSampleRate);
    mInternalPtr.reserve(mHeader.blockSize >> mHeader.logSampleRate);
}


DenseRank::Header::Header()
    : version(DenseRank::version), size(0), count(0)
{
}


DenseRank::Header::Header(const string& pFileName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr headerFileHolder(pFactory.in(pFileName));
    istream& headerFile(**headerFileHolder);
    headerFile.read(reinterpret_cast<char*>(this), sizeof(Header));
    if (version != DenseRank::version)
    {
        uint64_t v = DenseRank::version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(version, v)));
    }
}


void
DenseRank::Builder::end(uint64_t pEndPos)
{
    flush();

    uint64_t lastLargeBlock = pEndPos
        = (pEndPos + sLargeBlockSize - 1) / sLargeBlockSize;
    while (mCurrLargeBlock < lastLargeBlock)
    {
        flush();
    }

    alignFilePos((1ull << 4) - 1);

    mHeader.largeBlockArrayOffset = mFile.tellp();
    mFile.write(reinterpret_cast<const char*>(&mLargeBlockArray[0]),
                mLargeBlockArray.size() * sizeof(mLargeBlockArray[0]));

    mHeader.size = pEndPos;
    mHeader.count = mLastLargeBlockRank;

    mFile.seekp(0, ios_base::beg);
    mFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(Header));    
}


DenseRank::Builder::Builder(const string& pBaseName, FileFactory& pFactory)
    : DenseIndexBuilderBase(pBaseName, pFactory)
{
    static const uint64_t MAX_HEADER_SIZE = 4096; // MUST be a power of 2.
    BOOST_STATIC_ASSERT(sizeof(Header) < MAX_HEADER_SIZE);

    mFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(Header));    

    alignFilePos(MAX_HEADER_SIZE - 1);
    mHeader.smallBlockArrayOffset = mFile.tellp();
    mCurrLargeBlock = 0ull;
    mLastLargeBlockRank = 0ull;
    mLargeBlockArray.push_back(mLastLargeBlockRank);
    mSmallBlockArray.resize(sLargeBlockSize / sSmallBlockSize);
}


void
DenseRank::Builder::flush()
{
    uint16_t smallBlockRank = 0;
    for (vector<uint16_t>::const_iterator ii = mSmallBlockArray.begin();
         ii != mSmallBlockArray.end(); ++ii)
    {
        mFile.write(reinterpret_cast<const char*>(&smallBlockRank),
                    sizeof(smallBlockRank));
        smallBlockRank += *ii;
    }
    mLastLargeBlockRank += smallBlockRank;
    mSmallBlockArray.clear();
    mSmallBlockArray.resize(sLargeBlockSize / sSmallBlockSize);
    mLargeBlockArray.push_back(mLastLargeBlockRank);
    ++mCurrLargeBlock;
}


DenseRank::DenseRank(const WordyBitVector& pBitVector,
                     const string& pBaseName, FileFactory& pFactory)
    : mBitVector(pBitVector),
      mFileHolder(pFactory.map(pBaseName))
{
    mData = reinterpret_cast<const uint8_t*>(mFileHolder->data());
    mHeader = *reinterpret_cast<const Header*>(mData);
    mSmallBlockArray = reinterpret_cast<const uint16_t*>(mData + mHeader.smallBlockArrayOffset);
    mLargeBlockArray = reinterpret_cast<const uint64_t*>(mData + mHeader.largeBlockArrayOffset);
}


DenseArray::Header::Header()
    : version(DenseArray::version), size(0), count(0)
{
}


DenseArray::Header::Header(const string& pFileName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr headerFileHolder(pFactory.in(pFileName));
    istream& headerFile(**headerFileHolder);
    headerFile.read(reinterpret_cast<char*>(this), sizeof(Header));
    if (version != DenseArray::version)
    {
        uint64_t v = DenseArray::version;
        BOOST_THROW_EXCEPTION(
            Gossamer::error()
                << boost::errinfo_file_name(pFileName)
                << Gossamer::version_mismatch_info(pair<uint64_t,uint64_t>(version, v)));
    }
}


DenseArray::Builder::Builder(const string& pBaseName, FileFactory& pFactory)
    : mHeader(),
      mBitsFile(pBaseName + ".bits", pFactory),
      mSelectIndexFile(pBaseName + ".select", pFactory, false),
      mRankIndexFile(pBaseName + ".rank", pFactory),
      mHeaderFileHolder(pFactory.out(pBaseName + ".header")),
      mHeaderFile(**mHeaderFileHolder)
{
}


void
DenseArray::Builder::end(DenseArray::position_type pEnd)
{
    mBitsFile.pad(pEnd);
    mBitsFile.end();
    mSelectIndexFile.end();
    mRankIndexFile.end(pEnd);
    mHeader.size = pEnd;
    mHeaderFile.write(reinterpret_cast<const char*>(&mHeader), sizeof(mHeader));
}


PropertyTree
DenseArray::stat() const
{
    PropertyTree t;

    t.putSub("bits", mBits.stat());
    t.putSub("rank", mRankIndex.stat());
    t.putSub("select", mSelectIndex.stat());

    t.putProp("size", size());
    t.putProp("count", count());

    uint64_t s = sizeof(Header);
    s += t("bits").as<uint64_t>("storage");
    s += t("rank").as<uint64_t>("storage");
    s += t("select").as<uint64_t>("storage");
    t.putProp("storage", s);

    return t;
}


void
DenseArray::remove(const string& pBaseName, FileFactory& pFactory)
{
    pFactory.remove(pBaseName + ".header");
    pFactory.remove(pBaseName + ".bits");
    pFactory.remove(pBaseName + ".select");
    pFactory.remove(pBaseName + ".rank");
}


DenseArray::DenseArray(const string& pBaseName, FileFactory& pFactory)
    : mHeader(pBaseName + ".header", pFactory),
      mBits(pBaseName + ".bits", pFactory),
      mSelectIndex(mBits, pBaseName + ".select", pFactory, false),
      mRankIndex(mBits, pBaseName + ".rank", pFactory)
{
}


DenseArray::~DenseArray()
{
}


