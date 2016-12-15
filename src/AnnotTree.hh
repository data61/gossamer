// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef ANNOTTREE_HH
#define ANNOTTREE_HH

#ifndef STD_ISTREAM
#include <istream>
#define STD_ISTREAM
#endif

#ifndef STD_MAP
#include <map>
#define STD_MAP
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

#ifndef STD_VECTOR
#include <vector>
#define STD_VECTOR
#endif

#ifndef STD_MEMORY
#include <memory>
#define STD_MEMORY
#endif

class AnnotTree
{
public:
    struct Node;
    typedef std::shared_ptr<Node> NodePtr;

    struct Node
    {
        std::map<std::string,std::string> anns;
        std::vector<NodePtr> kids;
    };

    static NodePtr read(std::istream& pFile);

    static void write(std::ostream& pFile, const NodePtr& pNode);
};

#endif // ANNOTTREE_HH
