#include <cctype>
#include <fstream>
#include <ios>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <tr1/unordered_map>

#include <stdint.h>
#include <unistd.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "sqlite3.h"

using namespace std;
using namespace std::tr1;
using namespace boost;

namespace {
 
    static const string versionTable = "version";
    static const string nodesTable = "nodes";
    static const string linksTable = "links";
    static const string sequencesTable = "sequences";
    static const string alignmentsTable = "alignments";

    // Reference, direction ("+"/"-"), start, end, gene
    typedef tuple<string, string, uint64_t, uint64_t, string> Ref;
    typedef vector<Ref> Refs;

    // Matched bases, ref name, direction ("+"/"-"), target start, target end
    typedef tuple<uint64_t, string, string, uint64_t, uint64_t> Alignment;  

    // Other contig, gap, count
    typedef tuple<uint64_t, int64_t, uint64_t> Link;
    typedef vector<Link> Links;
 
    enum LinkType { READ_LINK = 0, PAIR_LINK = 1 };

    // Mean coverage
    typedef tuple<double> Coverage;

    const uint64_t version = 2012080201;
    const string versionText("2012080201 format");

    void usage(char* prog)
    {
        cout << "usage: " << prog << " -c <contigs> -l <links> -g <graph_prefix> [-a <alignment_psl>] [-r <refgene>] -o <output> [-h]\n";
    }

    char reverseComplement(char b)
    {
        switch (b)
        {
            case 'A': return 'T';
            case 'C': return 'G';
            case 'G': return 'C';
            case 'T': return 'A';
            case 'a': return 't';
            case 'c': return 'g';
            case 'g': return 'c';
            case 't': return 'a';
            default: return b;
        }
    }

    string reverseComplement(const string& seq)
    {
        string rc;
        rc.reserve(seq.size());
        for (string::const_reverse_iterator i = seq.rbegin(); i != seq.rend(); ++i)
        {
            rc.push_back(reverseComplement(*i));
        }
        return rc;
    }

    uint64_t headerId(const string& header)
    {
        // Skip '>', then parse first integer.
        string id;
        for (uint64_t i = 1; i < header.size(); ++i)
        {
            if (!isdigit(header[i]))
            {
                if (id.empty())
                {
                    continue;
                }
                break;
            }
            else
            {
                id.push_back(header[i]);
            }
        }
        if (!id.empty())
        {
            return lexical_cast<uint64_t>(id);
        }
        return 0;
    }

    Coverage headerCoverage(const string& header)
    {
        vector<string> bits;
        split(bits, header, is_any_of(","));
        double mean = 0.0;
        if (bits.size() >= 6)
        {
            mean = lexical_cast<double>(bits[5]);
        }
        return Coverage(mean);
    }

    bool overlap(uint64_t a0, uint64_t b0, uint64_t a1, uint64_t b1)
    {
        if (a1 < a0)
        {
            if (b1 < a0)
            {
                return false;
            }
            return true;
        }
        else if (a1 > b0)
        {
            return false;
        }
        return true;
    }

    bool match(const Alignment& align, const Ref& ref)
    {
        if (   align.get<1>() != ref.get<0>()
            || align.get<2>() != ref.get<1>())
        {
            return false;
        }

        return overlap(align.get<3>(), align.get<4>(), ref.get<2>(), ref.get<3>());
    }

    string geneId(const Alignment& align, const Refs& refs)
    {
        static const string none("");
        Ref dummy(align.get<1>(), align.get<2>(), align.get<3>(), align.get<4>(), "");
        Refs::const_iterator itr = lower_bound(refs.begin(), refs.end(), dummy);
        if (itr == refs.end())
        {
            return none;
        }

        // Alignment can only match the pointed-to gene, or its immediate predecessor.
        if (match(align, *itr))
        {
            return (*itr).get<4>();
        }

        if (itr == refs.begin())
        {
            return none;
        }

        --itr;
        if (match(align, *itr))
        {
            return (*itr).get<4>();
        }

        return none;
    }

