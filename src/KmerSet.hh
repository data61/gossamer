// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef KMERSET_HH
#define KMERSET_HH

#ifndef GRAPHESSENTIALS_HH
#include "GraphEssentials.hh"
#endif

#ifndef SPARSEARRAY_HH
#include "SparseArray.hh"
#endif

class KmerSet : public GraphEssentials<KmerSet>
{
public:
    class Iterator;
    friend class Iterator;

    static constexpr uint64_t version = 2011101701ULL; 
    // Version history
    // 2011101701 Initial Version.

    static const uint64_t MaxK = sizeof(Gossamer::position_type) * 4 - 1;

    struct Header
    {
        uint64_t version;
        uint64_t K;
        uint64_t count;

        Header(const uint64_t& pK)
        {
            version = KmerSet::version;
            K = pK;
            count = 0;
        }

        Header(const std::string& pFileName, FileFactory& pFactory)
        {
            FileFactory::InHolderPtr headerFileHolder(pFactory.in(pFileName));
            std::istream& headerFile(**headerFileHolder);
            headerFile.read(reinterpret_cast<char*>(this), sizeof(Header));
            if (version != KmerSet::version)
            {
                uint64_t v = KmerSet::version;
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << boost::errinfo_file_name(pFileName)
                        << Gossamer::version_mismatch_info(std::pair<uint64_t,uint64_t>(version, v)));
            }
        }
    };

    class Builder
    {
    public:
        void push_back(const Gossamer::position_type& pKmer, uint64_t pCount)
        {
            push_back(pKmer);
        }

        void push_back(const Gossamer::position_type& pKmer)
        {
            mKmerSetBuilder.push_back(pKmer);
            mHeader.count++;
        }

        void end()
        {
            mKmerSetBuilder.end(Gossamer::position_type(1) << (2 * mHeader.K));

            FileFactory::OutHolderPtr op(mFactory.out(mBaseName + ".header"));
            std::ostream& o(**op);
            o.write(reinterpret_cast<const char*>(&mHeader), sizeof(mHeader));
        }

        Builder(uint64_t pK, const std::string& pBaseName, FileFactory& pFactory, uint64_t pM, bool pAsymmetric=false)
            : mBaseName(pBaseName), mFactory(pFactory),
              mHeader(pK),
              mKmerSetBuilder(pBaseName + ".kmers", pFactory, Gossamer::position_type(1) << (2 * pK), pM)
        {
            if (pK > MaxK)
            {
                BOOST_THROW_EXCEPTION(
                    Gossamer::error()
                        << Gossamer::general_error_info("unable to build a graph with k="
                                                            + boost::lexical_cast<std::string>(pK)));
            }
        }

    private:
        const std::string mBaseName;
        FileFactory& mFactory;
        Header mHeader;
        SparseArray::Builder mKmerSetBuilder;
    };

    class Iterator
    {
    public:
        bool valid() const
        {
            return mKmersItr.valid();
        }

        std::pair<Edge,uint32_t> operator*() const
        {
            return std::make_pair<Edge,uint32_t>(Edge(*mKmersItr), 1);
        }

        void operator++()
        {
            ++mKmersItr;
        }

        Iterator(const KmerSet& pKmerSet)
            : mKmersItr(pKmerSet.mKmers.iterator())
        {
        }

    private:
        SparseArray::Iterator mKmersItr;
    };

    class LazyIterator
    {
    public:

        /**
         * Return true iff there is at least one remaining k-mer pair.
         */
        bool valid() const
        {
            return mKmersItr.valid();
        }

        /**
         * Get the current k-mer/count pair (where the count is always 1).
         */
        std::pair<Edge,uint32_t> operator*() const
        {
            return std::pair<Edge,uint32_t>(Edge(*mKmersItr), 1);
        }

        /**
         * Advance the iterator the the next k-mer/count pair it it exists.
         */
        void operator++()
        {
            BOOST_ASSERT(mKmersItr.valid());
            ++mKmersItr;
        }

        /**
         * Return the k-mer size for the set being iterated over.
         */
        uint64_t K() const
        {
            return mHeader.K;
        }

        /**
         * Dummy method, here only to match Graph::LazyIterator's interface.
         */
        bool asymmetric() const
        {
            return false;
        }

        /**
         * Return the edge count for the set being iterated over.
         */
        Gossamer::rank_type count() const
        {
            return mHeader.count;
        }

        LazyIterator(const std::string& pBaseName, FileFactory& pFactory) 
            : mHeader(pBaseName + ".header", pFactory), mKmersItr(pBaseName + ".kmers", pFactory)
        {
        }

    private:

        Header mHeader;
        SparseArray::LazyIterator mKmersItr;
    };

    uint64_t K() const
    {
        return mHeader.K;
    }

    Gossamer::rank_type count() const
    {
        return mHeader.count;
    }

    bool access(const Edge& pEdge) const
    {
        return mKmers.access(pEdge.value());
    }

    Gossamer::rank_type rank(const Edge& pEdge) const
    {
        return mKmers.rank(pEdge.value());
    }

    bool accessAndRank(const Edge& pEdge, Gossamer::rank_type& pRank) const
    {
        return mKmers.accessAndRank(pEdge.value(), pRank);
    }

    std::pair<Gossamer::rank_type,Gossamer::rank_type> rank(const Edge& pLhs, const Edge& pRhs) const
    {
        return mKmers.rank(pLhs.value(), pRhs.value());
    }

    Edge select(Gossamer::rank_type pRank) const
    {
        return Edge(mKmers.select(pRank));
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("kmers", mKmers.stat());

        t.putProp("count", count());
        t.putProp("K", K());

        uint64_t s = sizeof(Header);
        s += t("kmers").as<uint64_t>("storage");
        t.putProp("storage", s);

        return t;
    }

    static void remove(const std::string& pBaseName, FileFactory& pFactory);

    KmerSet(const std::string& pBaseName, FileFactory& pFactory)
        : mHeader(pBaseName + ".header", pFactory),
          mKmers(pBaseName + ".kmers", pFactory)
    {
    }

private:
    const Header mHeader;
    SparseArray mKmers;
};

#endif // KMERSET_HH
