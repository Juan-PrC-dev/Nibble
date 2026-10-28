#include "connection.hpp"
#include <iostream>
#include <thread>
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Pass fixture-server.exe path");
    SetEnvironmentVariableW(L"NIBBLE_CODEX_PATH",wide(fs::absolute(argv[1]).string()).c_str());
    for(auto mode:{"valid","signedout","offline","timeout","crash","unsupported","invalid"}){
        SetEnvironmentVariableW(L"NIBBLE_FIXTURE",wide(mode).c_str());std::atomic_bool stop=false;auto r=fetchUsage(stop,500);
        bool valid=std::string(mode)=="valid";
        if(valid&&( !r.error.empty()||!r.clean||r.snapshot.limits[0].remaining!=73))throw std::runtime_error("Valid fixture failed");
        if(!valid&&r.error.empty())throw std::runtime_error(std::string("Failure was not reported: ")+mode);
        if(r.helperPid){Handle p(OpenProcess(SYNCHRONIZE,FALSE,r.helperPid));if(p.h&&WaitForSingleObject(p.h,1000)!=WAIT_OBJECT_0)throw std::runtime_error("Helper leaked");}
        std::cout<<mode<<": passed\n";
    }
    SetEnvironmentVariableW(L"NIBBLE_FIXTURE",L"timeout");std::atomic_bool cancel=false;std::thread cancellation([&]{Sleep(100);cancel=true;});auto r=fetchUsage(cancel,20000);cancellation.join();if(!r.clean)throw std::runtime_error("Cancellation cleanup failed");std::cout<<"cancel: passed\n";
    SetEnvironmentVariableW(L"NIBBLE_CODEX_PATH",L"C:\\Nibble-nonexistent-test\\codex.exe");cancel=false;r=fetchUsage(cancel);if(r.error.empty()||r.helperPid)throw std::runtime_error("Missing Codex failure");std::cout<<"missing executable: passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
