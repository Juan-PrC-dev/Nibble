#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <filesystem>
#include <atomic>
#include <chrono>
#include <fstream>
#include "core.hpp"
namespace fs=std::filesystem;
inline std::wstring env(const wchar_t* name){DWORD n=GetEnvironmentVariableW(name,nullptr,0);if(!n)return {};std::wstring s(n,L'\0');GetEnvironmentVariableW(name,s.data(),n);s.resize(n-1);return s;}
inline long long utcNow(){return (long long)std::time(nullptr);}
inline std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n);return w;}
struct Handle {HANDLE h=nullptr;Handle()=default;explicit Handle(HANDLE v):h(v){};~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;};
inline fs::path findCodex(){
    auto overridePath=env(L"NIBBLE_CODEX_PATH");
    if(!overridePath.empty()){if(fs::is_regular_file(overridePath))return overridePath;throw std::runtime_error("Configured Codex executable is missing. Check NIBBLE_CODEX_PATH.");}
    wchar_t found[32768];DWORD n=SearchPathW(nullptr,L"codex.exe",nullptr,32768,found,nullptr);if(n&&n<32768)return found;
    fs::path root=fs::path(env(L"LOCALAPPDATA"))/L"OpenAI/Codex/bin";std::vector<fs::path> choices;std::error_code ec;
    for(auto& p:fs::directory_iterator(root,ec))if(fs::is_regular_file(p.path()/L"codex.exe",ec))choices.push_back(p.path()/L"codex.exe");
    if(!choices.empty()){std::sort(choices.begin(),choices.end(),[](auto& a,auto& b){return fs::last_write_time(a)>fs::last_write_time(b);});return choices[0];}
    throw std::runtime_error("Codex was not found. Install/open Codex, or set NIBBLE_CODEX_PATH to codex.exe.");
}
struct FetchResult {Snapshot snapshot;std::string error;SIZE_T peakTreeWorkingSet=0;SIZE_T peakTreePrivate=0;DWORD helperPid=0;bool clean=false;};
inline void memorySample(HANDLE job,FetchResult& r){
    alignas(8) BYTE buf[sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST)+sizeof(ULONG_PTR)*128]{};
    if(!QueryInformationJobObject(job,JobObjectBasicProcessIdList,buf,sizeof(buf),nullptr))return;
    auto ids=reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buf);SIZE_T ws=0,pv=0;
    for(DWORD i=0;i<ids->NumberOfProcessIdsInList;i++){Handle p(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,(DWORD)ids->ProcessIdList[i]));PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);if(p.h&&GetProcessMemoryInfo(p.h,(PROCESS_MEMORY_COUNTERS*)&m,sizeof(m))){ws+=m.WorkingSetSize;pv+=m.PrivateUsage;}}
    PROCESS_MEMORY_COUNTERS_EX own{};own.cb=sizeof(own);if(GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&own,sizeof(own))){ws+=own.WorkingSetSize;pv+=own.PrivateUsage;}
    r.peakTreeWorkingSet=std::max(r.peakTreeWorkingSet,ws);r.peakTreePrivate=std::max(r.peakTreePrivate,pv);
}
inline FetchResult fetchUsage(std::atomic_bool& stop,DWORD timeout=20000){
    FetchResult result;
    try{
        auto exe=findCodex();
        Handle job(CreateJobObjectW(nullptr,nullptr));if(!job.h)throw std::runtime_error("Could not create helper job.");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))throw std::runtime_error("Could not configure helper cleanup.");
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE readOutRaw,writeOutRaw,readInRaw,writeInRaw;
        if(!CreatePipe(&readOutRaw,&writeOutRaw,&sa,0))throw std::runtime_error("Could not create helper pipe.");
        Handle readOut(readOutRaw),writeOut(writeOutRaw);
        if(!CreatePipe(&readInRaw,&writeInRaw,&sa,0))throw std::runtime_error("Could not create helper pipe.");
        Handle readIn(readInRaw),writeIn(writeInRaw);
        SetHandleInformation(readOut.h,HANDLE_FLAG_INHERIT,0);SetHandleInformation(writeIn.h,HANDLE_FLAG_INHERIT,0);
        Handle nullErr(CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr));
        STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.StartupInfo.wShowWindow=SW_HIDE;si.StartupInfo.hStdInput=readIn.h;si.StartupInfo.hStdOutput=writeOut.h;si.StartupInfo.hStdError=nullErr.h;
        SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<BYTE> attrs(bytes);si.lpAttributeList=(LPPROC_THREAD_ATTRIBUTE_LIST)attrs.data();
        if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&bytes))throw std::runtime_error("Could not configure helper handles.");
        HANDLE inherited[]={readIn.h,writeOut.h,nullErr.h};
        if(!UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)){DeleteProcThreadAttributeList(si.lpAttributeList);throw std::runtime_error("Could not configure helper handles.");}
        std::wstring cmd=L"\""+exe.wstring()+L"\" app-server --listen stdio://";
        PROCESS_INFORMATION pi{};
        BOOL created=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,exe.parent_path().c_str(),&si.StartupInfo,&pi);
        DeleteProcThreadAttributeList(si.lpAttributeList);
        if(!created)throw std::runtime_error("Could not start Codex. Open Codex and retry.");
        Handle process(pi.hProcess),thread(pi.hThread);result.helperPid=pi.dwProcessId;
        if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,1);throw std::runtime_error("Could not contain Codex helper for cleanup.");}
        ResumeThread(thread.h);CloseHandle(writeOut.h);writeOut.h=nullptr;CloseHandle(readIn.h);readIn.h=nullptr;
        auto send=[&](std::string line){line+='\n';DWORD wrote=0;return WriteFile(writeIn.h,line.data(),(DWORD)line.size(),&wrote,nullptr)&&wrote==line.size();};
        if(!send(R"({"id":1,"method":"initialize","params":{"clientInfo":{"name":"nibble","title":"Nibble","version":"1.0.0"}}})"))throw std::runtime_error("Could not initialize Codex helper.");
        std::string buffer;ULONGLONG start=GetTickCount64();bool success=false,initialized=false;
        while(!stop && GetTickCount64()-start<timeout){
            memorySample(job.h,result);
            DWORD avail=0;if(!PeekNamedPipe(readOut.h,nullptr,0,nullptr,&avail,nullptr))break;
            if(avail){char chunk[8192];DWORD got=0;if(!ReadFile(readOut.h,chunk,std::min<DWORD>(sizeof(chunk),avail),&got,nullptr))break;buffer.append(chunk,got);if(buffer.size()>1024*1024)throw std::runtime_error("Unexpected helper response size.");
                size_t pos;while((pos=buffer.find('\n'))!=std::string::npos){auto line=buffer.substr(0,pos);buffer.erase(0,pos+1);auto j=Json::parse(line,nullptr,false);if(j.is_discarded()||!j.is_object()||!j.contains("id")||!j["id"].is_number_integer())continue;
                    int id=j["id"].get<int>();if(id==1&&!initialized){if(j.contains("error"))throw std::runtime_error("Codex initialization failed. Open/update Codex and retry.");initialized=true;if(!send(R"({"method":"initialized"})")||!send(R"({"id":2,"method":"account/rateLimits/read"})"))throw std::runtime_error("Codex helper disconnected.");}
                    if(id==2){if(j.contains("error"))throw std::runtime_error("Usage request failed. Check Codex sign-in/network, update Codex, then refresh.");result.snapshot=parseUsage(j.at("result"),utcNow());success=true;break;}
                }
                if(success)break;
            }else if(WaitForSingleObject(process.h,0)==WAIT_OBJECT_0)break;
            Sleep(20);
        }
        memorySample(job.h,result);
        // Every process in this private job was spawned for this single read-only request.
        TerminateJobObject(job.h,0);WaitForSingleObject(process.h,3000);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        for(int i=0;i<100;i++){QueryInformationJobObject(job.h,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr);if(!accounting.ActiveProcesses)break;Sleep(10);}
        result.clean=accounting.ActiveProcesses==0;
        if(!success&&!stop)throw std::runtime_error(GetTickCount64()-start>=timeout?"Usage request timed out. Check your network and Codex sign-in; retry shortly.":"Codex helper exited. Open Codex, confirm sign-in, then refresh.");
    }catch(const std::exception& e){result.error=e.what();}
    return result;
}
