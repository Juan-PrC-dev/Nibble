#pragma once
#include "core.hpp"
#include <algorithm>
#include <optional>

struct ExpressionThresholds {
    double attentive=50, worried=25, critical=10;
    bool valid() const {return std::isfinite(attentive)&&std::isfinite(worried)&&std::isfinite(critical)&&critical>=0&&critical<worried&&worried<attentive&&attentive<=100;}
    Json json() const {return {{"attentive",attentive},{"worried",worried},{"critical",critical}};}
    static ExpressionThresholds load(const Json& j){ExpressionThresholds t{j.at("attentive").get<double>(),j.at("worried").get<double>(),j.at("critical").get<double>()};if(!t.valid())throw std::runtime_error("Invalid expression thresholds");return t;}
};
enum class Mood {Happy,Moderate,Low,Critical,Processing,Sleeping,Reset,Unknown};
inline const char* moodName(Mood m){switch(m){case Mood::Happy:return "Happy";case Mood::Moderate:return "Attentive";case Mood::Low:return "Worried";case Mood::Critical:return "Critical";case Mood::Processing:return "Checking usage";case Mood::Sleeping:return "Resting";case Mood::Reset:return "Reset!";default:return "Check connection";}}
inline std::optional<double> leastRemaining(const Snapshot& s){std::optional<double> v;for(auto& a:s.limits)if(a.valid)v=v?std::min(*v,a.remaining):a.remaining;return v;}
inline Mood chooseMood(const Snapshot& s,const ExpressionThresholds& t,bool stale,bool fetching,bool celebrating,bool inactive){
    auto remaining=leastRemaining(s);
    // A known low allowance always remains visible through fetching, resting or celebration.
    if(remaining&&*remaining<=t.critical)return Mood::Critical;
    if(remaining&&*remaining<=t.worried)return Mood::Low;
    if(stale&&!fetching)return Mood::Unknown;
    if(fetching)return Mood::Processing;
    if(!remaining)return Mood::Unknown;
    if(celebrating)return Mood::Reset;
    if(inactive)return Mood::Sleeping;
    return *remaining<=t.attentive?Mood::Moderate:Mood::Happy;
}
inline int spriteFor(Mood m,unsigned long long tick){switch(m){
    case Mood::Happy:if(tick%56>=53)return 2;return 0;
    case Mood::Moderate:case Mood::Processing:case Mood::Unknown:return 3;
    case Mood::Low:return 4;case Mood::Critical:return 5;case Mood::Reset:return 6;case Mood::Sleeping:return 7;
    }return 0;
}
inline bool confirmedReset(const Snapshot& current,const Snapshot& previous){for(int i=0;i<2;i++){auto& a=current.limits[i];auto& b=previous.limits[i];if(a.valid&&b.valid&&a.minutes==b.minutes&&b.reset>0&&a.reset>b.reset&&a.remaining>b.remaining)return true;}return false;}
