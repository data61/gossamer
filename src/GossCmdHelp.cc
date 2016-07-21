#include "GossCmdHelp.hh"
#include "GossCmdReg.hh"
#include "Debug.hh"
#include <iostream>
#include <stdexcept>

using namespace boost;
using namespace std;

namespace // anonymous
{
    Debug listDebugs("list", "print a list of the available debugs");

    class GossCmdHelp : public GossCmd
    {
    public:
        void operator()(const GossCmdContext& pCxt)
        {
            typedef map<string,GossCmdFactoryPtr> CmdMap;
            if (listDebugs.on())
            {
                cerr << "available debugs:" << endl;
                Debug::print(cerr);
                return;
            }

            BOOST_ASSERT(GossCmdReg::cmds);
            CmdMap& cmds(*GossCmdReg::cmds);

            size_t maxLen = 0;
            for (CmdMap::const_iterator i = cmds.begin(); i != cmds.end(); ++i)
            {
                maxLen = max(i->first.length(), maxLen);
            }

            cerr << mName << " commands:" << endl;
            for (CmdMap::const_iterator i = cmds.begin(); i != cmds.end(); ++i)
            {
                string name = i->first;
                name += string(maxLen - name.length(), ' ');
                cerr << '\t' << name << '\t' << i->second->desc() << endl;
            }
        }

        GossCmdHelp(const string& pName)
            : mName(pName)
        {
        }

    private:
        string mName;
    };
} // namespace anonymous

GossCmdPtr
GossCmdFactoryHelp::create(App& pApp,
                           const boost::program_options::variables_map& pOpts)
{
    return GossCmdPtr(new GossCmdHelp(pApp.name()));
}

GossCmdFactoryHelp::GossCmdFactoryHelp(const App& pApp)
    : GossCmdFactory("print a summary of all the " + string(pApp.name()) + " commands")
{
}
