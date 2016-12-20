// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdBuildDb.hh"

#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "Debug.hh"
#include "EdgeIndex.hh"
#include "EntryEdgeSet.hh"
#include "EstimateGraphStatistics.hh"
#include "ExternalBufferSort.hh"
#include "FastaParser.hh"
#include "FastqParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "Graph.hh"
#include "LineParser.hh"
#include "PairAligner.hh"
#include "PairLink.hh"
#include "PairLinker.hh"
#include "ProgressMonitor.hh"
#include "ReadPairSequenceFileSequence.hh"
#include "ScaffoldGraph.hh"
#include "SuperGraph.hh"
#include "SuperPathId.hh"
#include "Timer.hh"
#include "TrivialVector.hh"

#include <algorithm>
#include <boost/noncopyable.hpp>
#include <boost/lexical_cast.hpp>
#include <thread>
#include <boost/tuple/tuple.hpp>
#include <unordered_set>
#include <iostream>
#include <math.h>
#include <sqlite3.h>
#include <string>

using namespace boost;
using namespace boost::program_options;
using namespace std;

typedef vector<string> strings;

typedef pair<SuperPathId,SuperPathId> Link;
typedef pair<int64_t, int64_t> Offsets;

// std::hash
namespace std {
    template<>
    struct hash<Link>
    {
        std::size_t operator()(const Link& pValue) const
        {
            BigInteger<2> l(pValue.first.value());
            BigInteger<2> r(pValue.second.value());
            l <<= 64;
            l += r;
            return l.hash();
        }
    };
}

namespace Sql {
    class Database : private boost::noncopyable
    {
        friend class Statement;
    public:

        bool tableExists(const string& str);

        void exec(const string& str)
        {
            if (sqlite3_exec(mDb, str.c_str(), 0, 0, 0))
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error exec'ing SQLite3 statement: " + str + ", `" + string(sqlite3_errmsg(mDb)) + "'"));
            }
        }

        ~Database() 
        {
            sqlite3_close(mDb);
        }

        Database(const string& pDbFile, Logger& log)
            : mLog(log), mDbFile(pDbFile), mDb(0)
        {
            if (sqlite3_open(pDbFile.c_str(), &mDb) != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("can't open database: " + string(sqlite3_errmsg(mDb))));
            }
        }

    private:
    
        sqlite3* getHandle()
        {
            return mDb;
        }

        Logger& mLog;
        const string mDbFile;
        sqlite3* mDb;
    };

    class Statement : private boost::noncopyable
    {
    public:

        int step()
        {
            int rc = sqlite3_step(mStmt);
            if (rc != SQLITE_DONE && rc != SQLITE_ROW && rc != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error stepping SQLite3 statement '" + mStr + "' (" + lexical_cast<string>(rc) + "): " + sqlite3_errmsg(mDb.getHandle())));
            }
            return rc;
        }

        void reset()
        {
            if (sqlite3_reset(mStmt) || sqlite3_clear_bindings(mStmt))
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error resetting or clearing SQLite3 statement `" + string(sqlite3_errmsg(mDb.getHandle())) + "'"));
            }
        }

        void bind(int pVar, int64_t pVal)
        {
            if (sqlite3_bind_int64(mStmt, pVar, pVal) != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error binding SQLite3 int64 value `" + string(sqlite3_errmsg(mDb.getHandle())) + "'"));
            }
        }

        void bind(int pVar, double pVal)
        {
            if (sqlite3_bind_double(mStmt, pVar, pVal) != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error binding SQLite3 double value `" + string(sqlite3_errmsg(mDb.getHandle())) + "'"));
            }
        }

        void bind(int pVar, const string& pVal)
        {
            if (sqlite3_bind_text(mStmt, pVar, pVal.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error binding SQLite3 text value `" + string(sqlite3_errmsg(mDb.getHandle())) + "'"));
            }
        }

        int64_t columnInt64(int pCol)
        {
            return sqlite3_column_int64(mStmt, pCol);
        }

        ~Statement()
        {
            if (sqlite3_finalize(mStmt) != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error finalizing SQLite3 statement `" + string(sqlite3_errmsg(mDb.getHandle())) + "'"));
            }
        }

        Statement(Database& pDb, const string& pStr) 
            : mStr(pStr), mDb(pDb), mStmt(0)
        {
            if (sqlite3_prepare_v2(mDb.getHandle(), pStr.c_str(), -1, &mStmt, 0) != SQLITE_OK)
            {
                BOOST_THROW_EXCEPTION(Gossamer::error()
                    << Gossamer::general_error_info("error preparing SQLite3 statement: " + pStr + " `" + string(sqlite3_errmsg(mDb.getHandle())) + "'"));
            }
        }

    private:
        
        const string mStr;
        Database& mDb;
        sqlite3_stmt* mStmt;
    };

    class Transaction : private boost::noncopyable
    {
    public:
        ~Transaction()
        {
            mDb.exec("COMMIT;");
        }

        Transaction(Database& pDb)
            : mDb(pDb)
        {
            mDb.exec("BEGIN;");
        }

    private:
        Database& mDb;
    };


    bool 
    Database::tableExists(const string& str)
    {
        string sql = "SELECT count(*) FROM sqlite_master WHERE name='" + str + "';";
        Statement stmt(*this, sql);
        stmt.step();
        return stmt.columnInt64(0);
    }
}   // namespace Sql

