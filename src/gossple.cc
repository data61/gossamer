// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifdef KILL_SIGNAL_SUPPORT
#include "GossKillSignal.hh"
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/xpressive/xpressive.hpp>
#include <boost/filesystem.hpp>

using namespace std;

static int sTotalStages = 0;
static int sCurrStage = 0;

void usage(bool pPrintGossCmds)
{
    cout << "usage: $0 [options and files]...." << endl;
    cout << "  -B <buffer-size>        amount of buffer space to use in GB (default 2)." << endl;
    cout << "  -c <coverage>           coverage estimate to use during pair/read threading." << endl;
    cout << "                          (defaults to using automatic coverage estimation)" << endl;
    cout << "  -D                      don't execute the goss commands." << endl;
    cout << "  -h                      print this help message (add -v for a longer version)." << endl;
    cout << "  -k <kmer-size>          kmer size for the de Bruijn graph (default 27, max 62)." << endl;
    cout << "  -m <insert-size>        introduce files containing mate pairs with the given insert size." << endl;
    cout << "                          files following (until the next -m or -p) will be treated as mate pairs." << endl;
    cout << "  -o <file-name>          file-name for the contigs. (default 'contigs.fa')" << endl;
    cout << "  -p <insert-size>        introduce files containing paired end reads with the given insert size." << endl;
    cout << "                          files following (until the next -m or -p) will be treated as paired ends." << endl;
    cout << "  -T <num-threads>        use the given number of worker threads (default 4)." << endl;
    cout << "  -t <coverage>           trim edges with coverage less than the given value." << endl;
    cout << "                          (defaults to using automatic trim value estimation)" << endl;
    cout << "  -V                      print verbose output from the individual goss commands." << endl;
    cout << "  -v                      print the goss commands." << endl;
    cout << "  -w <dir>                directory for graphs and other working files." << endl;
    cout << "                          (default 'goss-files')" << endl;
    cout << "  --tmp-dir <dir>         directory for temporary files. (default '/tmp')" << endl;
    cout << "  -x <path>               path to the 'goss' executable " << endl;
    if( pPrintGossCmds )
    {
        cout << "" << endl;
        cout << "Examples:" << endl;
        cout << "" << endl;
        cout << "Perform the simplest possible assembly on unpaired reads" << endl;
        cout << "and write the contigs to 'output.fa':" << endl;
        cout << "    $0 -o output.fa reads1.fastq.gz reads2.fastq.gz" << endl;
        cout << "" << endl;
        cout << "Perform a simple assembly on unpaired reads assuming a" << endl;
        cout << "machine with 8 cores and 24GB of useable RAM:" << endl;
        cout << "    $0 -T 8 -B 24 -o output.fa reads1.fastq.gz reads2.fastq.gz" << endl;
        cout << "" << endl;
        cout << "Assemble reads from a paired end library with an insert size" << endl;
        cout << "of 500bp, and a mate pair library with an insert size of 2kb" << endl;
        cout << "using k=55, 8 cores and 24GB of REAM:" << endl;
        cout << "    $0 -T 8 -B 24 -k 55 -o output.fa \\" << endl;
        cout << "            -p 500 paired_1.fastq paired_2.fastq \\" << endl;
        cout << "            -m 2000 mate_1.fastq mate_2.fastq \\" << endl;
        cout << "" << endl;
        cout << "Have $0 print out the indivitual 'goss' commands for a simple" << endl;
        cout << "assembly, without actually executing them:" << endl;
        cout << "    $0 -D -v -o output.fa reads1.fastq.gz reads2.fastq.gz" << endl;
    }
}

int runCmd(const string& pCmd, bool pExitOnError = true)
{
    int error = std::system(pCmd.c_str());
    if( error && pExitOnError )
    {
        cerr << "error executing command: " << pCmd << endl;
        exit(1);
    }
    return error;
}

string print(const vector<string>& v)
{
    stringstream ss;
    for( unsigned i = 0; i < v.size(); ++i )
    {
        ss << " " << v[i];
    }
    return ss.str();
}

class InputFileGroup
{
public:
    string files() const
    {
        return print(mFiles);
    }

public:
    int mInsertSize;
    string mKind;
    vector<string> mFiles;

};


