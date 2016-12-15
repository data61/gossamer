// Copyright (c) 2008-2016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
/**  \file
 * Testing Annotated Trees
 *
 * The results for testing rely on results from the gmp R package or Haskell.
 */

#include "AnnotTree.hh"

#include "StringFileFactory.hh"

using namespace boost;
using namespace std;

#define GOSS_TEST_MODULE TestAnnotTree
#include "testBegin.hh"

BOOST_AUTO_TEST_CASE(testTrivial)
{
    StringFileFactory fac;
    fac.addFile("x.tree",
        "(\n"
        ")\n");

    FileFactory::InHolderPtr inp(fac.in("x.tree"));
    istream& in(**inp);
    AnnotTree::NodePtr n = AnnotTree::read(in);
    BOOST_CHECK_EQUAL(n->anns.size(), 0);
    BOOST_CHECK_EQUAL(n->kids.size(), 0);

    {
        FileFactory::OutHolderPtr outp(fac.out("y.tree"));
        ostream& out(**outp);
        AnnotTree::write(out, n);
    }
    BOOST_CHECK_EQUAL(fac.readFile("x.tree"), fac.readFile("y.tree"));
}

BOOST_AUTO_TEST_CASE(testAnnot)
{
    StringFileFactory fac;
    fac.addFile("x.tree",
        "(\n"
        " kind	phylum\n"
        " name	proteobacteria\n"
        ")\n");

    FileFactory::InHolderPtr inp(fac.in("x.tree"));
    istream& in(**inp);
    AnnotTree::NodePtr n = AnnotTree::read(in);
    BOOST_CHECK_EQUAL(n->anns.size(), 2);
    BOOST_CHECK_EQUAL(n->kids.size(), 0);

    {
        FileFactory::OutHolderPtr outp(fac.out("y.tree"));
        ostream& out(**outp);
        AnnotTree::write(out, n);
    }
    BOOST_CHECK_EQUAL(fac.readFile("x.tree"), fac.readFile("y.tree"));
}

BOOST_AUTO_TEST_CASE(testKids)
{
    StringFileFactory fac;
    fac.addFile("x.tree",
        "(\n"
        " (\n"
        "  (\n"
        "  )\n"
        " )\n"
        " (\n"
        " )\n"
        " (\n"
        " )\n"
        ")\n");

    FileFactory::InHolderPtr inp(fac.in("x.tree"));
    istream& in(**inp);
    AnnotTree::NodePtr n = AnnotTree::read(in);
    BOOST_CHECK_EQUAL(n->anns.size(), 0);
    BOOST_CHECK_EQUAL(n->kids.size(), 3);
    BOOST_CHECK_EQUAL(n->kids[0]->kids.size(), 1);
    BOOST_CHECK_EQUAL(n->kids[1]->kids.size(), 0);
    BOOST_CHECK_EQUAL(n->kids[2]->kids.size(), 0);

    {
        FileFactory::OutHolderPtr outp(fac.out("y.tree"));
        ostream& out(**outp);
        AnnotTree::write(out, n);
    }
    BOOST_CHECK_EQUAL(fac.readFile("x.tree"), fac.readFile("y.tree"));
}

#include "testEnd.hh"