namespace // anonymous
{
    enum LinkType { READ_LINK = 0, PAIR_LINK = 1 }; // , MATE_LINK = 2 };

    static const string versionTable = "version";
    static const string nodesTable = "nodes";
    static const string linksTable = "links";
    static const string sequencesTable = "sequences";
    static const string alignmentsTable = "alignments";

    template <typename Dest>
    class LinkMapCompiler
    {
    public:
        void push_back(const vector<uint8_t>& pItem)
        {
            const uint8_t* hd(&pItem[0]);
            PairLink::decode(hd, mLink);

            // cerr << "c\t" << mLink.lhs.value() << '\t' << mLink.rhs.value() << '\n';

            if (mLink.lhs == mPrev.lhs && mLink.rhs == mPrev.rhs)
            {
                mCount++;
                mLhsOffsetSum += mLink.lhsOffset;
                mLhsOffsetSum2 += mLink.lhsOffset * mLink.lhsOffset;
                mRhsOffsetSum += mLink.rhsOffset;
                mRhsOffsetSum2 += mLink.rhsOffset * mLink.rhsOffset;
                return;
            }

            if (mCount > 0)
            {
                mDest.push_back(mPrev.lhs, mPrev.rhs, mCount,
                                mLhsOffsetSum, mLhsOffsetSum2,
                                mRhsOffsetSum, mRhsOffsetSum2);
            }
            mCount = 1;
            mLhsOffsetSum = mLink.lhsOffset;
            mLhsOffsetSum2 = mLink.lhsOffset * mLink.lhsOffset;
            mRhsOffsetSum = mLink.rhsOffset;
            mRhsOffsetSum2 = mLink.rhsOffset * mLink.rhsOffset;
            mPrev = mLink;
        }

        void end()
        {
            if (mCount > 0)
            {
                mDest.push_back(mPrev.lhs, mPrev.rhs, mCount,
                                mLhsOffsetSum, mLhsOffsetSum2,
                                mRhsOffsetSum, mRhsOffsetSum2);
            }
            mDest.end();
        }

        LinkMapCompiler(Dest& pDest)
            : mDest(pDest),
              mPrev(SuperPathId(-1LL), SuperPathId(-1LL), 0, 0),
              mLink(SuperPathId(0), SuperPathId(0), 0, 0),
              mCount(0),
              mLhsOffsetSum(0), mLhsOffsetSum2(0),
              mRhsOffsetSum(0), mRhsOffsetSum2(0)
        {
        }

