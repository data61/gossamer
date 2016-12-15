// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdGroupReads.hh"

#include "BackgroundMultiConsumer.hh"
#include "LineSource.hh"
#include "Debug.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "LineParser.hh"
#include "MappedArray.hh"
#include "MappedFile.hh"
#include "ProgressMonitor.hh"
#include "ReadPairSequenceFileSequence.hh"
#include "ReadSequenceFileSequence.hh"
#include "Spinlock.hh"
#include "SimpleHashSet.hh"
#include "Timer.hh"

#include <iostream>
#include <map>

#include <utility>

using namespace boost;
using namespace std;

typedef vector<string> strings;

typedef std::tuple<mutex*, ostream*> Out;
typedef std::tuple<mutex*, ostream*, ostream*> Outs;

typedef GossReadSequence::Item  ReadItem;
typedef deque<ReadItem> ReadItems;

namespace // anonymous
{

    class ReadClassWriter
    {
        friend class ReadClassItr;

    public:
        
        class ReadClassItr
        {
        public:
            bool valid() const
            {
                return mItr.valid();
            }

            pair<uint64_t, uint8_t> operator*() const
            {
                pair<uint64_t, uint8_t> p;
                ReadClassWriter::unpack(*mItr, p.first, p.second);
                return p;
            }

            void operator++()
            {
                ++mItr;
            }

            ReadClassItr(const string& pBuffer, FileFactory& pFac)
                : mItr(pBuffer, pFac)
            {
            }

        private:
            MappedArray<uint64_t>::LazyIterator mItr;
        };

        void operator()(uint64_t pReadNum, uint8_t pClass)
        {
            if (pReadNum > 0x0fffffffffffffffULL)
            {
                throw "Too many reads!";
            }

            const uint64_t rec = pack(pReadNum, pClass);
            // cerr << "writing " << pReadNum << ", " << uint64_t(pClass) << " as " << rec << '\n';

            unique_lock<mutex> l(mMutex);
            if (mBufferTotal == mBufferMax)
            {
                nextBuffer();
            }
            (**mBufferPtr).write((const char*)&rec, 8);
            mBufferTotal += 1;
        }

        // Sort and merge all buffers, returning an iterator to the result.
        // Warning: don't invoke operator() after this.
        ReadClassItr mergeBuffers()
        {
            mBufferPtr.reset();

            // Sort
            for (uint64_t i = 0; i < mNumBuffers; ++i)
            {
                const string name = bufferName(i);
                mLog(info, "sorting " + name);
                MappedFile<uint64_t> file(name, true, MappedFile<uint64_t>::ReadWrite, MappedFile<uint64_t>::Shared);
                std::sort(file.begin(), file.end());
            }

            // Merge
            mLog(info, "merging");
            typedef MappedArray<uint64_t>::LazyIterator Itr;
            typedef std::shared_ptr<Itr> ItrPtr;
            vector<ItrPtr> curs;
            for (uint64_t i = 0; i < mNumBuffers; ++i)
            {
                curs.push_back(ItrPtr(new MappedArray<uint64_t>::LazyIterator(bufferName(i), mFac)));
            }

            {
                mDone = true;
                MappedArray<uint64_t>::Builder bld(bufferName(), mFac);
                for (uint64_t readNum = 0; !curs.empty(); ++readNum)
                {
                    // cerr << "readNum " << readNum << '\n';
                    bool good = false;
                    uint8_t blrg = 0;
                    for (vector<ItrPtr>::iterator i = curs.begin(); i != curs.end(); )
                    {
                        Itr& cur(**i);
                        if (!cur.valid())
                        {
                            i = curs.erase(i);
                            continue;
                        }
                        uint64_t curReadNum;
                        uint8_t curBlrg;
                        while (true)
                        {
                            unpack(*cur, curReadNum, curBlrg);
                            if (curReadNum != readNum)
                            {
                                ++i;
                                break;
                            }

                            blrg |= curBlrg;
                            good = true;
                            ++cur;
                            if (!cur.valid())
                            {
                                i = curs.erase(i);
                                break;
                            }
                        }
                    }
                    
                    if (good)
                    {
                        // cerr << "write\n";
                        bld.push_back(pack(readNum, blrg));
                    }
                }
                bld.end();
            }

            return ReadClassItr(bufferName(), mFac);
        }

        ReadClassWriter(Logger& pLog, FileFactory& pFac, uint64_t pBufferMax)
            : mLog(pLog), mFac(pFac), mMutex(), mTotal(0), mBufferTotal(0), mNumBuffers(0),
              mPrefix(mFac.tmpName()), mBufferMax(pBufferMax), mBufferPtr(), mDone(false)
        {
            nextBuffer();
        }

