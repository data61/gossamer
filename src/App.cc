// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "App.hh"

#include "Debug.hh"
#include "Utils.hh"
#include "GossamerException.hh"
#include "GossCmdReg.hh"
#include "GossOption.hh"
#include "Logger.hh"
#include "PhysicalFileFactory.hh"

#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>

using namespace boost;
using namespace boost::program_options;
using namespace std;
using namespace Gossamer;

typedef vector<string> strings;

namespace { // anonymous
    MachineAutoSetup machineAutoSetup(true);

    Debug verboseExceptions("verbose-exceptions",
                            "print detailed information about errors");

    options_description opts;

    positional_options_description posOpts;

    string cmdName;

    GossCmdPtr cmd;

    FileFactoryPtr theFileFactory;

    LoggerPtr theLogger;

    void setupPlatform()
    {
        bool ok = false;

        try {
            machineAutoSetup();
            ok = true;
        }
        catch (Gossamer::error& e)
        {
            string const* msg = get_error_info<general_error_info>(e);
            if (msg)
            {
                cerr << *msg << endl;
                std::exit(1);
            }
        }
        catch (...)
        {
        }

        if (!ok)
        {
            std::cerr
                << "A highly unusual error condition occurred while\n"
                   "attempting to run this program.\n";
            std::exit(1);
        }
    }

    bool
    validOptions(const GossOptions& pCommonOpts)
    {
        map<string,GossCmdFactoryPtr>& cmds(*GossCmdReg::cmds);
        bool bad_opt = false;
        map<string, vector<string> > opt_users;
        for (map<string, GossCmdFactoryPtr>::const_iterator
             i = cmds.begin(); i != cmds.end(); ++i)
        {
            const string& cmd_name(i->first);
            GossCmdFactoryPtr f(i->second);
            // Accumulate specific options.
            const GossOptions& specs(f->specificOptions());
            for (GossOptions::OptsMap::const_iterator
                 j = specs.opts.begin(); j != specs.opts.end(); ++j)
            {
                opt_users[j->first].push_back(cmd_name);
            }

            // Check that all common options exist.
            const set<string>& comms(f->commonOptions());
            for (set<string>::const_iterator
                 j = comms.begin(); j != comms.end(); ++j)
            {
                if (!pCommonOpts.opts.count(*j))
                {
                    bad_opt = true;
                    cerr << "command '" << cmd_name 
                         << "' uses unknown common option '" << *j << "'" << endl;
                }
            }
        }

        for (map<string, vector<string> >::const_iterator
             i = opt_users.begin(); i != opt_users.end(); ++i)
        {
            if (i->second.size() > 1)
            {
                bad_opt = true;
                cerr << "specific option '" << i->first << "' shared by commands:";
                for (vector<string>::const_iterator
                     j = i->second.begin(); j != i->second.end(); ++j)
                {
                    cerr << " '" << *j << "'";
                }
                cerr << endl;
            }
        }

        return !bad_opt;
    }

} // namespace anonymous


FileFactory&
App::fileFactory()
{
    BOOST_ASSERT(theFileFactory.get());
    return *theFileFactory;
}

Logger&
App::logger()
{
    BOOST_ASSERT(theLogger.get());
    return *theLogger;
}

void
App::help(bool pExit)
{
    BOOST_ASSERT(GossCmdReg::cmds);
    map<string,GossCmdFactoryPtr>& cmds(*GossCmdReg::cmds);

    map<string,GossCmdFactoryPtr>::iterator i = cmds.find("help");
    BOOST_ASSERT(i != cmds.end());

    variables_map optsMap;
    GossCmdPtr cmd = i->second->create(*this, optsMap);
    GossCmdContext cxt(fileFactory(), logger(), cmdName, optsMap);
    (*cmd)(cxt);

    cerr << endl;
    if (cmd.get())
    {
        cerr << cmdName << endl;
    }
    cerr << opts << endl;

    if (pExit)
    {
        exit(1);
    }
}