    private:
        Dest& mDest;
        PairLink mPrev;
        PairLink mLink;
        uint64_t mCount;
        int64_t mLhsOffsetSum;
        uint64_t mLhsOffsetSum2;
        int64_t mRhsOffsetSum;
        uint64_t mRhsOffsetSum2;
    };

    // TODO: Consider, for mate-pairs, eliminating links that join short contigs.
    // e.g. contigs shorter than 1/2 the insert size.
    // Alternatively - in downstream processing - only consider mate-pairs links that
    // join otherwise disconnected components.
    template <typename Dest>
    class LinkFilter
    {
    public:
        void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                       const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                       const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
        {
            // Eliminate links which are too far apart, 
            // i.e. cannot be spanned by any in-bounds path.
            //
            // Lhs              Rhs
            // ----------.......-------------
            //     ^     ^            ^
            //     +-----+-----------+
            //     |     |           |
            //   lhsAvg  lhsSize     rhsAvg
            //
            int64_t lhsAvg = pLhsOffsetSum / pCount;
            int64_t rhsAvg = pRhsOffsetSum / pCount;
            int64_t minDist =   rhsAvg + static_cast<int64_t>(mSuperGraph[pLhs].size(mEntries) + mK) - lhsAvg;

            if (minDist > mMaxInsertSize)
            {
                return;
            }

            // cerr << "f\t" << pLhs.value() << '\t' << pRhs.value() << '\t' << pCount << '\n';

            mDest.push_back(pLhs, pRhs, pCount,
                            pLhsOffsetSum, pLhsOffsetSum2, pRhsOffsetSum, pRhsOffsetSum2);
        }

        void end()
        {
            mDest.end();
        }

        LinkFilter(Dest& pDest, uint64_t pMaxInsertSize,
                   const SuperGraph& pSuperGraph, const EntryEdgeSet& pEntries)
            : mDest(pDest), mMaxInsertSize(pMaxInsertSize),
              mSuperGraph(pSuperGraph), mEntries(pEntries), mK(mEntries.K())
        {
        }

    private:
        Dest& mDest;
        int64_t mMaxInsertSize;
        const SuperGraph& mSuperGraph;
        const EntryEdgeSet& mEntries;
        const uint64_t mK;
    };

    class LinkWriter
    {
    public:
        void push_back(const SuperPathId& pLhs, const SuperPathId& pRhs, const uint64_t& pCount,
                       const int64_t& pLhsOffsetSum, const uint64_t& pLhsOffsetSum2,
                       const int64_t& pRhsOffsetSum, const uint64_t& pRhsOffsetSum2)
        {
            // cerr << pLhs.value() << '\t' << pRhs.value() << '\n';
            if (!pCount)
            {
                return;
            }
            const int64_t lOfs = pLhsOffsetSum / int64_t(pCount);
            const int64_t rOfs = pRhsOffsetSum / int64_t(pCount);
            const int64_t len = mSg.baseSize(pLhs) - lOfs + rOfs;
            int64_t gap = int64_t(mInsertSize) - len;
            int64_t count = pCount;

            if (!mFresh)
            {
                // Combine results.
                int64_t oldGap = 0;
                int64_t oldCount = 0;
                read(pLhs, pRhs, oldGap, oldCount);
                int64_t sumGap = count * gap + oldCount * oldGap;
                count = count + oldCount;
                gap = sumGap / count;

                // Remove the old row.
                erase(pLhs, pRhs);
            }
            write(pLhs, pRhs, gap, count);
        }

        void end()
        {
        }

        LinkWriter(const SuperGraph& pSg, uint64_t pInsertSize, Sql::Database& pDb, bool pFresh)
            : mSg(pSg), mInsertSize(pInsertSize), mFresh(pFresh),
              mRead(pDb, "SELECT ALL gap, count FROM " + linksTable + " WHERE id_from=? and id_to=?;"),
              mWrite(pDb, "INSERT INTO " + linksTable + " VALUES (?, ?, ?, ?, ?);"),
              mDelete(pDb, "DELETE FROM " + linksTable + " WHERE id_from=? and id_to=?;"),
              mTrans(pDb)
        {
        }