        ~ReadClassWriter()
        {
            for (uint64_t i = 0; i < mNumBuffers; ++i)
            {
                mFac.remove(bufferName(i));
            }
            if (mDone)
            {
                mFac.remove(bufferName());
            }
        }

    private:
        
        static uint64_t pack(uint64_t pReadNum, uint8_t pClass)
        {
            return pReadNum << 4 | pClass;
        }

        static void unpack(uint64_t pRec, uint64_t& pReadNum, uint8_t& pClass)
        {
            pReadNum = pRec >> 4;
            pClass = pRec & 0xf;
        }

        string bufferName() const
        {
            return mPrefix + "-classbuf";
        }

        string bufferName(uint64_t pNum) const
        {
            return bufferName() + "-" + lexical_cast<string>(pNum);
        }

        void nextBuffer()
        {
            mLog(info, "opening buffer " + lexical_cast<string>(mNumBuffers) + " " +
                       bufferName(mNumBuffers));
            mBufferPtr = mFac.out(bufferName(mNumBuffers));
            mNumBuffers += 1;
            mBufferTotal = 0;
        }

        Logger& mLog;
        FileFactory& mFac;
        mutex mMutex;
        uint64_t mTotal;
        uint64_t mBufferTotal;
        uint64_t mNumBuffers;
        const string mPrefix;
        const uint64_t mBufferMax;
        FileFactory::OutHolderPtr mBufferPtr;
        bool mDone;
    };

    class KmerSrc 
    {
    public:
        virtual bool valid() const = 0;

        virtual Gossamer::edge_type operator*() const = 0;

        virtual void operator++() = 0;

        virtual void print(uint8_t pClass) = 0;

        virtual void writeClass(uint8_t pBlrg) = 0;
    };

    class Read : public KmerSrc
    {
    public:

        bool valid() const
        {
            return mItr.valid();
        }

        Gossamer::edge_type operator*() const
        {
            BOOST_ASSERT(valid());
            return mItr.kmer();
        }

        void operator++()
        {
            BOOST_ASSERT(valid());
            ++mItr;
        }

        void print(uint8_t pClass)
        {
            print(*mRead, pClass, mOutputs);
        }

        static void print(const GossRead& pRead, uint8_t pClass, const vector<Out>& pOutputs)
        {
            if (pOutputs.size())
            {
                auto& output = pOutputs[pClass];
                std::unique_lock<std::mutex> l(*std::get<0>(output));
                pRead.print(*std::get<1>(output));
            }
        }

        void writeClass(uint8_t pBlrg)
        {
            mClassWriter(mNum, pBlrg);
        }

        Read(uint64_t pK, uint64_t pNum, GossReadPtr pRead, vector<Out>& pOutputs, ReadClassWriter& pClassWriter)
            : mK(pK), mNum(pNum), mRead(pRead), mItr(*mRead, mK), mOutputs(pOutputs), mClassWriter(pClassWriter)
        {
        }

    private:

        const uint64_t mK;
        const uint64_t mNum;
        const GossReadPtr mRead;
        GossRead::Iterator mItr;
        vector<Out>& mOutputs;
        ReadClassWriter& mClassWriter;
    };

    class Pair : public KmerSrc
    {
    public:

        bool valid() const
        {
            return mItr1.valid() || mItr2.valid();
        }

        Gossamer::edge_type operator*() const
        {
            BOOST_ASSERT(valid());
            return mItr1.valid() ? mItr1.kmer() : mItr2.kmer();
        }

        void operator++()
        {
            BOOST_ASSERT(valid());
            if (mItr1.valid())
            {
                ++mItr1;
                return;
            }
            ++mItr2;
        }

        void print(uint8_t pClass)
        {
            print(*mRead1, *mRead2, pClass, mOutputs);
        }

        static void print(const GossRead& pRead1, const GossRead& pRead2, uint8_t pClass, const vector<Outs>& pOutputs)
        {
            if (pOutputs.size())
            {
                auto& output = pOutputs[pClass];
                unique_lock<mutex> l(*std::get<0>(output));
                pRead1.print(*std::get<1>(output));
                pRead2.print(*std::get<2>(output));
            }
        }

        void writeClass(uint8_t pBlrg)
        {
            mClassWriter(mNum, pBlrg);
        }

