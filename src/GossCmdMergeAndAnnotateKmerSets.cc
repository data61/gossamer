// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "GossCmdMergeAndAnnotateKmerSets.hh"

#include "LineSource.hh"
#include "BackgroundMultiConsumer.hh"
#include "FastaParser.hh"
#include "GossCmdReg.hh"
#include "GossOptionChecker.hh"
#include "GossReadSequenceBases.hh"
#include "KmerSet.hh"
#include "Phylogeny.hh"
#include "RunLengthCodedSet.hh"
#include "Spinlock.hh"
#include "Timer.hh"

#include <string>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace boost::program_options;
using namespace std;

void
GossCmdMergeAndAnnotateKmerSets::operator()(const GossCmdContext& pCxt)
{
    FileFactory& fac(pCxt.fac);
    Logger& log(pCxt.log);

    Timer t;

    KmerSet lhs(mLhs, fac);
    KmerSet rhs(mRhs, fac);

    if (lhs.count() == 0 || rhs.count() == 0)
    {
        throw "nonsense";
    }

    if (lhs.K() != rhs.K())
    {
        throw "nonsense";
    }

    log(info, "counting kmers.");

    uint64_t n = 0;
    uint64_t c = 0;

    uint64_t l = 0;
    KmerSet::Edge le = lhs.select(l);
    uint64_t r = 0;
    KmerSet::Edge re = rhs.select(r);
    while (l < lhs.count() && r < rhs.count())
    {
        if (le < re)
        {
            ++n;

            ++l;
            if (l < lhs.count())
            {
                le = lhs.select(l);
            }
            continue;
        }
        if (le > re)
        {
            ++n;

            ++r;
            if (r < rhs.count())
            {
                re = rhs.select(r);
            }
            continue;
        }
        ++n;
        ++c;

        ++l;
        if (l < lhs.count())
        {
            le = lhs.select(l);
        }

        ++r;
        if (r < rhs.count())
        {
            re = rhs.select(r);
        }
    }
    while (l < lhs.count())
    {
        ++n;

        ++l;
        if (l < lhs.count())
        {
            le = lhs.select(l);
        }
    }
    while (r < rhs.count())
    {
        ++n;

        ++r;
        if (r < rhs.count())
        {
            re = rhs.select(r);
        }
    }

    log(info, "writing out " + lexical_cast<string>(n) + " kmers.");
    log(info, "of which " + lexical_cast<string>(c) + " are common.");

    KmerSet::Builder bld(lhs.K(), mOut, fac, n);
    WordyBitVector::Builder lhsBld(mOut + ".lhs-bits", fac);
    WordyBitVector::Builder rhsBld(mOut + ".rhs-bits", fac);

    l = 0;
    le = lhs.select(l);
    r = 0;
    re = rhs.select(r);
    while (l < lhs.count() && r < rhs.count())
    {
        if (le < re)
        {
            bld.push_back(le.value());
            lhsBld.push_backX(true);
            rhsBld.push_backX(false);

            ++l;
            if (l < lhs.count())
            {
                le = lhs.select(l);
            }
            continue;
        }
        if (le > re)
        {
            bld.push_back(re.value());
            lhsBld.push_backX(false);
            rhsBld.push_backX(true);

            ++r;
            if (r < rhs.count())
            {
                re = rhs.select(r);
            }
            continue;
        }

        bld.push_back(le.value());
        lhsBld.push_backX(true);
        rhsBld.push_backX(true);

        ++l;
        if (l < lhs.count())
        {
            le = lhs.select(l);
        }

        ++r;
        if (r < rhs.count())
        {
            re = rhs.select(r);
        }
    }
    while (l < lhs.count())
    {
        bld.push_back(le.value());
        lhsBld.push_backX(true);
        rhsBld.push_backX(false);

        ++l;
        if (l < lhs.count())
        {
            le = lhs.select(l);
        }
    }
    while (r < rhs.count())
    {
        bld.push_back(re.value());
        lhsBld.push_backX(false);
        rhsBld.push_backX(true);

        ++r;
        if (r < rhs.count())
        {
            re = rhs.select(r);
        }
    }
    bld.end();
    lhsBld.end();
    rhsBld.end();

    cout << l << '\t' << r << '\t' << c << '\n';
    log(info, "total elapsed time: " + lexical_cast<string>(t.check()));
}


GossCmdPtr
GossCmdFactoryMergeAndAnnotateKmerSets::create(App& pApp, const variables_map& pOpts)
{
    GossOptionChecker chk(pOpts);

    string lhs;
    string rhs;
    chk.getRepeatingTwice("graph-in", lhs, rhs);

    string out;
    chk.getMandatory("graph-out", out);

    chk.throwIfNecessary(pApp);

    return GossCmdPtr(new GossCmdMergeAndAnnotateKmerSets(lhs, rhs, out));
}

GossCmdFactoryMergeAndAnnotateKmerSets::GossCmdFactoryMergeAndAnnotateKmerSets()
    : GossCmdFactory("Decorate a graph with an assignment of kmers to graphs.")
{
    mCommonOptions.insert("graph-in");
    mCommonOptions.insert("graph-out");
}
