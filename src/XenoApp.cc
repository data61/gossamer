// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "XenoApp.hh"

#include "BackyardHash.hh"
#include "Debug.hh"
#include "Utils.hh"
#include "GossamerException.hh"
#include "GossCmdReg.hh"
#include "GossOption.hh"
#include "GossOptionChecker.hh"
#include "Logger.hh"
#include "PhysicalFileFactory.hh"
#include "Timer.hh"
#include "XenoVersion.hh"

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>

#include "GossCmdBuildKmerSet.hh"
#include "GossCmdComputeNearKmers.hh"
#include "GossCmdGroupReads.hh"
#include "GossCmdHelp.hh"
#include "GossCmdMergeAndAnnotateKmerSets.hh"

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;

namespace // anonymous
{
    GossOptions globalOpts;

    GossOptions commonOpts;

    vector<GossCmdReg> cmds;

    class XenoCmdIndex : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            Logger& log(pCxt.log);

            // Assume 0.2 GB operating overhead.
            const uint64_t M = (mM - 0.2) * 1024 * 1024 * 1024;
            const uint64_t S = BackyardHash::maxSlotBits(M);
            const uint64_t N = M / (1.5 * sizeof(uint32_t) + sizeof(BackyardHash::value_type));

            string g = mOut + "-graft";
            string h = mOut + "-host";
            string b = mOut + "-both";

            log(info, "building graft reference kmer set");
            GossCmdBuildKmerSet(mK, S, N, mT, g, strings(1, mGraft), strings(), strings())(pCxt);

            log(info, "building host reference kmer set");
            GossCmdBuildKmerSet(mK, S, N, mT, h, strings(1, mHost), strings(), strings())(pCxt);

            log(info, "merging host and graft reference kmer sets");
            GossCmdMergeAndAnnotateKmerSets(g, h, b)(pCxt);

            log(info, "computing marginal kmers");
            GossCmdComputeNearKmers(b, mT)(pCxt);

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        XenoCmdIndex(const std::string& pGraft, const std::string& pHost,
                     const std::string& pOut, uint64_t pK, double pM, uint64_t pT)
            : mGraft(pGraft), mHost(pHost), mOut(pOut), mK(pK), mM(pM), mT(pT)
        {
        }

