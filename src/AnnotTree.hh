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