int
App::main(int argc, char* argv[])
{
    setupPlatform();
    try
    {
        BOOST_ASSERT(GossCmdReg::cmds);
        BOOST_ASSERT(validOptions(mCommonOpts));
        (void) (&validOptions); // suppress a warning in the release build.

        for (int i = 1; i < argc; ++i)
        {
            if (!strcmp(argv[i], "--version"))
            {
                cout << name() << " version " << version() << endl;
                return 0;
            }
        }

        uint64_t argsToSkip = 0;
        if (argc < 2)
        {
            cmdName = "help";
        }
        else if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
        {
            cmdName = "help";
        }
        else
        {
            cmdName = argv[1];
            if (argv[1][0] != '-')
            {
                argsToSkip = 1;
            }
        }

        map<string,GossCmdFactoryPtr>& cmds(*GossCmdReg::cmds);

        map<string,GossCmdFactoryPtr>::iterator i = cmds.find(cmdName);
        bool bad_cmd = false;
        if (i == cmds.end())
        {
            cerr << "unknown command '" << cmdName << "'" << endl;
            i = cmds.find("help");
            BOOST_ASSERT(i != cmds.end());
            bad_cmd = true;
        }

        variables_map optsMap;
        bool bad_opt = false;
        if (!bad_cmd)
        {
            stringstream bad_opt_msg;
            for (GossOptions::OptsMap::const_iterator j = mGlobalOpts.opts.begin(); j != mGlobalOpts.opts.end(); ++j)
            {
                j->second->add(opts, posOpts);
            }

            const set<string>& common(i->second->commonOptions());
            for (set<string>::const_iterator j = common.begin(); j != common.end(); ++j)
            {
                GossOptions::OptsMap::const_iterator k = mCommonOpts.opts.find(*j);
                if (k == mCommonOpts.opts.end())
                {
                    BOOST_THROW_EXCEPTION(
                        Gossamer::error()
                            << Gossamer::general_error_info("unrecognised common option " + *j));
                }
                k->second->add(opts, posOpts);
            }

            const GossOptions& specific(i->second->specificOptions());
            for (GossOptions::OptsMap::const_iterator j = specific.opts.begin(); j != specific.opts.end(); ++j)
            {
                j->second->add(opts, posOpts);
            }

            parsed_options parsed_opts = command_line_parser(argc - argsToSkip, argv + argsToSkip).
                                                            options(opts).allow_unregistered().run();
            for (vector<basic_option<char> >::const_iterator
                 j  = parsed_opts.options.begin();
                 j != parsed_opts.options.end();
                 ++j)
            {
                if (j->unregistered || j->string_key.empty())
                {
                    bad_opt_msg << "unknown option '" << j->original_tokens.front() << "'" << endl;
                    bad_opt = true;
                }
            }
            store(parsed_opts, optsMap);
            if (bad_opt)
            {
                if (optsMap.count("help"))
                {
                    cerr << bad_opt_msg.str();
                    help(true);
                }
                BOOST_THROW_EXCEPTION(Gossamer::error() << Gossamer::usage_info(bad_opt_msg.str()));
            }
        }

        // set up the file factory
        strings tmp;
        if (optsMap.count("tmp-dir") == 0)
        {
            //tmp.push_back("/tmp"); //???
            tmp.push_back( Gossamer::defaultTmpDir() );
        }
        else
        {
            tmp = optsMap["tmp-dir"].as<strings>();
        }
        theFileFactory = FileFactoryPtr(new PhysicalFileFactory(tmp[0]));

        // set up logging
        Severity sev(optsMap.count("verbose") ? info : warning);
        if (optsMap.count("log-file") == 0)
        {
            theLogger = LoggerPtr(new Logger(cerr, sev));
        }
        else
        {
            string logName = optsMap["log-file"].as<string>();
            theLogger = std::make_shared<Logger>(logName, *theFileFactory, sev);
        }

        if (optsMap.count("debug"))
        {
            strings ss = optsMap["debug"].as<strings>();
            for (uint64_t i = 0; i < ss.size(); ++i)
            {
                if (!Debug::enable(ss[i]))
                {
                    logger()(warning, "unknown debug: " + ss[i]);
                }
            }
        }

        cmd = i->second->create(*this, optsMap);

        GossCmdContext cxt(fileFactory(), logger(), cmdName, optsMap);
        try
        {
            (*cmd)(cxt);
        }
        catch (Gossamer::error& e)
        {
            e << Gossamer::cmd_name_info(cmdName);
            throw;
        }
    }
    catch (Gossamer::error& e)
    {
        string const* cmdNm = get_error_info<cmd_name_info>(e);
        if (cmdNm)
        {
            cerr << "error performing " << *cmdNm << ":" << endl;
        }

        string const* grNm = get_error_info<open_graph_name_info>(e);
        if (grNm)
        {
            cerr << "\tunable to open graph '" << *grNm << "'" << endl;
        }

        pair<uint64_t,uint64_t> const* ver = get_error_info<version_mismatch_info>(e);
        if (ver)
        {
            cerr << "\ta version mismatch was detected. " << name() << " expected "
                 << lexical_cast<string>(ver->second) << " but found "
                 << lexical_cast<string>(ver->first) << "." << endl;
        }

        string const* parse = get_error_info<parse_error_info>(e);
        if (parse)
        {
            string const* fn = get_error_info<errinfo_file_name>(e);
            BOOST_ASSERT(fn);
            cerr << "\t'" << *fn << "': " << *parse << endl;
        }

        string const* fileNm = get_error_info<write_error_info>(e);
        if (fileNm)
        {
            cerr << "\tcannot write to '" << *fileNm << "'" << endl;
        }

        string const* msg = get_error_info<general_error_info>(e);
        if (msg)
        {
            cerr << *msg;
        }

        int const* eno = get_error_info<errinfo_errno>(e);
        if (eno)
        {
            string const* fn = get_error_info<errinfo_file_name>(e);
            cerr << "\t";
            if (fn)
            {
                cerr << "'" << *fn << "': ";        
            }
            cerr << strerror(*eno) << endl;
        }

        string const* usg = get_error_info<usage_info>(e);
        if (usg)
        {
            cerr << *usg;
            cerr << "use\n"
                 << "\t" << name() << " " << cmdName << " -h\n"
                 << "for more usage information." << endl;
        }

        pair<uint64_t,uint64_t> const* fsm = get_error_info<array_file_size_info>(e);
        if (ver)
        {
            string const* fn = get_error_info<errinfo_file_name>(e);
            cerr << "\tfile '" << fn << "' of size " << fsm->first 
                 << ", does not contain an integral number of objects of size "
                 << fsm->second << endl;
        }

        if (verboseExceptions.on())
        {
            cerr << "------ start of diagnostic information ------" << endl;
            cerr << diagnostic_information(e);
            cerr << "------ end of diagnostic information ------" << endl;
        }
        return 1;
    }
    catch (std::exception& e)
    {
        cerr << "caught unexpected exception: " << e.what() << endl;
        return 1;
    }
    catch (...)
    {
        cerr << "caught unknown exception" << endl;
        return 1;
    }
    return 0;
}
