#include "GossamerException.hh"

#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

Gossamer::general_error_info
Gossamer::range_error(const std::string& pWhere, uint64_t pMax, uint64_t pVal)
{
    string s("out of range access on " + pWhere
                + ". size  = " + lexical_cast<string>(pMax)
                + " index = " + lexical_cast<string>(pVal));

    return Gossamer::general_error_info(s);
}
