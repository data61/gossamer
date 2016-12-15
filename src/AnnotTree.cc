// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#include "AnnotTree.hh"

#include <string>
#include <stdint.h>
#include <boost/assert.hpp>

using namespace std;

namespace // anonymous
{
    class Tokens
    {
    public:
        bool valid() const
        {
            return mFile.good();
        }

        const string& operator*() const
        {
            BOOST_ASSERT(valid());
            return mToken;
        }

        void operator++()
        {
            mFile >> mToken;
        }

        Tokens(istream& pFile)
            : mFile(pFile)
        {
            mFile >> mToken;
        }

    private:
        istream& mFile;
        string mToken;
    };

    void ind(ostream& pOut, uint64_t pInd)
    {
        string s(pInd, ' ');
        pOut << s;
    }

    void require(bool pAssertion)
    {
        if (!pAssertion)
        {
            throw 42;
        }
    }

    AnnotTree::NodePtr readTree(Tokens& pToks)
    {
        require(pToks.valid());
        require(*pToks == "(");
        ++pToks;

        AnnotTree::NodePtr n(new AnnotTree::Node);

        require(pToks.valid());
        while (*pToks != "(" && *pToks != ")")
        {
            string key = *pToks;
            ++pToks;

            require(pToks.valid());
            string val = *pToks;
            ++pToks;

            n->anns[key] = val;

            require(pToks.valid());
        }


        require(pToks.valid());
        while (*pToks != ")")
        {
            n->kids.push_back(readTree(pToks));
            require(pToks.valid());
        }
        ++pToks;
        return n;
    }

    void writeTree(ostream& pFile, const AnnotTree::NodePtr& pNode, uint64_t pInd)
    {
        ind(pFile, pInd);
        pFile << "(\n";
        for (map<string,string>::const_iterator i = pNode->anns.begin(); i != pNode->anns.end(); ++i)
        {
            ind(pFile, pInd + 1);
            pFile << i->first << '\t' << i->second << '\n';
        }
        for (vector<AnnotTree::NodePtr>::const_iterator i = pNode->kids.begin(); i != pNode->kids.end(); ++i)
        {
            writeTree(pFile, *i, pInd + 1);
        }
        ind(pFile, pInd);
        pFile << ")\n";
    }
} // namespace anonymous

AnnotTree::NodePtr
AnnotTree::read(istream& pFile)
{
    Tokens toks(pFile);
    return readTree(toks);
}

void
AnnotTree::write(ostream& pFile, const NodePtr& pNode)
{
    writeTree(pFile, pNode, 0);
}