    private:

        bool read(const SuperPathId& pLhs, const SuperPathId& pRhs, int64_t& pGap, int64_t& pCount)
        {
            mRead.bind(1, int64_t(pLhs.value()));
            mRead.bind(2, int64_t(pRhs.value()));
            if (mRead.step() != SQLITE_ROW)
            {
                mRead.reset();
                return false;
            }
            pGap = mRead.columnInt64(0);
            pCount = mRead.columnInt64(1);
            mRead.reset();
            return true;
        }

        void write(const SuperPathId& pLhs, const SuperPathId& pRhs, int64_t pGap, int64_t pCount)
        {
                mWrite.bind(1, int64_t(pLhs.value()));
                mWrite.bind(2, int64_t(pRhs.value()));
                mWrite.bind(3, pGap);
                mWrite.bind(4, int64_t(pCount));
                mWrite.bind(5, int64_t(PAIR_LINK));
                mWrite.step();
                mWrite.reset();
        }

        void erase(const SuperPathId& pLhs, const SuperPathId& pRhs)
        {
            mDelete.bind(1, int64_t(pLhs.value()));
            mDelete.bind(2, int64_t(pRhs.value()));
            mDelete.step();
            mDelete.reset();
        }

        const SuperGraph& mSg;
        const uint64_t mInsertSize;
        const bool mFresh;
        Sql::Statement mRead;
        Sql::Statement mWrite;
        Sql::Statement mDelete;
        Sql::Transaction mTrans;
    };

} // namespace anonymous


void
GossCmdBuildDb::operator()(const GossCmdContext& pCxt)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);

    Timer t;

    // Open database and optimise for bulk writes.
    Sql::Database db(mDb, log);
    db.exec("PRAGMA synchronous = OFF");
    db.exec("PRAGMA journal_mode = OFF");

    // Check if the database is already populated.
    bool popd = true;
    popd = popd && db.tableExists(versionTable);
    popd = popd && db.tableExists(nodesTable);
    popd = popd && db.tableExists(linksTable);
    popd = popd && db.tableExists(sequencesTable);
    popd = popd && db.tableExists(alignmentsTable);
    
    bool fresh = !popd || mResetDb;
    if (fresh)
    {
        // Create tables.
        {
            Sql::Transaction tn(db);
            db.exec("DROP TABLE IF EXISTS " + versionTable + ";");
            db.exec("DROP TABLE IF EXISTS " + nodesTable + ";");
            db.exec("DROP TABLE IF EXISTS " + linksTable + ";");
            db.exec("DROP TABLE IF EXISTS " + sequencesTable + ";");
            db.exec("DROP TABLE IF EXISTS " + alignmentsTable + ";");
        }
        db.exec("CREATE TABLE IF NOT EXISTS " + versionTable + " (version INTEGER, description TEXT);");
        db.exec("CREATE TABLE IF NOT EXISTS " + nodesTable + " (id INTEGER PRIMARY KEY ASC, rc INTEGER, cov_mean REAL, length INTEGER);");
        db.exec("CREATE TABLE IF NOT EXISTS " + linksTable + " (id_from INTEGER, id_to INTEGER, gap INTEGER, count INTEGER, type INTEGER);");
        db.exec("CREATE TABLE IF NOT EXISTS " + sequencesTable + " (id INTEGER PRIMARY KEY ASC, sequence TEXT);");
        db.exec("CREATE TABLE IF NOT EXISTS " + alignmentsTable + " (id INTEGER PRIMARY KEY ASC, name TEXT, start INTEGER, end INTEGER, matchLen INTEGER, dir INTEGER, gene TEXT);");
    }
    db.exec("CREATE INDEX IF NOT EXISTS index_from ON " + linksTable + " (id_from);");
    db.exec("CREATE INDEX IF NOT EXISTS index_to ON " + linksTable + " (id_to);");

    log(info, "loading supergraph");
    auto sgp = SuperGraph::read(mIn, fac);
    SuperGraph& sg(*sgp);

    log(info, "loading graph");
    GraphPtr gPtr = Graph::open(mIn, fac);
    Graph& g(*gPtr);
    if (g.asymmetric())
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::general_error_info("Asymmetric graphs not yet handled")
            << Gossamer::open_graph_name_info(mIn));
    }

    if (fresh)
    {
        storeContigs(pCxt, g, sg, db);
    }
    if (mLines.size() || mFastas.size() || mFastqs.size())
    {
	storeReadLinks(pCxt, g, sg, db, fresh);
    }
    if (mIncludeGraphLinks)
    {
	storeGraphLinks(pCxt, g, sg, db);
    }

    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}