class Options
{
public:
    Options(int pArgc, char* pArgv[]);

    void parse();
    void parse2();
    bool isOption(const string& pOption );
    void parseFile();
    string files() const;
    string graphName(int pGraphCnt);

public:
    // program options
    int mBufferSizeGb;
    int mExpectedCoverage;
    bool mDryRun;
    bool mHelp;
    int mKmerSize;
    vector<InputFileGroup> mInputFileGroups;
    string mOut;
    int mThreads;
    int mTrim;
    string mTempDir;
    bool mPrintGossCmds;
    string mVerbose;
    string mWorkingDir;
    string mGossExec;
    string mKillSignal;

protected:
    static boost::xpressive::sregex sFastaRegex;
    static boost::xpressive::sregex sFastqRegex;

    vector<char*> mArgs;
    int mIdx;
};

class CmdBuilder
{
public:
    CmdBuilder(Options& pOps) : mOps(pOps) {}

    void build( stringstream& ss,
        const string& pModule,
        const string& pGraphIn,
        const string& pGraphOut );
    int runStage(const string& pCmd, bool pExitOnError = true);
    int createDir(const string& dir);


public:
    const Options& mOps;
};



Options::Options(int pArgc, char* pArgv[])
    : mBufferSizeGb(2)
    , mExpectedCoverage(-1)
    , mDryRun(false)
    , mHelp(false)
    , mKmerSize(27)
    , mInputFileGroups()
    , mOut( " -o contigs.fa" )
    , mThreads(4)
    , mTrim(-1)
    , mTempDir()
    , mPrintGossCmds(true)
    , mVerbose()
    , mWorkingDir("goss-files")
    , mGossExec("goss") // default assumes goss executable is in current dir
    , mKillSignal()
    , mArgs(pArgv, pArgv + pArgc)
    , mIdx(1) // skip the program name
{
    // first group are not paired reads
    mInputFileGroups.push_back( InputFileGroup() );
    mInputFileGroups.back().mInsertSize = 0; // not paired 
    mInputFileGroups.back().mKind.clear(); // not paired 
}

bool Options::isOption(const string& pOption )
{
    return 0 == pOption.compare( mArgs[mIdx] );
}

string Options::files() const
{
    vector<string> v;
    for( unsigned i = 0; i < mInputFileGroups.size(); ++i )
    {
        v.insert( v.end(),
            mInputFileGroups[i].mFiles.begin(),
            mInputFileGroups[i].mFiles.end() );
    }
    return print(v);
}

boost::xpressive::sregex Options::sFastaRegex = boost::xpressive::sregex::compile(
     ".*\\.fa$"
    "|.*\\.fasta$"
    "|.*\\.fa\\.gz$"
    "|.*\\.fa\\.bz2$"
    "|.*\\.fasta\\.gz$"
    "|.*\\.fasta\\.bz2$"
    "|.*s_._._sequence.txt$"
    "|.*s_._._sequence.txt.gz$"
    "|.*s_._._sequence.txt.bz2$" );

boost::xpressive::sregex Options::sFastqRegex = boost::xpressive::sregex::compile(
     ".*\\.fq$"
    "|.*\\.fastq$"
    "|.*\\.fq\\.gz$"
    "|.*\\.fq\\.bz2$"
    "|.*\\.fastq\\.gz$"
    "|.*\\.fastq\\.bz2$"
    "|.*s_._sequence\\.txt$"
    "|.*s_._sequence\\.txt\\.gz$"
    "|.*s_._sequence\\.txt\\.bz2$" );


void Options::parseFile()
{
    string option;
    string s = mArgs[mIdx];
    if( regex_match(s, sFastaRegex) )
    {
        option = "-I";
    }
    else if( regex_match(s, sFastqRegex) )
    {
        option = "-i";
    }
    else
    {
        return;
    }
    mInputFileGroups.back().mFiles.push_back(" " + option + " " + s);
    mIdx += 1;
}

void Options::parse()
{
    while( mIdx < int(mArgs.size()) )
    {
        int oldIdx = mIdx;
        
        parse2();
        
        if( oldIdx == mIdx )
        {
            cerr << "Unknow option: " << mArgs[mIdx] << endl;
            exit(1);
        }
    }
}

