#include "connection.hpp"
#include <iostream>
int main(){std::atomic_bool stop=false;auto r=fetchUsage(stop);Json j={{"error",r.error},{"peakTreeWorkingSetMB",r.peakTreeWorkingSet/1048576.0},{"peakTreePrivateMB",r.peakTreePrivate/1048576.0},{"helperCleanupConfirmed",r.clean}};if(r.error.empty())j["snapshot"]=encode(r.snapshot);std::cout<<j.dump(2)<<std::endl;return r.error.empty()?0:1;}
