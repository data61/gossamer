// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "KmerSpectrum.hh"
#include "Debug.hh"
#include "SimpleHashSet.hh"
#include "Heap.hh"
#include <iostream>
#include <set>
#include <boost/numeric/ublas/matrix.hpp>

using namespace std;
using namespace boost;
using namespace boost::numeric;

namespace // anonymous
{
    typedef vector<string> strings;

    Debug eliminateUnbiquity("eliminate-ubiquitous-kmers", "");

    uint64_t dot(const vector<uint64_t>& pLhs, const vector<uint64_t>& pRhs)
    {
        uint64_t r = 0;
        uint64_t i = 0;
        uint64_t j = 0;
        while (i < pLhs.size() && j < pRhs.size())
        {
            if (pLhs[i] < pRhs[j])
            {
                ++i;
                continue;
            }
            if (pLhs[i] > pRhs[j])
            {
                ++j;
                continue;
            }
            ++i;
            ++j;
            ++r;
        }
        return r;
    }

    uint64_t asymmetricDot(const vector<uint64_t>& pSparseLhs, const vector<uint64_t>& pDenseRhs)
    {
        uint64_t r = 0;
        for (uint64_t i = 0; i < pSparseLhs.size(); ++i)
        {
            r += pDenseRhs[pSparseLhs[i]];
        }
        return r;
    }

    class SingleRowHandler : public GossReadHandler
    {
    public:
        typedef uint32_t count_type;

        void operator()(const GossRead& pRead)
        {
            for (GossRead::Iterator i(pRead, mK); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(mK);
                uint32_t rnk = mCanonical[e.asUInt64()];
                mSpectrum[rnk]++;
                mNumKmers++;
            }
        }

        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            for (GossRead::Iterator i(pLhs, mK); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(mK);
                uint32_t rnk = mCanonical[e.asUInt64()];
                mSpectrum[rnk]++;
                mNumKmers++;
            }
            for (GossRead::Iterator i(pRhs, mK); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(mK);
                uint32_t rnk = mCanonical[e.asUInt64()];
                mSpectrum[rnk]++;
                mNumKmers++;
            }
        }

        void startFile(const std::string& pFileName)
        {
            cerr << pFileName << endl;
            mNumKmers = 0;
        }

        void endFile()
        {
            cerr << mNumKmers << endl;
        }

        void end()
        {
            string vn(mVarName);
            string fn(mFileName);
            size_t dims[2] = {1, mSpectrum.size()};
            matvar_t *m = Mat_VarCreate(vn.c_str(), MAT_C_UINT32, MAT_T_UINT32, 2, dims, reinterpret_cast<void*>(&mSpectrum[0]), 0);
            mat_t* v = Mat_Open(fn.c_str(), MAT_ACC_RDWR);
            Mat_VarWrite(v, m, MAT_COMPRESSION_ZLIB);
            Mat_VarFree(m);
            Mat_Close(v);
        }

        SingleRowHandler(uint64_t pK, const string& pVarName, const string& pFileName)
            : mK(pK), mVarName(pVarName), mFileName(pFileName), mNumKmers(0)
        {
            BOOST_ASSERT(mK < 32);
            mCanonical.resize(1ULL << (2 * mK));
            uint64_t j = 0;
            for (uint64_t i = 0; i < mCanonical.size(); ++i)
            {
                Gossamer::edge_type e(i);
                e.normalize(mK);
                if (e.asUInt64() == i)
                {
                    mCanonical[i] = j++;
                }
            }
            mSpectrum.resize(j);
        }