        Pair(uint64_t pK, uint64_t pNum, GossReadPtr pRead1, GossReadPtr pRead2, vector<Outs>& pOutputs, 
             ReadClassWriter& pClassWriter)
            : mK(pK), mNum(pNum), mRead1(pRead1), mRead2(pRead2),
              mItr1(*mRead1, mK), mItr2(*mRead2, mK),
              mOutputs(pOutputs), mClassWriter(pClassWriter)
        {
        }

    private:
        const uint64_t mK;
        const uint64_t mNum;
        const GossReadPtr mRead1;
        const GossReadPtr mRead2;
        GossRead::Iterator mItr1;
        GossRead::Iterator mItr2;
        vector<Outs>& mOutputs;
        ReadClassWriter& mClassWriter;
    };

    typedef std::shared_ptr<KmerSrc> KmerSrcPtr;
    typedef std::shared_ptr<Read> ReadPtr;
    typedef std::shared_ptr<Pair> PairPtr;

    class KmerClassifier
    {
    public:

        bool operator()(Gossamer::edge_type pKmer, uint8_t& pClass) const
        {
            uint64_t r;
            uint8_t c = 0;
            pKmer.normalize(K());
            if (   !mBounded
                || (pKmer >= mFrom && pKmer <= mTo))
            {
                if (mKmers.accessAndRank(KmerSet::Edge(pKmer), r))
                {
                    c += uint8_t(mLhs.get(r)) << 1;
                    c += mRhs.get(r);
                    pClass = c;
                    return true;
                }
            }
            return false;
        }

        uint64_t K() const
        {
            return mKmers.K();
        }

        KmerClassifier(const string& pName, FileFactory& pFactory)
            : mKmers(pName, pFactory),
              mLhs(pName + ".lhs-bits", pFactory),
              mRhs(pName + ".rhs-bits", pFactory),
              mBounded(false), mFrom(0), mTo(~mFrom)
        {
        }

        KmerClassifier(const string& pName, FileFactory& pFactory, uint64_t pNumPasses, uint64_t pCurPass)
            : mKmers(pName, pFactory),
              mLhs(pName + ".lhs-bits", pFactory),
              mRhs(pName + ".rhs-bits", pFactory),
              mBounded(pNumPasses > 1)
        {
            const uint64_t z = mKmers.count();
            const uint64_t from = pCurPass * z / pNumPasses;
            const uint64_t to = (pCurPass + 1) * z / pNumPasses - 1;
            mFrom = mKmers.select(from).value();
            mTo = mKmers.select(to).value();
            // cerr << "kmer classifer [" << kmerToString(25, mFrom) << ", " << kmerToString(25, mTo) << "]\n";
        }

    private:
        const KmerSet mKmers;
        const WordyBitVector mLhs;
        const WordyBitVector mRhs;
        bool mBounded;
        Gossamer::edge_type mFrom;
        Gossamer::edge_type mTo;
    };

    class Classifier
    {
    public:
        const vector<uint64_t>& getCounts() const
        {
            return mCounts;
        }

        void operator()(KmerSrc& pSrc)
        {
            uint8_t blrg = 0;
            for (; pSrc.valid(); ++pSrc)
            {
                uint8_t c;
                if (mKmerClass(*pSrc, c))
                {
                    blrg |= 1 << c;
                }
            }
            if (mSinglePass)
            {
                pSrc.print(blrg);
                mCounts[blrg] += 1;
            }
            else
            {
                pSrc.writeClass(blrg);
            }
        }

        void push_back(KmerSrcPtr pSrc)
        {
            (*this)(*pSrc);
        }

        Classifier(const KmerClassifier& pKmerClass, bool pSinglePass)
            : mSinglePass(pSinglePass), mKmerClass(pKmerClass), mCounts(16, 0)
        {
        }

    private:

        bool mSinglePass;
        const KmerClassifier& mKmerClass;
        vector<uint64_t> mCounts;
    };

    typedef std::shared_ptr<Classifier> ClassifierPtr;

    string classStr(const string& pLhsName, const string& pRhsName, uint64_t i)
    {
        switch (i)
        {
            case 0x0:
                return "neither";
            case 0x1:
                return "both";
            case 0x2:
                return "definitely " + pRhsName;
            case 0x3:
                return "probably " + pRhsName;
            case 0x4:
                return "definitely " + pLhsName;
            case 0x5:
                return "probably " + pLhsName;
            case 0x6:
                return "ambiguous";
            case 0x7:
                return "ambiguous";
            case 0x8:
                return "both";
            case 0x9:
                return "probably both";
            case 0xa:
                return "definitely " + pRhsName;
            case 0xb:
                return "probably " + pRhsName;
            case 0xc:
                return "definitely " + pLhsName;
            case 0xd:
                return "probably " + pLhsName;
            case 0xe:
                return "ambiguous";
            case 0xf:
                return "ambiguous";
        }
        return "?!";
    }