void
GossCmdBuildDb::storeContigs(const GossCmdContext& pCxt, const Graph& pG, const SuperGraph& pSg, Sql::Database& pDb)
{
    Logger& log(pCxt.log);
    ProgressMonitorNew mon(log, pSg.count());
    uint64_t n = 0;
    log(info, "storing contig information");
    Sql::Transaction tn(pDb);
    Sql::Statement sNode(pDb, "INSERT INTO " + nodesTable + " VALUES (?, ?, ?, ?);");
    Sql::Statement sSeq(pDb, "INSERT INTO " + sequencesTable + " VALUES (?, ?);");
    for (SuperGraph::PathIterator itr(pSg); itr.valid(); ++itr)
    {
        mon.tick(++n);
        const SuperPathId id = *itr;
        SuperPathId rc(0);
        double covMean;
        string seq;
        pSg.contigInfo(pG, id, seq, rc, covMean);

        sNode.bind(1, int64_t(id.value()));
        sNode.bind(2, int64_t(rc.value()));
        sNode.bind(3, covMean);
        sNode.bind(4, int64_t(seq.length()));
        sNode.step();
        sNode.reset();

        sSeq.bind(1, int64_t(id.value()));
        sSeq.bind(2, seq);
        sSeq.step();
        sSeq.reset();
    }
}

