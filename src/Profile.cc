#include "Profile.hh"

boost::mutex Profile::sMutex;
boost::thread_specific_ptr<Profile::NodePtr> Profile::sCurrentNodePtr;
Profile::RootsPtr Profile::sThreadRoots;