    string
    filename(const string& pPrefix, const string& pSuffix, const string& pHalf, const string& pClass)
    {
        stringstream ss;
        if (pPrefix.size())
        {
            ss << pPrefix << '_';
        }
        ss << pClass;
        if (pHalf.size())
        {
            ss << '_' << pHalf;
        }
        if (pSuffix.size())
        {
            ss << '.' << pSuffix;
        }
        return ss.str();
    }

    string 
    neither(const string& pPrefix, const string& pSuffix, const string pHalf="")
    {
        return filename(pPrefix, pSuffix, pHalf, "neither");
    }

    string
    both(const string& pPrefix, const string& pSuffix, const string pHalf="")
    {
        return filename(pPrefix, pSuffix, pHalf, "both");
    }

    string
    ambiguous(const string& pPrefix, const string& pSuffix, const string pHalf="")
    {
        return filename(pPrefix, pSuffix, pHalf, "ambiguous");
    }

    string
    lhs(const string& pName, const string& pPrefix, const string& pSuffix, const string pHalf="")
    {
        return filename(pPrefix, pSuffix, pHalf, pName.size() ? pName : "graft");
    }

    string
    rhs(const string& pName, const string& pPrefix, const string& pSuffix, const string pHalf="")
    {
        return filename(pPrefix, pSuffix, pHalf, pName.size() ? pName : "host");
    }

    void classReads(Logger& pLog, FileFactory& pFac, const string& pIn, uint64_t pNumPasses, 
                    uint64_t pNumThreads, const ReadItems& pReadItems, bool pNoWrite,
                    ReadClassWriter& pClassWriter, uint64_t pK, vector<uint64_t>& pCounts,
                    const string& pPrefix, const string& pLhsName, const string& pRhsName, const string& pSuffix)
    {
        mutex neitherM;
        mutex bothM;
        mutex lhsM;
        mutex rhsM;
        mutex ambiguousM;
        FileFactory::OutHolderPtr neitherP, bothP, lhsP, rhsP, ambiguousP;
        vector<Out> outs;

        if (!pNoWrite)
        {
            neitherP = pFac.out(neither(pPrefix, pSuffix));
            pLog(info, "writing to " + neither(pPrefix, pSuffix));
            bothP = pFac.out(both(pPrefix, pSuffix));
            pLog(info, "writing to " + both(pPrefix, pSuffix));
            lhsP = pFac.out(lhs(pLhsName, pPrefix, pSuffix));
            pLog(info, "writing to " + lhs(pLhsName, pPrefix, pSuffix));
            rhsP = pFac.out(rhs(pRhsName, pPrefix, pSuffix));
            pLog(info, "writing to " + rhs(pRhsName, pPrefix, pSuffix));
            ambiguousP = pFac.out(ambiguous(pPrefix, pSuffix));
            pLog(info, "writing to " + ambiguous(pPrefix, pSuffix));

            outs.push_back(Out(&neitherM, &**neitherP));
            outs.push_back(Out(&bothM, &**bothP));
            outs.push_back(Out(&rhsM, &**rhsP));
            outs.push_back(Out(&rhsM, &**rhsP));
            outs.push_back(Out(&lhsM, &**lhsP));
            outs.push_back(Out(&lhsM, &**lhsP));
            outs.push_back(Out(&ambiguousM, &**ambiguousP));
            outs.push_back(Out(&ambiguousM, &**ambiguousP));
            outs.push_back(Out(&bothM, &**bothP));
            outs.push_back(Out(&bothM, &**bothP));
            outs.push_back(Out(&rhsM, &**rhsP));
            outs.push_back(Out(&rhsM, &**rhsP));
            outs.push_back(Out(&lhsM, &**lhsP));
            outs.push_back(Out(&lhsM, &**lhsP));
            outs.push_back(Out(&ambiguousM, &**ambiguousP));
            outs.push_back(Out(&ambiguousM, &**ambiguousP));
        }

        for (uint64_t p = 0; p < pNumPasses; ++p)
        {
            pLog(info, "pass " + lexical_cast<string>(p));
            KmerClassifier kmerClassr(pIn, pFac, pNumPasses, p);
            vector<ClassifierPtr> classrs;
            BackgroundMultiConsumer<KmerSrcPtr> grp(128);
            for (uint64_t i = 0; i < pNumThreads; ++i)
            {
                classrs.push_back(ClassifierPtr(new Classifier(kmerClassr, pNumPasses == 1)));
                grp.add(*classrs.back());
            }

            UnboundedProgressMonitor umon(pLog, 100000, " reads");
            LineSourceFactory lineSrcFac(BackgroundLineSource::create);
            ReadSequenceFileSequence reads(pReadItems, pFac, lineSrcFac, &umon, &pLog);
            for (uint64_t r = 0; reads.valid(); ++reads, ++r)
            {
                KmerSrcPtr srcPtr(new Read(pK, r, (*reads).clone(), outs, pClassWriter));
                grp.push_back(srcPtr);
            }
            grp.wait();

            if (pNumPasses == 1)
            {
                for (uint64_t i = 0; i < classrs.size(); ++i)
                {
                    for (uint64_t j = 0; j < pCounts.size(); ++j)
                    {
                        const uint64_t c = classrs[i]->getCounts()[j];
                        pCounts[j] += c;
                    }
                }
            }
        }

        if (pNumPasses > 1)
        {
            ReadClassWriter::ReadClassItr rcItr(pClassWriter.mergeBuffers());
            if (pNoWrite)
            {
                // Just accumulate counts.
                for (; rcItr.valid(); ++rcItr)
                {
                    pCounts[(*rcItr).second] += 1;
                }
            }
            else
            {
                pLog(info, "grouping output reads");
                UnboundedProgressMonitor umon(pLog, 100000, " reads");
                LineSourceFactory lineSrcFac(BackgroundLineSource::create);
                ReadSequenceFileSequence reads(pReadItems, pFac, lineSrcFac,
                                               &umon, &pLog);
                for (uint64_t r = 0; reads.valid(); ++reads, ++r, ++rcItr)
                {
                    BOOST_ASSERT(rcItr.valid());
                    pair<uint64_t, uint8_t> rc(*rcItr);
                    BOOST_ASSERT(rc.first == r);
                    Read::print(*reads, rc.second, outs);
                    pCounts[rc.second] += 1;
                }
            }
        }
    }

