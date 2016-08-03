#include "Profile.hh"

std::mutex Profile::sMutex;
boost::thread_specific_ptr<Profile::NodePtr> Profile::sCurrentNodePtr;
Profile::RootsPtr Profile::sThreadRoots;
