// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef PROPERTIES_HH
#define PROPERTIES_HH

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef BOOST_LEXICAL_CAST_HPP
#include <boost/lexical_cast.hpp>
#define BOOST_LEXICAL_CAST_HPP
#endif

#ifndef GOSSAMEREXCEPTION_HH
#include "GossamerException.hh"
#endif

class PropertyTree
{
public:
    template <typename T>
    T as(const std::string& pProp) const
    {
        std::map<std::string,std::string>::const_iterator i;
        i = mProps.find(pProp);
        if (i == mProps.end())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("PropertyTree::as: unknown property '" + pProp + "'"));
        }
        return boost::lexical_cast<T>(i->second);
    }

    const PropertyTree& operator()(const std::string& pSub) const
    {
        std::map<std::string,PropertyTree>::const_iterator i;
        i = mKids.find(pSub);
        if (i == mKids.end())
        {
            BOOST_THROW_EXCEPTION(
                Gossamer::error()
                    << Gossamer::general_error_info("PropertyTree::operator(): unknown property '" + pSub + "'"));
        }
        return i->second;
    }

    template <typename T>
    void putProp(const std::string& pProp, const T& pVal)
    {
        mProps[pProp] = boost::lexical_cast<std::string>(pVal);
    }

    void putSub(const std::string& pSub, const PropertyTree& pVal)
    {
        mKids[pSub] = pVal;
    }

    void print(std::ostream& pOut, uint64_t pInd = 0) const
    {
        for (std::map<std::string,std::string>::const_iterator i = mProps.begin(); i != mProps.end(); ++i)
        {
            ind(pOut, pInd);
            pOut << i->first << '\t' << i->second << std::endl;
        }
        for (std::map<std::string,PropertyTree>::const_iterator i = mKids.begin(); i != mKids.end(); ++i)
        {
            ind(pOut, pInd);
            pOut << i->first << '\n';
            i->second.print(pOut, pInd + 1);
        }
    }
private:
    static void ind(std::ostream& pOut, uint64_t pInd)
    {
        for (uint64_t i = 0; i < pInd; ++i)
        {
            pOut << ' ';
        }
    }

    std::map<std::string,std::string> mProps;
    std::map<std::string,PropertyTree> mKids;
};

#endif // PROPERTIES_HH
