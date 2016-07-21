#ifndef DEBUG_HH
#define DEBUG_HH

#ifndef STD_STRING
#include <string>
#define STD_STRING
#endif

#ifndef STD_OSTREAM
#include <ostream>
#define STD_OSTREAM
#endif

class Debug
{
public:
    const bool& on() const
    {
        return mOn;
    }

    void enable()
    {
        mOn = true;
    }

    void disable()
    {
        mOn = false;
    }

    static bool enable(const std::string& pName);

    static bool disable(const std::string& pName);

    static void print(std::ostream& pOut);

    Debug(const std::string& pName, const std::string& pMsg);

private:
    bool mOn;
    std::string mMsg;
};

#endif // DEBUG_HH