void
GossCmdBuildDb::storeReadLinks(const GossCmdContext& pCxt, const Graph& pG, const SuperGraph& pSg,  
                               Sql::Database& pDb, bool pFresh)
{
    Logger& log(pCxt.log);
    FileFactory& fac(pCxt.fac);
    log(info, "storing read links");

    // Infer coverage if we need to.
    uint64_t coverage = mExpectedCoverage;
    if (mInferCoverage)
    {
        log(info, "inferring coverage");
        Graph::LazyIterator itr(mIn, fac);
        if (itr.asymmetric())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error()
                << Gossamer::general_error_info("Asymmetric graphs not yet handled")
                << Gossamer::open_graph_name_info(mIn));
        }
        map<uint64_t,uint64_t> h = Graph::hist(mIn, fac);
        EstimateCoverageOnly estimator(log, h);
        if (!estimator.modelFits())
        {
            BOOST_THROW_EXCEPTION(Gossamer::error() << Gossamer::general_error_info("Could not infer coverage."));
        }
        coverage = static_cast<uint64_t>(estimator.estimateRhomerCoverage());
        log(info, "estimated coverage = " + lexical_cast<string>(coverage));
        if (mEstimateOnly)
        {
            return;
        }
    }

    const EntryEdgeSet& entries(pSg.entries());
    map<int64_t,uint64_t> dist;
    ExternalBufferSort sorter(1024ULL * 1024ULL * 1024ULL, fac);
    log(info, "constructing edge index");

    auto idxPtr = EdgeIndex::create(pG, entries, pSg, mCacheRate, mNumThreads, log);
    EdgeIndex& idx(*idxPtr);
    const PairAligner alnr(pG, entries, idx);

    std::deque<GossReadSequence::Item> items;

    {
        GossReadSequenceFactoryPtr seqFac
            = std::make_shared<GossReadSequenceBasesFactory>();

        GossReadParserFactory lineParserFac(LineParser::create);
        for (auto& f : mLines)
        {
            items.push_back(GossReadSequence::Item(f, lineParserFac, seqFac));
        }

        GossReadParserFactory fastaParserFac(FastaParser::create);
        for (auto& f : mFastas)
        {
            items.push_back(GossReadSequence::Item(f, fastaParserFac, seqFac));
        }

        GossReadParserFactory fastqParserFac(FastqParser::create);
        for (auto& f : mFastqs)
        {
            items.push_back(GossReadSequence::Item(f, fastqParserFac, seqFac));
        }
    }

    UnboundedProgressMonitor umon(log, 100000, " read pairs");
    LineSourceFactory lineSrcFac(BackgroundLineSource::create);
    ReadPairSequenceFileSequence reads(items, fac, lineSrcFac, &umon, &log);

    log(info, "mapping pairs.");

    UniquenessCache ucache(pSg, coverage);
    BackgroundMultiConsumer<PairLinker::ReadPair> grp(128);
    vector<PairLinkerPtr> linkers;
    mutex mut;
    for (uint64_t i = 0; i < mNumThreads; ++i)
    {
        linkers.push_back(PairLinkerPtr(new PairLinker(pSg, alnr, mOrientation, ucache, sorter, mut)));
        grp.add(*linkers.back());
    }

    while (reads.valid())
    {
        grp.push_back(PairLinker::ReadPair(reads.lhs().clone(), reads.rhs().clone()));
        ++reads;
    }
    grp.wait();

    const double dev = mInsertTolerance * mInsertStdDevFactor * mExpectedInsertSize;
    const uint64_t maxInsertSize = mExpectedInsertSize + dev;

    LinkWriter writer(pSg, mExpectedInsertSize, pDb, pFresh);
    LinkFilter<LinkWriter> filter(writer, maxInsertSize, pSg, entries);
    LinkMapCompiler<LinkFilter<LinkWriter> > compiler(filter);
    sorter.sort(compiler);
}

void
GossCmdBuildDb::storeGraphLinks(const GossCmdContext& pCxt, const Graph& pG, const SuperGraph& pSg, Sql::Database& pDb)
{
    Logger& log(pCxt.log);
    ProgressMonitorNew mon(log, pSg.count());
    uint64_t n = 0;
    log(info, "storing graph links");
    {
        Sql::Transaction tn(pDb);
        pDb.exec("DROP TABLE IF EXISTS " + linksTable + ";");
        pDb.exec("CREATE TABLE IF NOT EXISTS " + linksTable + " (id_from INTEGER, id_to INTEGER, gap INTEGER, count INTEGER, type INTEGER);");
        pDb.exec("CREATE INDEX IF NOT EXISTS index_from ON " + linksTable + " (id_from);");
        pDb.exec("CREATE INDEX IF NOT EXISTS index_to ON " + linksTable + " (id_to);");
    }
    Sql::Transaction tn(pDb);
    Sql::Statement write(pDb, "INSERT INTO " + linksTable + " VALUES (?, ?, ?, ?, ?);");

    const EntryEdgeSet& entries(pSg.entries());
    SuperGraph::SuperPathIds succs;
    for (SuperGraph::PathIterator i(pSg); i.valid(); ++i)
    {
        mon.tick(++n);
	const SuperPathId a(*i);
	const SuperPath& p(pSg[a]);
	SuperGraph::Node end(p.end(entries));
	succs.clear();
	pSg.successors(end, succs);
	for (SuperGraph::SuperPathIds::const_iterator j = succs.begin(); j != succs.end(); ++j)
	{
	    const SuperPathId b(*j);
	    write.bind(1, int64_t(a.value()));
	    write.bind(2, int64_t(b.value()));
	    write.bind(3, int64_t(0));
	    write.bind(4, int64_t(1));
	    write.bind(5, int64_t(PAIR_LINK));	    // FIX!
	    write.step();
	    write.reset();
	}
    }
}

