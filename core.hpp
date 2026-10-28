#pragma once
#include "json.hpp"
#include <array>
#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>
using Json = nlohmann::json;
struct Allowance {
    bool valid = false;
    double remaining = 0;
    long long reset = 0;
    int minutes = 0;
};
struct Snapshot {
    std::array<Allowance, 2> limits{};
    int credits = -1;
    long long fetched = 0;
};
inline Json encode(const Snapshot& s) {
    Json a = Json::array();
    for (auto& v : s.limits) a.push_back({{"valid",v.valid},{"remaining",v.remaining},{"reset",v.reset},{"minutes",v.minutes}});
    return {{"limits",a},{"credits",s.credits},{"fetched",s.fetched}};
}
inline Snapshot decode(const Json& j) {
    Snapshot s;
    for (int i=0;i<2;i++) {
        auto v=j.at("limits").at(i);
        s.limits[i]={v.at("valid").get<bool>(),v.at("remaining").get<double>(),v.at("reset").get<long long>(),v.at("minutes").get<int>()};
        auto& a=s.limits[i];
        if(!std::isfinite(a.remaining)||a.remaining<0||a.remaining>100||a.reset<0||a.minutes<0) throw std::runtime_error("Invalid cache");
    }
    s.credits=j.at("credits").get<int>(); s.fetched=j.at("fetched").get<long long>();
    return s;
}
inline Snapshot parseUsage(const Json& r, long long now) {
    const Json* bucket=nullptr;
    if(r.contains("rateLimitsByLimitId")&&r["rateLimitsByLimitId"].is_object()&&r["rateLimitsByLimitId"].contains("codex")) bucket=&r["rateLimitsByLimitId"]["codex"];
    else if(r.contains("rateLimits")&&r["rateLimits"].is_object()) bucket=&r["rateLimits"];
    if(!bucket) throw std::runtime_error("Usage information unavailable. Sign in to Codex and retry.");
    Snapshot s; s.fetched=now;
    for(int i=0;i<2;i++) {
        const char* key=i==0?"primary":"secondary";
        if(!bucket->contains(key)||(*bucket)[key].is_null()) continue;
        auto& a=(*bucket)[key];
        if(!a.contains("usedPercent")||!a["usedPercent"].is_number()||!a.contains("resetsAt")||!a["resetsAt"].is_number_integer()||!a.contains("windowDurationMins")||!a["windowDurationMins"].is_number_integer()) throw std::runtime_error("Unsupported usage response. Update Codex and retry.");
        double used=a["usedPercent"].get<double>();
        long long reset=a["resetsAt"].get<long long>(); int minutes=a["windowDurationMins"].get<int>();
        if(!std::isfinite(used)||used<0||used>100||reset<0||minutes<=0) throw std::runtime_error("Invalid usage response. Retry shortly.");
        s.limits[i]={true,100-used,reset,minutes};
    }
    if(!s.limits[0].valid&&!s.limits[1].valid) throw std::runtime_error("No usage allowances available. Check Codex sign-in.");
    if(r.contains("rateLimitResetCredits")&&r["rateLimitResetCredits"].is_object()) {
        auto& c=r["rateLimitResetCredits"];
        if(c.contains("availableCount")&&c["availableCount"].is_number_integer()) {s.credits=c["availableCount"].get<int>(); if(s.credits<0)s.credits=-1;}
    }
    return s;
}
struct Notice { std::string title, body; };
struct AlertState {
    std::array<long long,2> window{};
    std::array<int,2> severity{};
    int credits=-1;
    Json json() const {return {{"window",window},{"severity",severity},{"credits",credits}};}
    static AlertState load(const Json& j) {
        AlertState s; s.window=j.at("window").get<std::array<long long,2>>();s.severity=j.at("severity").get<std::array<int,2>>();s.credits=j.at("credits").get<int>();return s;
    }
    std::vector<Notice> apply(const Snapshot& s, const Snapshot& previous, bool quiet) {
        std::vector<Notice> result;
        for(int i=0;i<2;i++) {
            auto& a=s.limits[i];if(!a.valid)continue;
            std::string label=i==0?"Short-window allowance":"Weekly allowance";
            bool changed=window[i]!=a.reset;
            // A moved reset timestamp alone can also mean a new rolling window or plan change.
            bool reset=window[i]>0 && a.reset>window[i] && previous.limits[i].valid && a.minutes==previous.limits[i].minutes && a.remaining>previous.limits[i].remaining;
            if(changed) {window[i]=a.reset;severity[i]=0;}
            if(reset) result.push_back({label+" has reset", "Your allowance is available again. Check Nibble for the current remaining percentage."});
            int level=a.remaining<=5?3:a.remaining<=10?2:a.remaining<=25?1:0;
            if(level>severity[i]) {severity[i]=level; result.push_back({label+" running low",std::to_string((int)std::round(a.remaining))+"% remaining. Consider pausing until the reset."});}
        }
        if(s.credits>=0) {
            if(s.credits>0 && (credits<0||s.credits>credits)) result.push_back({"Limit-reset credit available",std::to_string(s.credits)+" available. Open Codex to redeem an eligible reset."});
            credits=s.credits;
        }
        if(quiet) result.clear();
        return result;
    }
};