    void classPairs(Logger& pLog, FileFactory& pFac, const string& pIn, uint64_t pNumPasses, 
                    uint64_t pNumThreads, const ReadItems& pReadItems, bool pNoWrite,
                    ReadClassWriter& pClassWriter, uint64_t pK, vector<uint64_t>& pCounts,
                    const string& pPrefix, const string& pLhsName, const string& pRhsName, const string& pSuffix)
    {
        mutex neitherM;
        mutex bothM;
        mutex lhsM;
        mutex rhsM;
        mutex ambiguousM;
        FileFactory::OutHolderPtr neitherP_1, neitherP_2, bothP_1, bothP_2, 
                     ambiguousP_1, ambiguousP_2, lhsP_1, lhsP_2, rhsP_1, rhsP_2;
        vector<Outs> outs;

        if (!pNoWrite)
        {
            neitherP_1 = pFac.out(neither(pPrefix, pSuffix, "1"));
            pLog(info, "writing to " + neither(pPrefix, pSuffix, "1"));
            neitherP_2 = pFac.out(neither(pPrefix, pSuffix, "2"));
            pLog(info, "writing to " + neither(pPrefix, pSuffix, "2"));
            bothP_1 = pFac.out(both(pPrefix, pSuffix, "1"));
            pLog(info, "writing to " + both(pPrefix, pSuffix, "1"));
            bothP_2 = pFac.out(both(pPrefix, pSuffix, "2"));
            pLog(info, "writing to " + both(pPrefix, pSuffix, "2"));
            ambiguousP_1 = pFac.out(ambiguous(pPrefix, pSuffix, "1"));
            pLog(info, "writing to " + ambiguous(pPrefix, pSuffix, "1"));
            ambiguousP_2 = pFac.out(ambiguous(pPrefix, pSuffix, "2"));
            pLog(info, "writing to " + ambiguous(pPrefix, pSuffix, "2"));
            lhsP_1 = pFac.out(lhs(pLhsName, pPrefix, pSuffix, "1"));
            pLog(info, "writing to " + lhs(pLhsName, pPrefix, pSuffix, "1"));
            lhsP_2 = pFac.out(lhs(pLhsName, pPrefix, pSuffix, "2"));
            pLog(info, "writing to " + lhs(pLhsName, pPrefix, pSuffix, "2"));
            rhsP_1 = pFac.out(rhs(pRhsName, pPrefix, pSuffix, "1"));
            pLog(info, "writing to " + rhs(pRhsName, pPrefix, pSuffix, "1"));
            rhsP_2 = pFac.out(rhs(pRhsName, pPrefix, pSuffix, "2"));
            pLog(info, "writing to " + rhs(pRhsName, pPrefix, pSuffix, "2"));

            outs.push_back(Outs(&neitherM, &**neitherP_1, &**neitherP_2));
            outs.push_back(Outs(&bothM, &**bothP_1, &**bothP_2));
            outs.push_back(Outs(&rhsM, &**rhsP_1, &**rhsP_2));
            outs.push_back(Outs(&rhsM, &**rhsP_1, &**rhsP_2));
            outs.push_back(Outs(&lhsM, &**lhsP_1, &**lhsP_2));
            outs.push_back(Outs(&lhsM, &**lhsP_1, &**lhsP_2));
            outs.push_back(Outs(&ambiguousM, &**ambiguousP_1, &**ambiguousP_2));
            outs.push_back(Outs(&ambiguousM, &**ambiguousP_1, &**ambiguousP_2));
            outs.push_back(Outs(&bothM, &**bothP_1, &**bothP_2));
            outs.push_back(Outs(&bothM, &**bothP_1, &**bothP_2));
            outs.push_back(Outs(&rhsM, &**rhsP_1, &**rhsP_2));
            outs.push_back(Outs(&rhsM, &**rhsP_1, &**rhsP_2));
            outs.push_back(Outs(&lhsM, &**lhsP_1, &**lhsP_2));
            outs.push_back(Outs(&lhsM, &**lhsP_1, &**lhsP_2));
            outs.push_back(Outs(&ambiguousM, &**ambiguousP_1, &**ambiguousP_2));
            outs.push_back(Outs(&ambiguousM, &**ambiguousP_1, &**ambiguousP_2));
        }

        for (uint64_t p = 0; p < pNumPasses; ++p)
        {
            pLog(info, "pass " + lexical_cast<string>(p));
            KmerClassifier kmerClassr(pIn, pFac, pNumPasses, p);
            vector<ClassifierPtr> classrs;
            BackgroundMultiConsumer<KmerSrcPtr> grp(128);
            for (uint64_t i = 0; i < pNumThreads; ++i)
            {
                classrs.push_back(ClassifierPtr(new Classifier(kmerClassr, pNumPasses == 1)));
                grp.add(*classrs.back());
            }

            UnboundedProgressMonitor umon(pLog, 100000, " reads");
            LineSourceFactory lineSrcFac(BackgroundLineSource::create);
            ReadPairSequenceFileSequence reads(pReadItems, pFac, lineSrcFac,
                    &umon, &pLog);
            for (uint64_t r = 0; reads.valid(); ++reads, ++r)
            {
                KmerSrcPtr srcPtr(new Pair(pK, r, reads.lhs().clone(), reads.rhs().clone(), outs, pClassWriter));
                grp.push_back(srcPtr);
            }
            grp.wait();

            if (pNumPasses == 1)
            {
                for (uint64_t i = 0; i < classrs.size(); ++i)
                {
                    for (uint64_t j = 0; j < pCounts.size(); ++j)
                    {
                        const uint64_t c = classrs[i]->getCounts()[j];
                        pCounts[j] += c;
                    }
                }
            }
        }

        if (pNumPasses > 1)
        {
            ReadClassWriter::ReadClassItr rcItr(pClassWriter.mergeBuffers());
            if (pNoWrite)
            {
                // Just accumulate counts.
                for (; rcItr.valid(); ++rcItr)
                {
                    pCounts[(*rcItr).second] += 1;
                }
            }
            else
            {
                pLog(info, "grouping output reads");
                UnboundedProgressMonitor umon(pLog, 100000, " reads");
                LineSourceFactory lineSrcFac(BackgroundLineSource::create);
                ReadPairSequenceFileSequence reads(pReadItems, pFac,
                        lineSrcFac, &umon, &pLog);
                for (uint64_t r = 0; reads.valid(); ++reads, ++r, ++rcItr)
                {
                    BOOST_ASSERT(rcItr.valid());
                    pair<uint64_t, uint8_t> rc(*rcItr);
                    BOOST_ASSERT(rc.first == r);
                    Pair::print(reads.lhs(), reads.rhs(), rc.second, outs);
                    pCounts[rc.second] += 1;
                }
            }
        }
    }

} // namespace anonymous