    private:
        const uint64_t mK;
        const string mVarName;
        const string mFileName;
        vector<uint64_t> mCanonical;
        vector<uint32_t> mSpectrum;
        uint64_t mNumKmers;
    };

    class SparseSingleRowHandler : public GossReadHandler
    {
    public:
        typedef uint32_t count_type;

        void operator()(const GossRead& pRead)
        {
            uint64_t k = mKmerSet.K();
            for (GossRead::Iterator i(pRead, k); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(k);
                uint64_t rnk = 0;
                if (mKmerSet.accessAndRank(KmerSet::Edge(e), rnk))
                {
                    mSpectrum[rnk]++;
                }
            }
        }

        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            uint64_t k = mKmerSet.K();
            for (GossRead::Iterator i(pLhs, k); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(k);
                uint64_t rnk = 0;
                if (mKmerSet.accessAndRank(KmerSet::Edge(e), rnk))
                {
                    mSpectrum[rnk]++;
                }
            }
            for (GossRead::Iterator i(pRhs, k); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(k);
                uint64_t rnk = 0;
                if (mKmerSet.accessAndRank(KmerSet::Edge(e), rnk))
                {
                    mSpectrum[rnk]++;
                }
            }
        }

        void end()
        {
#if 0
            for (uint64_t i = 0; i < mSpectrum.size(); ++i)
            {
                cerr << mSpectrum[i] << '\t';
            }
            cerr << endl;
#endif
            vector<vector<uint64_t> > spectra;
            string sn = mKmersName + ".idx";
            ifstream in(sn.c_str());
            uint64_t x = 0;
            in >> x;
            if (!in.good())
            {
                throw "xxx";
            }
            spectra.resize(x);
            for (uint64_t i = 0; i < spectra.size(); ++i)
            {
                in >> x;
                if (!in.good())
                {
                    throw "xxx";
                }
                spectra[i].resize(x);
                for (uint64_t j = 0; j < spectra[i].size(); ++j)
                {
                    in >> spectra[i][j];
                }
            }

            const uint64_t n = spectra.size();
            float mx[n];
            for (uint64_t i = 0; i < n; ++i)
            {
                mx[i] = asymmetricDot(spectra[i], mSpectrum);
            }

            string vn(mVarName);
            string fn(mFileName);
            size_t dims[2] = {1, n};
            matvar_t *m = Mat_VarCreate(vn.c_str(), MAT_C_SINGLE, MAT_T_SINGLE, 2, dims, reinterpret_cast<void*>(mx), 0);
            mat_t* v = Mat_Open(fn.c_str(), MAT_ACC_RDWR);
            Mat_VarWrite(v, m, MAT_COMPRESSION_ZLIB);
            Mat_VarFree(m);
            Mat_Close(v);
        }

        SparseSingleRowHandler(const KmerSet& pKmerSet, const string& pKmersName, const string& pVarName, const string& pFileName)
            : mKmerSet(pKmerSet), mKmersName(pKmersName), mVarName(pVarName), mFileName(pFileName), mSpectrum(mKmerSet.count())
        {
        }

    private:
        const KmerSet& mKmerSet;
        const string mKmersName;
        const string mVarName;
        const string mFileName;
        vector<uint64_t> mSpectrum;
    };

    class MultiRowHandler : public GossReadHandler
    {
    public:
        typedef uint32_t count_type;

        void operator()(const GossRead& pRead)
        {
            vector<uint32_t> spectrum(mColumns);
            for (GossRead::Iterator i(pRead, mK); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(mK);
                uint32_t rnk = mCanonical[e.asUInt64()];
                spectrum[rnk]++;
            }
            //for (uint64_t i = 0; i < spectrum.size(); ++i)
            //{
            //    cerr << spectrum[i] << '\t';
            //}
            //cerr << endl;
            mRows++;
            mSpectra.insert(mSpectra.end(), spectrum.begin(), spectrum.end());
        }

        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            vector<uint32_t> spectrum(mColumns);
            for (GossRead::Iterator i(pLhs, mK); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(mK);
                uint32_t rnk = mCanonical[e.asUInt64()];
                spectrum[rnk]++;
            }
            for (GossRead::Iterator i(pRhs, mK); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(mK);
                uint32_t rnk = mCanonical[e.asUInt64()];
                spectrum[rnk]++;
            }
            mRows++;
            mSpectra.insert(mSpectra.end(), spectrum.begin(), spectrum.end());
        }

        void end()
        {
            string vn(mVarName);
            string fn(mFileName);
            size_t dims[2] = {mColumns, mRows};
            cerr << mColumns << '\t' << mRows << '\n';
            matvar_t *m = Mat_VarCreate(vn.c_str(), MAT_C_UINT32, MAT_T_UINT32, 2, dims, reinterpret_cast<void*>(&mSpectra[0]), 0);
            mat_t* v = Mat_Open(fn.c_str(), MAT_ACC_RDWR);
            Mat_VarWrite(v, m, MAT_COMPRESSION_ZLIB);
            Mat_Close(v);
            Mat_VarFree(m);
        }

        MultiRowHandler(uint64_t pK, const string& pVarName, const string& pFileName)
            : mK(pK), mVarName(pVarName), mFileName(pFileName)
        {
            BOOST_ASSERT(mK < 32);
            mCanonical.resize(1ULL << (2 * mK));
            uint64_t j = 0;
            for (uint64_t i = 0; i < mCanonical.size(); ++i)
            {
                Gossamer::edge_type e(i);
                e.normalize(mK);
                if (e.asUInt64() == i)
                {
                    mCanonical[i] = j++;
                }
            }
            mColumns = j;
            mRows = 0;
        }

    private:
        const uint64_t mK;
        const string mVarName;
        const string mFileName;
        vector<uint64_t> mCanonical;
        uint64_t mColumns;
        uint64_t mRows;
        vector<uint32_t> mSpectra;
    };

    class RowIterator
    {
    public:
        bool valid() const
        {
            return mCurr < mEnd;
        }

        pair<uint64_t,uint64_t> operator*() const
        {
            return mHere;
        }

        bool operator<(const RowIterator& pRhs) const
        {
            return mHere < pRhs.mHere;
        }

        void operator++()
        {
            ++mCurr;
            if (valid())
            {
                mHere.first = mIndex->select(mCurr).value().asUInt64() - mStart;
            }
        }

        RowIterator(const SparseArray& pIndex, uint64_t pRow, uint64_t pStart, uint64_t pBegin, uint64_t pEnd)
            : mIndex(&pIndex), mStart(pStart), mCurr(pBegin), mEnd(pEnd), mHere(0, pRow)
        {
            if (valid())
            {
                mHere.first = mIndex->select(mCurr).value().asUInt64() - mStart;
            }
        }

    private:
        const SparseArray* mIndex;
        uint64_t mStart;
        uint64_t mCurr;
        uint64_t mEnd;
        pair<uint64_t,uint64_t> mHere;
    };


    class SparseMultiRowHandler : public GossReadHandler
    {
    public:
        typedef uint32_t count_type;

        void operator()(const GossRead& pRead)
        {
            cerr << pRead.label() << endl;
            const uint64_t k = mKmers.K();
            for (GossRead::Iterator i(pRead, k); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(k);
                uint64_t rnk = 0;
                if (mKmers.accessAndRank(KmerSet::Edge(e), rnk))
                {
                    mSpectrum.push_back(rnk);
                }
            }
            if (!mPerFile)
            {
                mNames.push_back(pRead.label());
                mLens.push_back(pRead.length());
                flush(mSpectrum);
                mSpectrum.clear();
                ++mCurrFileNum;
            }
        }

        void operator()(const GossRead& pLhs, const GossRead& pRhs)
        {
            const uint64_t k = mKmers.K();
            for (GossRead::Iterator i(pLhs, k); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(k);
                uint64_t rnk = 0;
                if (mKmers.accessAndRank(KmerSet::Edge(e), rnk))
                {
                    mSpectrum.push_back(rnk);
                }
            }
            for (GossRead::Iterator i(pRhs, k); i.valid(); ++i)
            {
                Gossamer::edge_type e(i.kmer());
                e.normalize(k);
                uint64_t rnk = 0;
                if (mKmers.accessAndRank(KmerSet::Edge(e), rnk))
                {
                    mSpectrum.push_back(rnk);
                }
            }
            if (!mPerFile)
            {
                mNames.push_back(pLhs.label());
                mLens.push_back(pLhs.length() + pRhs.length());
                flush(mSpectrum);
                mSpectrum.clear();
                ++mCurrFileNum;
            }
        }

        void startFile(const string& pFileName)
        {
            if (mPerFile)
            {
                mNames.push_back(pFileName);
            }
        }

        void endFile()
        {
            if (mPerFile)
            {
                mLens.push_back(mSpectrum.size());
                flush(mSpectrum);
                mSpectrum.clear();
                ++mCurrFileNum;
            }
        }

        void end()
        {
            const uint64_t numFiles = mCurrFileNum;
            uint64_t mz = numFiles * mKmers.count();

            cerr << "mNumKmers = " << mNumKmers << endl;
            cerr << "mKmers.count() = " << mKmers.count() << endl;
            cerr << "mz = " << mz << endl;

            mKmersFileHolder = FileFactory::OutHolderPtr();

            {
                MappedArray<uint64_t>::Builder lens(mKmersName + ".lens", mFactory);
                for (uint64_t i = 0; i < mLens.size(); ++i)
                {
                    lens.push_back(mLens[i]);
                }
                lens.end();
            }
            {
                FileFactory::OutHolderPtr outp = mFactory.out(mKmersName + ".names");
                ostream& out = **outp;
                for (uint64_t i = 0; i < mNames.size(); ++i)
                {
                    out << mNames[i] << '\n';
                }
            }

            {
                FileFactory::InHolderPtr inp = mFactory.in(mKmersName + ".tmp-kmers");
                istream& in(**inp);
                SparseArray::Builder bld(mKmersName + ".idx-transpose", mFactory, Gossamer::position_type(mz), mNumKmers);
                for (uint64_t i = 0; i < numFiles; ++i)
                {
                    uint64_t r = i * mKmers.count();
                    uint64_t z;
                    //in.read(reinterpret_cast<char*>(&z), sizeof(z));
                    in >> z;
                    cerr << z << '\t' << mNames[i] << endl;
                    for (uint64_t j = 0; j < z; ++j)
                    {
                        uint64_t w = 0;
                        //in.read(reinterpret_cast<char*>(&z), sizeof(w));
                        in >> w;
                        bld.push_back(Gossamer::position_type(r + w));
                    }
                }
                bld.end(Gossamer::position_type(mz));
            }
            //mFactory.remove(mKmersName + ".tmp-kmers");
            {
                SparseArray t(mKmersName + ".idx-transpose", mFactory);
                typedef pair<uint64_t,uint64_t> Range;
                Heap<RowIterator> itrs;
                for (uint64_t i = 0; i < numFiles; ++i)
                {
                    uint64_t r0 = i * mKmers.count();
                    uint64_t r1 = (i + 1) * mKmers.count();
                    Range r = t.rank(Gossamer::position_type(r0), Gossamer::position_type(r1));
                    RowIterator itr(t, i, r0, r.first, r.second);
                    if (itr.valid())
                    {
                        itrs.push_back(itr);
                    }
                }
                itrs.heapify();
                SparseArray::Builder bld(mKmersName + ".idx", mFactory, Gossamer::position_type(mz), mNumKmers);
                while (itrs.size())
                {
                    pair<uint64_t,uint64_t> itm = *(itrs.front());
                    ++(itrs.front());
                    if (itrs.front().valid())
                    {
                        itrs.front_changed();
                    }
                    else
                    {
                        itrs.pop();
                    }
                    uint64_t j = itm.first * numFiles + itm.second;
                    bld.push_back(Gossamer::position_type(j));
                }
                bld.end(Gossamer::position_type(mz));
            }
        }

        SparseMultiRowHandler(const KmerSet& pKmers, const string& pKmersName,
                              FileFactory& pFactory, const strings& pInitKmerSets,
                              bool pPerFile)
            : mKmers(pKmers), mFactory(pFactory), mKmersName(pKmersName), mCurrFileNum(0), mPerFile(pPerFile),
              mNumKmers(0), mKmersFileHolder(mFactory.out(mKmersName + ".tmp-kmers")), mKmersFile(**mKmersFileHolder)
        {
            addKmerSets(pInitKmerSets);
        }

    private:

        template <typename Collection>
        class PushBackVisitor
        {
        public:
            typedef typename Collection::value_type value_type;

            void operator()(const value_type& pItem)
            {
                mCollection.push_back(pItem);
            }

            PushBackVisitor(Collection& pCollection)
                : mCollection(pCollection)
            {
            }

        private:
            Collection& mCollection;
        };

        void flush(vector<uint64_t>& pKmers)
        {
            sort(pKmers.begin(), pKmers.end());
            pKmers.erase(unique(pKmers.begin(), pKmers.end()), pKmers.end());
            uint64_t z = pKmers.size();
            //mKmersFile.write(reinterpret_cast<const char*>(&z), sizeof(z));
            //mKmersFile.write(reinterpret_cast<const char*>(&pKmers[0]), sizeof(uint64_t) * z);
            mKmersFile << z;
            for (uint64_t i = 0; i < pKmers.size(); ++i)
            {
                mKmersFile << '\t' << pKmers[i];
            }
            mKmersFile << endl;
            mNumKmers += z;
        }

        void addKmerSets(const strings& pKmerSets)
        {
            const uint64_t k = mKmers.K();
            for (uint64_t i = 0; i < pKmerSets.size(); ++i)
            {
                const string& name(pKmerSets[i]);
                startFile(name);
                for (KmerSet::LazyIterator itr(name, mFactory); itr.valid(); ++itr)
                {
                    Gossamer::position_type kmer = (*itr).first.value();
                    kmer.normalize(k);
                    uint64_t rnk = 0;
                    if (mKmers.accessAndRank(KmerSet::Edge(kmer), rnk))
                    {
                        mSpectrum.push_back(rnk);
                    }
                }
                endFile();
            }
        }

        const KmerSet& mKmers;
        FileFactory& mFactory;
        const string mKmersName;
        vector<uint64_t> mSpectrum;
        uint64_t mCurrFileNum;
        vector<uint64_t> mLens;
        vector<string> mNames;
        bool mPerFile;
        uint64_t mNumKmers;
        FileFactory::OutHolderPtr mKmersFileHolder;
        ostream& mKmersFile;
    };
}
// namespace anonymous

