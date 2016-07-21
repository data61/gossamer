#include "GossCmdReg.hh"

using namespace std;

map<string,GossCmdFactoryPtr>* GossCmdReg::cmds = NULL;
