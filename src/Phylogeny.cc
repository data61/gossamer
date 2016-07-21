#include "Phylogeny.hh"
#include "AnnotTree.hh"

using namespace std;
using namespace boost;

namespace // anonymous
{
    const string& get(const map<string,string>& pMap, const string& pKey)
    {
        map<string,string>::const_iterator i = pMap.find(pKey);
        if (i == pMap.end())
        {
            throw 42;
        }
        return i->second;
    }
} // namespace anonymous

void
Phylogeny::read(const string& pFileName, FileFactory& pFactory)
{
    FileFactory::InHolderPtr inp(pFactory.in(pFileName));
    istream& in(**inp);

    mRoot = AnnotTree::read(in);
    mRootNode = index(mRoot);
    mParentIndex[mRootNode] = mRootNode;
}

void
Phylogeny::write(ostream& pOut) const
{
    AnnotTree::write(pOut, mRoot);
}

uint32_t
Phylogeny::index(const AnnotTree::NodePtr& pNode)
{
    uint32_t n = lexical_cast<uint32_t>(get(pNode->anns, "node"));
    mNameIndex[n] = get(pNode->anns, "name");
    mNodeIndex[n] = pNode;
    for (vector<AnnotTree::NodePtr>::const_iterator i = pNode->kids.begin(); i != pNode->kids.end(); ++i)
    {
        uint32_t c = index(*i);
        mParentIndex[c] = n;
        mChildIndex[n].push_back(c);
    }
    return n;
}