GossReadHandlerPtr
KmerSpectrum::singleRowBuilder(uint64_t pK, const std::string& pVarName,
                               const std::string& pFileName)
{
    return GossReadHandlerPtr(new SingleRowHandler(pK, pVarName, pFileName));
}

GossReadHandlerPtr
KmerSpectrum::multiRowBuilder(uint64_t pK, const std::string& pVarName,
                               const std::string& pFileName)
{
    return GossReadHandlerPtr(new MultiRowHandler(pK, pVarName, pFileName));
}

GossReadHandlerPtr
KmerSpectrum::sparseSingleRowBuilder(const KmerSet& pKmerSet, const string& pKmersName, const std::string& pVarName,
                               const std::string& pFileName)
{
    return GossReadHandlerPtr(new SparseSingleRowHandler(pKmerSet, pKmersName, pVarName, pFileName));
}

GossReadHandlerPtr
KmerSpectrum::sparseMultiRowBuilder(const KmerSet& pKmerSet, const string& pKmersName, 
                                    const strings& pInitKmerSets,
                                    bool pPerFile,
                                    FileFactory& pFactory)
{
    return GossReadHandlerPtr(new SparseMultiRowHandler(pKmerSet, pKmersName, pFactory, pInitKmerSets, pPerFile));
}