GossCmdPtr
GossCmdFactoryBuildDb::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);
    FileFactory& fac(pApp.fileFactory());
    GossOptionChecker::FileReadCheck readChk(fac);

    string in;
    chk.getRepeatingOnce("graph-in", in);

    string db;
    chk.getMandatory("db-out", db);

    bool reset;
    chk.getOptional("reset", reset);

    bool incGraph;
    chk.getOptional("include-graph-links", incGraph);

    strings fastas;
    chk.getRepeating0("fasta-in", fastas, readChk);

    strings fastqs;
    chk.getRepeating0("fastq-in", fastqs, readChk);

    strings lines;
    chk.getRepeating0("line-in", lines, readChk);

    uint64_t expectedCoverage = 0;
    bool inferCoverage = true;
    if (chk.getOptional("expected-coverage", expectedCoverage))
    {
        inferCoverage = false;
    }

    // Only mandatory if input files are provided.
    uint64_t expectedSize = 0;
    if (fastas.size() + fastqs.size() + lines.size())
    {
        chk.getMandatory("insert-expected-size", expectedSize);
    }
    else
    {
        chk.getOptional("insert-expected-size", expectedSize);
    }

    double stdDev = 10.0;
    chk.getOptional("insert-size-std-dev", stdDev);
    double stdDevFactor = stdDev / 100.0;

    double tolerance = 2.0;
    chk.getOptional("insert-size-tolerance", tolerance);

    chk.exclusive("paired-ends", "mate-pairs", "innies", "outies");
    bool pe = false;
    chk.getOptional("paired-ends", pe);

    bool mp = false;
    chk.getOptional("mate-pairs", mp);

    bool inn = false;
    chk.getOptional("innies", inn);

    bool out = false;
    chk.getOptional("outies", out);

    uint64_t T = 4;
    chk.getOptional("num-threads", T);

    uint64_t cr = 4;
    chk.getOptional("edge-cache-rate", cr);

    bool estimateOnly = false;
    chk.getOptional("estimate-only", estimateOnly);

    if (estimateOnly && !inferCoverage)
    {
        BOOST_THROW_EXCEPTION(Gossamer::error()
            << Gossamer::usage_info("Must be inferring coverage to estimate-only."));
    }

    PairLinker::Orientation o(  mp ? PairLinker::MatePairs
                              : inn ? PairLinker::Innies
                              : out ? PairLinker::Outies
                              : PairLinker::PairedEnds);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdBuildDb(in, db, reset, incGraph,
			    fastas, fastqs, lines,
			    inferCoverage, expectedCoverage, expectedSize, stdDevFactor,
			    tolerance, o, T, cr, estimateOnly));
}

GossCmdFactoryBuildDb::GossCmdFactoryBuildDb()
    : GossCmdFactory("produce a database of contig, and optionally link, information")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("fasta-in");
    mCommonOptions.insert("fastq-in");
    mCommonOptions.insert("line-in");
    mCommonOptions.insert("expected-coverage");
    mCommonOptions.insert("edge-cache-rate");
    mCommonOptions.insert("estimate-only");
    mCommonOptions.insert("paired-ends");
    mCommonOptions.insert("mate-pairs");
    mCommonOptions.insert("innies");
    mCommonOptions.insert("outies");
    mCommonOptions.insert("insert-expected-size");
    mCommonOptions.insert("insert-size-std-dev");
    mCommonOptions.insert("insert-size-tolerance");

    mSpecificOptions.addOpt<string>("db-out", "", 
            "name of the output database");
    mSpecificOptions.addOpt<bool>("reset-db", "",
            "clear the database before populating it, if it already exists");
    mSpecificOptions.addOpt<bool>("include-graph-links", "",
	    "add edge links from the supergraph");
}