void
GossCmdGroupReads::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);;

    Timer t;

    // Assume 0.2 GB operating overhead.
    const uint64_t maxB = (mMaxMemory - 0.2) * 1024ULL * 1024ULL * 1024ULL;
    const uint64_t maxBuff = maxB / 8;
    ReadClassWriter classWriter(log, fac, maxBuff);

    uint64_t K;
    uint64_t numPasses;
    {
        fac.populate(false);
        KmerSet kmers(mIn, fac);
        K = kmers.K();
        const uint64_t kmersB = kmers.stat().as<uint64_t>("storage");
        const uint64_t lhsB = fac.size(mIn + ".lhs-bits");
        const uint64_t rhsB = fac.size(mIn + ".rhs-bits");
        const uint64_t refB = kmersB + lhsB + rhsB;
        numPasses = refB / maxB + 1;
    }
    log(info, "performing " + lexical_cast<string>(numPasses) + 
              " pass" + (numPasses > 1 ? "es" : ""));

    fac.populate(numPasses == 1);

    uint64_t T = mNumThreads;
    if (mPreserveReadOrder)
    {
        log(warning, "--preserve-read-order implies --num-threads 1");
        T = 1;
    }

    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    GossReadSequenceFactoryPtr seqFac
        = std::make_shared<GossReadSequenceBasesFactory>();

    vector<uint64_t> counts(16, 0);
    if (mPairs)
    {
        if (!mLines.empty())
        {
            std::deque<GossReadSequence::Item> items;

            GossReadParserFactory lineParserFac(LineParser::create);
            for (auto& f: mLines)
            {
                items.push_back(GossReadSequence::Item(f,
                                lineParserFac, seqFac));
            }

            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            ReadPairSequenceFileSequence reads(items, fac, lineSrcFac,
                &umon, &log);
            classPairs(log, fac, mIn, numPasses, T, items, mDontWriteReads,
                classWriter, K, counts, mPrefix, mLhsName, mRhsName, "txt");
        }

        if (!mFastas.empty())
        {
            std::deque<GossReadSequence::Item> items;

            GossReadParserFactory fastaParserFac(FastaParser::create);
            for (auto& f: mFastas)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastaParserFac, seqFac));
            }

            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            ReadPairSequenceFileSequence reads(items, fac, lineSrcFac,
                &umon, &log);
            classPairs(log, fac, mIn, numPasses, T, items, mDontWriteReads,
                classWriter, K, counts, mPrefix, mLhsName, mRhsName, "fasta");
        }

        if (!mFastqs.empty())
        {
            std::deque<GossReadSequence::Item> items;

            GossReadParserFactory fastqParserFac(FastqParser::create);
            for (auto& f: mFastqs)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastqParserFac, seqFac));
            }

            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            ReadPairSequenceFileSequence reads(items, fac, lineSrcFac,
                &umon, &log);
            classPairs(log, fac, mIn, numPasses, T, items, mDontWriteReads,
                classWriter, K, counts, mPrefix, mLhsName, mRhsName, "fastq");
        }
    }
    else
    {
        if (!mLines.empty())
        {
            std::deque<GossReadSequence::Item> items;

            GossReadParserFactory lineParserFac(LineParser::create);
            for (auto& f: mLines)
            {
                items.push_back(GossReadSequence::Item(f,
                                lineParserFac, seqFac));
            }

            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            ReadSequenceFileSequence reads(items, fac, lineSrcFac,
                &umon, &log);
            classReads(log, fac, mIn, numPasses, T, items, mDontWriteReads,
                classWriter, K, counts, mPrefix, mLhsName, mRhsName, "txt");
        }

        if (!mFastas.empty())
        {
            std::deque<GossReadSequence::Item> items;

            GossReadParserFactory fastaParserFac(FastaParser::create);
            for (auto& f: mFastas)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastaParserFac, seqFac));
            }

            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            ReadSequenceFileSequence reads(items, fac, lineSrcFac,
                &umon, &log);
            classReads(log, fac, mIn, numPasses, T, items, mDontWriteReads,
                classWriter, K, counts, mPrefix, mLhsName, mRhsName, "fasta");
        }

        if (!mFastqs.empty())
        {
            std::deque<GossReadSequence::Item> items;

            GossReadParserFactory fastqParserFac(FastqParser::create);
            for (auto& f: mFastqs)
            {
                items.push_back(GossReadSequence::Item(f,
                                fastqParserFac, seqFac));
            }

            UnboundedProgressMonitor umon(log, 100000, " read pairs");
            ReadSequenceFileSequence reads(items, fac, lineSrcFac,
                &umon, &log);
            classReads(log, fac, mIn, numPasses, T, items, mDontWriteReads,
                classWriter, K, counts, mPrefix, mLhsName, mRhsName, "fastq");
        }
    }

    uint64_t total = 0;
    for (uint64_t i = 0; i < counts.size(); ++i)
    {
        total += counts[i];
    }

    const uint64_t graftC = counts[0x4] + counts[0x5] + counts[0xc] + counts[0xd];
    const uint64_t hostC = counts[0x2] + counts[0x3] + counts[0xa] + counts[0xb];
    const uint64_t bothC = counts[0x1] + counts[0x8] + counts[0x9];
    const uint64_t neitherC = counts[0x0];
    const uint64_t ambigC = counts[0x6] + counts[0x7] + counts[0xe] + counts[0xf];

    if (mDontWriteReads)
    {
        cout << 100.0 * graftC / total << '\t'
             << 100.0 * hostC / total << '\t'
             << 100.0 * bothC / total << '\t'
             << 100.0 * neitherC / total << '\t'
             << 100.0 * ambigC / total << '\n';
    }
    else
    {
        cout << "Statistics\n";
        cout << "B\tG\tH\tM\t" << "count\t" << "percent\t" << "class\n";
        for (uint64_t i = 0; i < counts.size(); ++i)
        {
            cout << (i & 8 ? '1' : '0') << '\t';
            cout << (i & 4 ? '1' : '0') << '\t';
            cout << (i & 2 ? '1' : '0') << '\t';
            cout << (i & 1 ? '1' : '0') << '\t';
            cout << counts[i] << '\t';
            cout << (100.0 * double(counts[i]) / total) << '\t';
            cout << '"' << classStr(mLhsName, mRhsName, i) << '"' << '\n';
        }

        cout << "\nSummary\n";
        cout << "count\t"       << "percent\t"                      << "class\n";
        cout << graftC << '\t'  << 100.0 * graftC / total << '\t'   << mLhsName << '\n';
        cout << hostC << '\t'   << 100.0 * hostC / total << '\t'    << mRhsName << '\n';
        cout << bothC << '\t'   << 100.0 * bothC / total << '\t'    << "both" << '\n';
        cout << neitherC << '\t'<< 100.0 * neitherC / total << '\t' << "neither" << '\n';
        cout << ambigC << '\t'  << 100.0 * ambigC / total << '\t'   << "ambiguous" << '\n';
    }

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