    sqlite3_stmt* sqlPrepare(sqlite3* db, const char* str)
    {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, str, /*strlen(str) + 1*/ -1, &stmt, 0))
        {
            cerr << "Error preparing SQLite3 statement: " << str << '\n';
            sqlite3_close(db);
            exit(1);
        }
        return stmt;
    }

    sqlite3_stmt* sqlPrepare(sqlite3* db, const string& str)
    {
        return sqlPrepare(db, str.c_str());
    }

    void sqlStep(sqlite3* db, const char* str, sqlite3_stmt* stmt)
    {
        int rc;
        if ((rc = sqlite3_step(stmt)) != SQLITE_DONE)
        {
            cerr << "Error stepping SQLite3 statement (" << rc << "): " << sqlite3_errmsg(db) << '\n';
            sqlite3_close(db);
            exit(1);
        }
    }

    void sqlFinalize(sqlite3* db, const char* str, sqlite3_stmt* stmt)
    {
        if (sqlite3_finalize(stmt))
        {
            cerr << "Error finalizing SQLite3 statement: " << str << '\n';
            sqlite3_close(db);
            exit(1);
        }
    }

    void sqlBindInt64(sqlite3* db, sqlite3_stmt* stmt, int var, int64_t val)
    {
        if (sqlite3_bind_int64(stmt, var, val))
        {
            cerr << "Error binding SQLite3 int64 value\n";
            sqlite3_close(db);
            exit(1);
        }
    }

    void sqlBindDouble(sqlite3* db, sqlite3_stmt* stmt, int var, double val)
    {
        if (sqlite3_bind_double(stmt, var, val))
        {
            cerr << "Error binding SQLite3 double value\n";
            sqlite3_close(db);
            exit(1);
        }
    }

    void sqlBindText(sqlite3* db, sqlite3_stmt* stmt, int var, const string& str)
    {
        if (sqlite3_bind_text(stmt, var, str.c_str(), -1, SQLITE_TRANSIENT))
        {
            cerr << "Error binding SQLite3 double value\n";
            sqlite3_close(db);
            exit(1);
        }
    }

    void sqlReset(sqlite3* db, sqlite3_stmt* stmt)
    {
        if (sqlite3_reset(stmt) || sqlite3_clear_bindings(stmt))
        {
            cerr << "Error resetting or clearing SQLite3 statement\n";
            sqlite3_close(db);
            exit(1);
        }
    }

    void sqlRun(sqlite3* db, const char* str)
    {
        sqlite3_stmt* stmt = sqlPrepare(db, str);
        sqlStep(db, str, stmt);
        sqlFinalize(db, str, stmt);
    }

    void sqlRun(sqlite3* db, const string& str)
    {
        const char* p = str.c_str();
        sqlRun(db, p);
    }

    void sqlExec(sqlite3* db, const string& str)
    {
        if (sqlite3_exec(db, str.c_str(), 0, 0, 0))
        {
            cerr << "Error exec'ing or SQLite3 statement: " << str << '\n';
            sqlite3_close(db);
            exit(1);
        }
    }

    class SqlTransaction
    {
    public:
        ~SqlTransaction()
        {
            sqlExec(mDb, "COMMIT;");
        }

        SqlTransaction(sqlite3* db)
            : mDb(db)
        {
            sqlExec(mDb, "BEGIN;");
        }

    private:
        sqlite3* mDb;
    };

    template <uint64_t Mask=0xffff>
    class ProgressMonitor
    {
    public:
        void verboseUpdate(uint64_t n)
        {
            mLastN = n;
            erase();
            print(lexical_cast<string>(n));
        }

        void update(uint64_t n)
        {
            mLastN = n;
            if (!(n % Mask))
            {
                erase();
                print(lexical_cast<string>(n));
            }
        }

        ~ProgressMonitor()
        {
            erase();
            print(lexical_cast<string>(mLastN));
            cerr << '\n';
        }

        ProgressMonitor(const string& base)
            : mLastN(0), mLastLen(0)
        {
            cerr << base;
        }

    private:

        void print(const string& str)
        {
            mLastLen = str.size();
            cerr << str;
        }

        void erase()
        {
            for (uint64_t i = 0; i < mLastLen; ++i)
            {
                cerr << '\b';
            }
        }

        uint64_t mLastN;
        uint64_t mLastLen;
    };


    typedef ProgressMonitor<0x3fff> FastMonitor;
    typedef ProgressMonitor<0xffff> Monitor;
    typedef ProgressMonitor<0x3ffff> SlowMonitor;

};