void Options::parse2()
{
    if( isOption("-B") )
    {
        mBufferSizeGb = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("-c") )
    {
        mExpectedCoverage = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("-D") )
    {
        mDryRun = true;
        mIdx += 1;
    }
    else if( isOption("-h") )
    {
        mHelp = true;
        mIdx += 1;
    }
    else if( isOption("-k") )
    {
        mKmerSize = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("-m") )
    {
        mInputFileGroups.push_back( InputFileGroup() );
        mInputFileGroups.back().mKind = " --mate-pairs";
        mInputFileGroups.back().mInsertSize = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("-p") )
    {
        mInputFileGroups.push_back( InputFileGroup() );
        mInputFileGroups.back().mKind = " --paired-ends";
        mInputFileGroups.back().mInsertSize = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("-o") )
    {
        mOut = string(" -o ") + mArgs[mIdx+1];
        mIdx += 2;
    }
    else if( isOption("-T") )
    {
        mThreads = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("-t") )
    {
        mTrim = atoi(mArgs[mIdx+1]);
        mIdx += 2;
    }
    else if( isOption("--tmp-dir") )
    {
        mTempDir = string("--tmp-dir ") + mArgs[mIdx+1];
        mIdx += 2;
    }
    else if( isOption("-v") )
    {
        mPrintGossCmds = true;
        mIdx += 1;
    }
    else if( isOption("-V") )
    {
        mVerbose = "-v";
        mIdx += 1;
    }
    else if( isOption("-w") )
    {
        mWorkingDir = mArgs[mIdx+1];
        mIdx += 2;
    }
    else if( isOption("-x") )
    {
        mGossExec = mArgs[mIdx+1];
        mGossExec = "\"" + mGossExec + "\"";
        mIdx += 2;
    }
#ifdef KILL_SIGNAL_SUPPORT
    else if( isOption(KILL_SIGNAL_CMD_OPTION) )
    {
        GossKillSignal::Register( mArgs[mIdx+1] );
        // forward the command to goss as well.
        mKillSignal = KILL_SIGNAL_CMD_OPTION + " " + mArgs[mIdx+1];
        mIdx += 2;
    }
#endif
    else
    {
        parseFile();
    }
}

string Options::graphName(int pGraphCnt)
{
    stringstream ss;
    ss << mWorkingDir << "/graph-" << pGraphCnt;
    return ss.str();
}

int CmdBuilder::runStage(const string& pCmd, bool pExitOnError)
{
    ++sCurrStage;
    if( mOps.mPrintGossCmds )
    {
        cout << pCmd << endl;
    }
    if( mOps.mDryRun )
    {
        return 0;
    }
   
    ofstream out( (mOps.mWorkingDir + "/progress.txt").c_str() );
    out << sTotalStages << endl;
    out << sCurrStage << endl;
    out.close();
    
    return runCmd(pCmd, pExitOnError);
}

int CmdBuilder::createDir(const string& dir)
{
    if( mOps.mDryRun )
    {
        return 1;
    }
    if( boost::filesystem::exists(dir) )
    {
        return 1;
    }
    return runCmd("mkdir \"" + dir + "\"");
}

void CmdBuilder::build(
    stringstream& ss,
    const string& pModule,
    const string& pGraphIn,
    const string& pGraphOut )
{
    ss.str("");
    ss  << mOps.mGossExec
        << " " << pModule
        << " " << mOps.mKillSignal
        << " " << mOps.mVerbose
        << " -T " << mOps.mThreads
        << " " << mOps.mTempDir;
    if( !pGraphIn.empty() )
    {
        ss << " -G " << pGraphIn;
    }
    if( !pGraphOut.empty() )
    {
        ss << " -O " << pGraphOut;
    }
}


