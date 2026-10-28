// Isolated integration-test server. Never contacts a network or reads account data.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include "json.hpp"
using Json=nlohmann::json;
int main(){char m[64]{};GetEnvironmentVariableA("NIBBLE_FIXTURE",m,64);std::string mode=m,line;
    while(std::getline(std::cin,line)){
        auto j=Json::parse(line,nullptr,false);if(j.is_discarded())continue;
        if(j.value("method","")=="initialize")std::cout<<R"({"id":1,"result":{}})"<<std::endl;
        if(j.value("method","")=="account/rateLimits/read"){
            if(mode=="timeout"){Sleep(30000);return 0;}
            if(mode=="crash")return 1;
            if(mode=="signedout"||mode=="offline"){std::cout<<R"({"id":2,"error":{"code":-32000,"message":"fixture failure"}})"<<std::endl;continue;}
            Json r={{"rateLimits",{{"primary",{{"usedPercent",27},{"windowDurationMins",300},{"resetsAt",2000000000}}},{"secondary",{{"usedPercent",34},{"windowDurationMins",10080},{"resetsAt",2000600000}}}}},{"rateLimitResetCredits",{{"availableCount",1}}}};
            if(mode=="unsupported")r=Json::object();
            if(mode=="invalid")r["rateLimits"]["primary"]["usedPercent"]=nullptr;
            std::cout<<Json({{"id",2},{"result",r}}).dump()<<std::endl;
        }
    }
}