int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        usage(argv[0]);
        return 1;
    }

    string contigFile;
    string linkFile;
    string rcFile;
    string pslFile;
    string refFile;
    string outFile = "out.db";

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "a:c:g:hl:o:r:")) != -1)
    {
        switch (c)
        {
            case 'a':
                pslFile = optarg;
                break;

            case 'c':
                contigFile = optarg;
                break;

            case 'g':
                rcFile = string(optarg) + "-supergraph.rcs.rc-path-ids";
                break;
                
            case 'h':
                usage(argv[0]);
                return 0;

            case 'l':
                linkFile = optarg;
                break;

            case 'o':
                outFile = optarg;
                break;

            case 'r':
                refFile = optarg;
                break;

            case '?':
                cerr << "unrecognised option -" << (char)optopt << '\n';
                return 1;
        }
    }

    if (contigFile.empty())
    {
        cerr << "no contig file!\n";
        return 1;
    }
    
    if (linkFile.empty())
    {
        cerr << "no link file!\n";
        return 1;
    }
    
    if (rcFile.empty())
    {
        cerr << "no graph prefix!\n";
        return 1;
    }

    if (optind < argc)
    {
        cerr << "unrecognised option " << argv[optind] << '\n';
        return 1;
    }

    unordered_map<uint64_t, string> contigs;
    unordered_map<uint64_t, uint64_t> lengths;
    map<uint64_t, uint64_t> rcs;
    unordered_map<uint64_t, Links> fwdLinks;
    unordered_map<uint64_t, Links> bckLinks;
    unordered_map<uint64_t, Alignment> alignments;
    unordered_map<uint64_t, Coverage> coverage;
    Refs refs;

    // Read input.

    // refgene
    {
        uint64_t n = 0;
        FastMonitor prog("loading gene reference ... ");
        string line;
        ifstream in(refFile.c_str());
        if (in.good())
        {
            getline(in, line);
            while (line.size())
            {
                vector<string> bits;
                split(bits, line, is_any_of("\t"));

                const string& ref = bits[2];
                // const string& dir = bits[3];
                const uint64_t start = lexical_cast<uint64_t>(bits[4]);
                const uint64_t end = lexical_cast<uint64_t>(bits[5]);
                const string& gene = bits[12];
                // refs.push_back(Ref(ref, dir, start, end, gene));
                // Ignore strand.
                refs.push_back(Ref(ref, "-", start, end, gene));
                refs.push_back(Ref(ref, "+", start, end, gene));
                getline(in, line);
                prog.update(++n);
            }
            sort(refs.begin(), refs.end());
        }
    }

    // goto alnload;

    // rcs
    {
        uint64_t n = 0;
        SlowMonitor prog("loading rcs ... ");
        ifstream in(rcFile.c_str(), ios_base::binary);
        for (uint64_t i = 0; ; ++i)
        {
            int64_t j;
            in.read((char*)(&j), 8);
            if (!in.good())
            {
                break;
            }
            rcs.insert(make_pair(i, j));
            prog.update(++n);
        }
    }

    // goto dbstuff;

    // links
    {
        uint64_t n = 0;
        SlowMonitor prog("loading links ... ");
        uint64_t numLinks = 0;
        ifstream in(linkFile.c_str());
        while (in.good())
        {
            while (in.good())
            {
                char k;
                in.get(k);
                if (!isspace(k))
                {
                    in.unget();
                    break;
                }
            }
            if (!in.good())
            {
                break;
            }

            uint64_t l;
            uint64_t r;
            uint64_t c;
            int64_t g;
            in >> l >> r >> c >> g;
            if (in.fail() || in.bad())
            {
                throw "failed to read from scaffold file!";
            }

            numLinks += 1;
            fwdLinks[l].push_back(Link(r, g, c));
            bckLinks[r].push_back(Link(l, g, c));
            prog.update(++n);
        }
    }

    // contigs
    {
        uint64_t n = 0;
        FastMonitor prog("loading contigs ... ");
        stringstream ss;
        ifstream in(contigFile.c_str());
        if (in.good())
        {
            string line;
            getline(in, line);
            while (line.size() && line[0] == '>')
            {
                ss.str("");
                const uint64_t id = headerId(line);
                const uint64_t rc = rcs[id];
                Coverage cov = headerCoverage(line);
                getline(in, line);
                while (line.size() && line[0] != '>')
                {
                    ss << line;
                    getline(in, line);
                }

                string seq = ss.str();
                contigs[id] = seq;
                contigs[rc] = reverseComplement(seq);
                lengths[id] = seq.size();
                lengths[rc] = seq.size();
                coverage[id] = cov;
                coverage[rc] = cov;
                prog.update(++n);
            }
        }
    }