GossCmdPtr
GossCmdFactoryGroupReads::create(App& pApp, const boost::program_options::variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    strings fastas;
    chk.getRepeating0("fasta-in", fastas, readChk);

    strings fastqs;
    chk.getRepeating0("fastq-in", fastqs, readChk);

    strings lines;
    chk.getRepeating0("line-in", lines, readChk);

    bool pairs;
    chk.getOptional("pairs", pairs);

    double M = 1.0e9;
    chk.getOptional("max-memory", M);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    string lhsName("lhs");
    chk.getOptional("lhs-name", lhsName);

    string rhsName("rhs");
    chk.getOptional("rhs-name", rhsName);

    string prefix;
    chk.getOptional("output-filename-prefix", prefix);

    bool noOut = false;
    chk.getOptional("dont-write-reads", noOut);

    bool ord = false;
    chk.getOptional("preserve-read-order", ord);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdGroupReads(in, fastas, fastqs, lines, pairs, M, T, 
                                            lhsName, rhsName, prefix, noOut, ord));
}

GossCmdFactoryGroupReads::GossCmdFactoryGroupReads()
    : GossCmdFactory("filter reads keeping/discarding those that coincide with a graph.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");

    mSpecificOptions.addOpt<double>("max-memory", "M",
            "maximum memory (in GB) to use (default: unlimited)");
    mSpecificOptions.addOpt<bool>("pairs", "",
            "treat reads as pairs");
    mSpecificOptions.addOpt<string>("lhs-name", "",
            "name for left-hand side reads");
    mSpecificOptions.addOpt<string>("rhs-name", "",
            "name for right-hand side reads");
    mSpecificOptions.addOpt<string>("output-filename-prefix", "",
            "prefix for output files");
    mSpecificOptions.addOpt<bool>("dont-write-reads", "",
            "do not produce any output read files");
    mSpecificOptions.addOpt<bool>("preserve-read-order", "",
            "maintain the same relative ordering of reads in output files");
}
