// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSAMEREXCEPTION_HH
#define GOSSAMEREXCEPTION_HH

#ifndef BOOST_EXCEPTION_HPP
#include <boost/exception/all.hpp>
#define BOOST_EXCEPTION_HPP
#endif

#ifndef STD_EXCEPTION
#include <exception>
#define STD_EXCEPTION
#endif

#ifndef STDINT_H
#include <stdint.h>
#define STDINT_H
#endif

namespace Gossamer
{
    struct error : virtual std::exception, virtual boost::exception {};

    typedef boost::error_info<struct tag_cmd_name_info,std::string> cmd_name_info;
    typedef boost::error_info<struct tag_open_graph_name_info,std::string> open_graph_name_info;
    typedef boost::error_info<struct tag_parse_error_info,std::string> parse_error_info;
    typedef boost::error_info<struct tag_version_mismatch_info,std::pair<uint64_t,uint64_t> > version_mismatch_info;
    typedef boost::error_info<struct tag_file_size_contents_mismatch_info,std::pair<uint64_t,uint64_t> > array_file_size_info;
    typedef boost::error_info<struct tag_write_error_info,std::string> write_error_info;
    typedef boost::error_info<struct tag_general_error_info,std::string> general_error_info;
    typedef boost::error_info<struct tag_usage_info,std::string> usage_info;

    general_error_info range_error(const std::string& pWhere, uint64_t pMax, uint64_t pVal);

}

#endif // GOSSAMEREXCEPTION_HH