alnload:

    // alignments
    {
        uint64_t n = 0;
        FastMonitor prog("loading alignments ... ");
        ifstream in(pslFile.c_str());
        if (in.good())
        {
            string line;
            getline(in, line);
            // Consume lines until we get to the first one with a leading digit (the first `match' value).
            while (!in.eof() && (line.empty() || !isdigit(line[0])))
            {
                getline(in, line);
            }
            while (line.size())
            {
                vector<string> bits;
                split(bits, line, is_any_of("\t"));

                const uint64_t match = lexical_cast<uint64_t>(bits[0]);
                const uint64_t query = lexical_cast<uint64_t>(bits[9]);
                const string& dir = bits[8];
                const string& target = bits[13];
                const uint64_t start = lexical_cast<uint64_t>(bits[15]);
                const uint64_t end = lexical_cast<uint64_t>(bits[16]);
                
                unordered_map<uint64_t, Alignment>::const_iterator itr(alignments.find(query));
                Alignment align(match, target, dir, start, end);
                if (itr == alignments.end() || itr->second.get<0>() < match)
                {
                    alignments[query] = align;
                    prog.update(++n);
                }

                getline(in, line);
            }
        }
    }


    // Validate input.
    // TODO!

dbstuff:

    // Open the database.
    sqlite3* db;
    if (sqlite3_open(outFile.c_str(), &db))
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << '\n';
        sqlite3_close(db);
        return 1;
    }

    // Optimise for bulk writes.
    sqlExec(db, "PRAGMA synchronous = OFF");
    sqlExec(db, "PRAGMA journal_mode = OFF");

    // goto alnstuff;

    // Create tables.
    {
        SqlTransaction tn(db);
        sqlExec(db, "DROP TABLE IF EXISTS " + versionTable + ";");
        sqlExec(db, "DROP TABLE IF EXISTS " + nodesTable + ";");
        sqlExec(db, "DROP TABLE IF EXISTS " + linksTable + ";");
        sqlExec(db, "DROP TABLE IF EXISTS " + sequencesTable + ";");
        sqlExec(db, "DROP TABLE IF EXISTS " + alignmentsTable + ";");
    }
    sqlExec(db, "CREATE TABLE IF NOT EXISTS " + versionTable + " (version INTEGER, description TEXT);");
    sqlExec(db, "CREATE TABLE IF NOT EXISTS " + nodesTable + " (id INTEGER PRIMARY KEY ASC, rc INTEGER, cov_mean REAL, length INTEGER);");
    sqlExec(db, "CREATE TABLE IF NOT EXISTS " + linksTable + " (id_from INTEGER, id_to INTEGER, gap INTEGER, count INTEGER, type INTEGER);");
    sqlExec(db, "CREATE TABLE IF NOT EXISTS " + sequencesTable + " (id INTEGER PRIMARY KEY ASC, sequence TEXT);");
    sqlExec(db, "CREATE TABLE IF NOT EXISTS " + alignmentsTable + " (id INTEGER PRIMARY KEY ASC, name TEXT, start INTEGER, end INTEGER, matchLen INTEGER, dir INTEGER, gene TEXT);");

    // version table
    sqlExec(db, "INSERT INTO " + versionTable + " VALUES (" + lexical_cast<string>(version) + ", \"" + versionText + "\");");

    {
        uint64_t n = 0;
        Monitor prog("populating nodes ... ");
        SqlTransaction tn(db);
        sqlite3_stmt* stmt = sqlPrepare(db, "INSERT INTO " + nodesTable + " VALUES (?, ?, ?, ?);");
        for (map<uint64_t, uint64_t>::const_iterator i = rcs.begin(); i != rcs.end(); ++i)
        {
            prog.update(++n);
            const int64_t id = (int64_t)i->first;
            const int64_t rc = (int64_t)i->second;
            unordered_map<uint64_t, Coverage>::const_iterator c = coverage.find(id);
            const double covMean = c != coverage.end() ? c->second.get<0>() : 0.0;
            unordered_map<uint64_t, uint64_t>::const_iterator l = lengths.find(id);
            const int64_t length = l != lengths.end() ? l->second : 0;
            sqlBindInt64(db, stmt, 1, id);
            sqlBindInt64(db, stmt, 2, rc);
            sqlBindDouble(db, stmt, 3, covMean);
            sqlBindInt64(db, stmt, 4, length);
            sqlStep(db, "", stmt);
            sqlReset(db, stmt);
        }
        sqlFinalize(db, "", stmt);
    }

    {
        uint64_t n = 0;
        Monitor prog("populating links ... ");
        SqlTransaction tn(db);
        sqlite3_stmt* stmt = sqlPrepare(db, "INSERT INTO " + linksTable + " VALUES (?, ?, ?, ?, ?);");
        for (unordered_map<uint64_t, Links>::const_iterator i = fwdLinks.begin(); i != fwdLinks.end(); ++i)
        {
            const int64_t id_from = (int64_t)i->first;
            for (Links::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
            {
                prog.update(++n);
                const int64_t id_to = (int64_t)j->get<0>();
                const int64_t gap = (int64_t)j->get<1>();
                const int64_t count = (int64_t)j->get<2>();
                const int64_t type = PAIR_LINK;
                sqlBindInt64(db, stmt, 1, id_from);
                sqlBindInt64(db, stmt, 2, id_to);
                sqlBindInt64(db, stmt, 3, gap);
                sqlBindInt64(db, stmt, 4, count);
                sqlBindInt64(db, stmt, 5, type);
                sqlStep(db, "", stmt);
                sqlReset(db, stmt);
            }
        }
        sqlFinalize(db, "", stmt);
    }
    // Note: Creating the indexes is incredibly slow!
    // Consider having two tables, rather than two indexes!
    //
    {
        Monitor prog("creating link indices ... ");
        SqlTransaction tn(db);
        sqlExec(db, "CREATE INDEX index_from ON " + linksTable + " (id_from);");
        prog.verboseUpdate(1);
        sqlExec(db, "CREATE INDEX index_to ON " + linksTable + " (id_to);");
        prog.verboseUpdate(2);
    }

    {
        uint64_t n = 0;
        Monitor prog("populating sequences ... ");
        SqlTransaction tn(db);
        sqlite3_stmt* stmt = sqlPrepare(db, "INSERT INTO " + sequencesTable + " VALUES (?, ?);");
        for (unordered_map<uint64_t, string>::const_iterator i = contigs.begin(); i != contigs.end(); ++i)
        {
            prog.update(++n);
            const int64_t id = (int64_t)i->first;
            const string& seq = i->second;
            sqlBindInt64(db, stmt, 1, id);
            sqlBindText(db, stmt, 2, seq);
            sqlStep(db, "", stmt);
            sqlReset(db, stmt);
        }
        sqlFinalize(db, "", stmt);
    }

alnstuff:
    {
        uint64_t n = 0;
        Monitor prog("populating alignments ... ");
        SqlTransaction tn(db);
        sqlite3_stmt* stmt = sqlPrepare(db, "INSERT INTO " + alignmentsTable + " VALUES (?, ?, ?, ?, ?, ?, ?);");
        for (unordered_map<uint64_t, Alignment>::const_iterator i = alignments.begin(); i != alignments.end(); ++i)
        {
            prog.update(++n);
            const Alignment& aln = i->second;
            const int64_t id = (int64_t)i->first;
            const int64_t matchLen = (int64_t)aln.get<0>();
            const string& ref = aln.get<1>();
            const int64_t dir = aln.get<2>() == "+" ? 1 : -1;
            const int64_t start = (int64_t)aln.get<3>();
            const int64_t end = (int64_t)aln.get<4>();
            const string gene = geneId(aln, refs);
            sqlBindInt64(db, stmt, 1, id);
            sqlBindText(db, stmt, 2, ref);
            sqlBindInt64(db, stmt, 3, start);
            sqlBindInt64(db, stmt, 4, end);
            sqlBindInt64(db, stmt, 5, matchLen);
            sqlBindInt64(db, stmt, 6, dir);
            sqlBindText(db, stmt, 7, gene);
            sqlStep(db, "", stmt);
            sqlReset(db, stmt);
        }
        sqlFinalize(db, "", stmt);
    }

    
    {
        Monitor prog("creating gene index ... ");
        SqlTransaction tn(db);
        sqlExec(db, "CREATE INDEX index_gene ON " + alignmentsTable + " (gene);");
        prog.verboseUpdate(1);
    }

    {
        Monitor prog("creating ref index ... ");
        SqlTransaction tn(db);
        sqlExec(db, "CREATE INDEX index_name ON " + alignmentsTable + " (name);");
        prog.verboseUpdate(1);
    }

    {
        Monitor prog("creating start index ... ");
        SqlTransaction tn(db);
        sqlExec(db, "CREATE INDEX index_start ON " + alignmentsTable + " (start);");
        prog.verboseUpdate(1);
    }

    {
        Monitor prog("creating end index ... ");
        SqlTransaction tn(db);
        sqlExec(db, "CREATE INDEX index_end ON " + alignmentsTable + " (end);");
        prog.verboseUpdate(1);
    }

    sqlite3_close(db);

    cerr << "done!\n";
    return 0;
}