int run(Options& ops)
{
    if( ops.mHelp )
    {
        usage(ops.mPrintGossCmds);
        return 0;
    }

    // assuming that goosple is in the same dir as gossple
    CmdBuilder cb(ops);

    if( ops.mPrintGossCmds )
    {
        cb.runStage( ops.mGossExec + " help --version" );
    }

    if( ops.mKmerSize >= 63 )
    {
        cerr << "maximum k is 62" <<endl;
        exit(1);
    }

    cb.createDir(ops.mWorkingDir);
    string scaf = ops.mWorkingDir + "/scaf";

    stringstream ss;

    int graphCnt = 0;
    

    // build-graph
    // --------------------------------------------------------
    ++graphCnt;
    string g = ops.graphName(graphCnt);

    cb.build(ss, "build-graph", "", g);
    ss  << " -k " << ops.mKmerSize
        << " -B " << ops.mBufferSizeGb
        << ops.files();
    cb.runStage( ss.str() );
    
    // trim-graph
    // --------------------------------------------------------
    ++graphCnt;
    string h = ops.graphName(graphCnt);

    cb.build(ss, "trim-graph", g, h);

    // default is auto trim
    if( ops.mTrim >= 0 )
    {
        ss << " -C " <<  ops.mTrim;
    }
    cb.runStage( ss.str() );


    // prune-tips
    // --------------------------------------------------------
    const int PRUNE_N_TIMES = 5;
    for( int i = 0; i < PRUNE_N_TIMES; ++i )
    {
        g = h;
        ++graphCnt;
        h = ops.graphName(graphCnt);
    
        cb.build(ss, "prune-tips", g, h);
        cb.runStage(ss.str());
    }

    // pop-bubbles
    // --------------------------------------------------------
    g = h;
    ++graphCnt;
    h = ops.graphName(graphCnt);
    cb.build(ss, "pop-bubbles", g, h);
    cb.runStage(ss.str());

    // build-entry-edge-set
    // --------------------------------------------------------
    g = h;
    cb.build(ss, "build-entry-edge-set", g, "");
    cb.runStage(ss.str());
    

    // build-supergraph
    // --------------------------------------------------------
    cb.build(ss, "build-supergraph", g, "");
    cb.runStage(ss.str());


    // --------------------------------------------------------
    ss.str("");
    if( ops.mExpectedCoverage >= 0 )
    {
        ss << " --expected-coverage " << ops.mExpectedCoverage;
    }
    string expectedCoverage = ss.str();    


    // thread-pairs
    // --------------------------------------------------------
    for( unsigned i = 1; i < ops.mInputFileGroups.size(); ++i )
    {
        InputFileGroup& group = ops.mInputFileGroups[i];

        cb.build(ss, "thread-pairs", g, "");
        ss << " --insert-expected-size " << group.mInsertSize;
        ss << expectedCoverage;
        ss << group.mKind;
        ss << group.files();

        cb.runStage(ss.str());
    }

    // thread-reads
    // --------------------------------------------------------
    cb.build(ss, "thread-reads", g, "");
    ss << expectedCoverage;    
    ss << ops.files();
    cb.runStage(ss.str());

    // build-scaffold
    // --------------------------------------------------------
    for( unsigned i = 1; i < ops.mInputFileGroups.size(); ++i )
    {
        InputFileGroup& group = ops.mInputFileGroups[i];

        cb.build(ss, "build-scaffold", g, "");
        ss << " --insert-expected-size " << group.mInsertSize;
        ss << expectedCoverage;
        ss << group.mKind;
        ss << " --scaffold-out " << scaf;
        ss << group.files();
        cb.runStage(ss.str());
    }


    // scaffold
    // --------------------------------------------------------
    if( ops.mInputFileGroups.size() > 1 )
    {
        cb.build(ss, "scaffold", g, "");
        ss << " --scaffold-in " << scaf;
        cb.runStage(ss.str());
    }

    // print-contigs
    // --------------------------------------------------------
    cb.build(ss, "print-contigs", g, "");
    ss << " --min-length 100 ";
    ss << ops.mOut;
    cb.runStage(ss.str());


    // --------------------------------------------------------
    //system("pause");

    return 0;
}

int main(int argc, char* argv[])
{
    Options ops(argc, argv);
    ops.parse();

    if( !ops.mDryRun )
    {
        // do a dry run to see how many stages there will be
        ops.mDryRun = true;
        int err = run(ops);
        if( err )
        {
            return err;
        }
        sTotalStages = sCurrStage;
        sCurrStage = 0;
        ops.mDryRun = false;
    }
    
    return run(ops);
}