    private:
        const std::string mGraft;
        const std::string mHost;
        const std::string mOut;
        const uint64_t mK;
        const double mM;
        const uint64_t mT;
    };

    class XenoCmdFactoryIndex : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);

            uint64_t K = 25;
            chk.getOptional("kmer-size", K);

            string graft;
            chk.getMandatory("graft", graft);

            string host;
            chk.getMandatory("host", host);

            string out;
            chk.getMandatory("prefix", out);

            double M = 2;
            chk.getOptional("max-memory", M);

            uint64_t T = 4;
            chk.getOptional("num-threads", T);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new XenoCmdIndex(graft, host, out, K, M, T));
        }

        XenoCmdFactoryIndex()
            : GossCmdFactory("build an index for classifying reads")
        {
            mCommonOptions.insert("prefix");
            mCommonOptions.insert("max-memory");
            mSpecificOptions.addOpt<uint64_t>("kmer-size", "K", "kmer size to use (default 25)");
            mSpecificOptions.addOpt<string>("graft", "G", "graft reference in FASTA format");
            mSpecificOptions.addOpt<string>("host", "H", "host reference in FASTA format");
        }
    };

    class XenoCmdGroup : public GossCmd
    {
    public:
        typedef std::vector<std::string> strings;

        void operator()(const GossCmdContext& pCxt)
        {
            Timer t;
            Logger& log(pCxt.log);

            string b = mIn + "-both";
            GossCmdGroupReads(b, mFastas, mFastqs, mLines, mPairs, mMaxMemory, mNumThreads,
                              mGraftName, mHostName, mPrefix, mDontWriteReads, mPreserveReadOrder)(pCxt);

            log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
        }

        XenoCmdGroup(const std::string& pIn,
                     const strings& pFastas, const strings& pFastqs, const strings& pLines,
                     bool pPairs, double pMaxMemory, uint64_t pNumThreads,
                     const std::string& pGraftName, const std::string& pHostName, const std::string& pPrefix,
                     bool pDontWriteReads, bool pPreserveReadOrder)
            : mIn(pIn), mFastas(pFastas), mFastqs(pFastqs), mLines(pLines), 
              mPairs(pPairs), mMaxMemory(pMaxMemory), mNumThreads(pNumThreads),
              mGraftName(pGraftName), mHostName(pHostName), mPrefix(pPrefix),
              mDontWriteReads(pDontWriteReads), mPreserveReadOrder(pPreserveReadOrder)
        {
        }

    private:
        const std::string mIn;
        const strings mFastas;
        const strings mFastqs;
        const strings mLines;
        const bool mPairs;
        const uint64_t mMaxMemory;
        const uint64_t mNumThreads;
        const std::string mGraftName;
        const std::string mHostName;
        const std::string mPrefix;
        const bool mDontWriteReads;
        const bool mPreserveReadOrder;
    };

    class XenoCmdFactoryGroup : public GossCmdFactory
    {
    public:
        GossCmdPtr create(App& pApp, const boost::program_options::variables_map& pOpts)
        {
            GossOptionChecker chk(pOpts);
            FileFactory& fac(pApp.fileFactory());
            GossOptionChecker::FileReadCheck readChk(fac);

            string in;
            chk.getMandatory("prefix", in);

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

            string graftName = "graft";
            chk.getOptional("graft-name", graftName);

            string hostName = "host";
            chk.getOptional("host-name", hostName);

            string prefix;
            chk.getOptional("output-filename-prefix", prefix);

            bool noOut = false;
            chk.getOptional("dont-write-reads", noOut);

            bool ord = false;
            chk.getOptional("preserve-read-order", ord);

            chk.throwIfNecessary(pApp);

            return GossCmdPtr(new XenoCmdGroup(in, fastas, fastqs, lines, pairs, M, T, 
                                               graftName, hostName, prefix, noOut, ord));
        }

        XenoCmdFactoryGroup()
            : GossCmdFactory("classify reads according to index")
        {
            mCommonOptions.insert("fasta-in");
            mCommonOptions.insert("fastq-in");
            mCommonOptions.insert("line-in");
            mCommonOptions.insert("prefix");
            mCommonOptions.insert("max-memory");

            mSpecificOptions.addOpt<bool>("pairs", "",
                    "treat reads as pairs");
            mSpecificOptions.addOpt<string>("graft-name", "",
                    "name of graft reads");
            mSpecificOptions.addOpt<string>("host-name", "",
                    "name of host reads");
            mSpecificOptions.addOpt<string>("output-filename-prefix", "",
                    "prefix for output files");
            mSpecificOptions.addOpt<bool>("dont-write-reads", "",
                    "do not produce any output read files");
            mSpecificOptions.addOpt<bool>("preserve-read-order", "",
                    "maintain the same relative ordering of reads in output files");
        }
    };

} // namespace anonymous

const char*
XenoApp::name() const
{
    return "xenome";
}

const char*
XenoApp::version() const
{
    return Gossamer::version;
}

XenoApp::XenoApp()
    : App(globalOpts, commonOpts)
{

    cmds.push_back(GossCmdReg("index", GossCmdFactoryPtr(new XenoCmdFactoryIndex)));
    cmds.push_back(GossCmdReg("classify", GossCmdFactoryPtr(new XenoCmdFactoryGroup)));
    cmds.push_back(GossCmdReg("help", GossCmdFactoryPtr(new GossCmdFactoryHelp(*this))));

    globalOpts.addOpt<strings>("debug", "D",
                                "enable particular debugging output");
    globalOpts.addOpt<bool>("help", "h", "show a help message");
    globalOpts.addOpt<string>("log-file", "l", "place to write messages");
    globalOpts.addOpt<strings>("tmp-dir", "", "a directory to use for temporary files (default /tmp)");
    globalOpts.addOpt<uint64_t>("num-threads", "T", "maximum number of worker threads to use, where possible");
    globalOpts.addOpt<bool>("verbose", "v", "show progress messages");
    globalOpts.addOpt<bool>("version", "V", "show the software version");

    commonOpts.addOpt<strings>("fasta-in", "I", "input file in FASTA format");
    commonOpts.addOpt<strings>("fastq-in", "i", "input file in FASTQ format");
    commonOpts.addOpt<strings>("line-in", "", "input file with one sequence per line");
    commonOpts.addOpt<string>("prefix", "P", "filename prefix for index");
    commonOpts.addOpt<double>("max-memory", "M", "maximum memory (in GB) to use");
}
