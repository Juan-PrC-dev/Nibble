#include "core.hpp"
#include "animation.hpp"
#include <iostream>
static int count=0;
void check(bool v,const char* msg){++count;if(!v)throw std::runtime_error(msg);}
int main(){try{
    Json fixture={{"rateLimits",{{"primary",{{"usedPercent",27},{"windowDurationMins",300},{"resetsAt",1000}}},{"secondary",{{"usedPercent",34},{"windowDurationMins",10080},{"resetsAt",2000}}}}},{"rateLimitResetCredits",{{"availableCount",0}}}};
    auto s=parseUsage(fixture,100);check(s.limits[0].remaining==73&&s.limits[1].remaining==66,"remaining conversion");
    check(decode(encode(s)).limits[1].reset==2000,"cache round trip");
    AlertState a;Snapshot empty;check(a.apply(s,empty,false).empty(),"healthy startup");
    auto old=s;s.limits[0].remaining=24;check(a.apply(s,old,false).size()==1,"25 threshold");check(a.apply(s,s,false).empty(),"dedupe");
    old=s;s.limits[0].remaining=4;check(a.apply(s,old,false).size()==1,"crossed thresholds consolidate");
    auto restored=AlertState::load(a.json());check(restored.apply(s,s,false).empty(),"persistent dedupe");
    s.limits[1].remaining=9;check(a.apply(s,s,true).empty(),"quiet");check(a.apply(s,s,false).empty(),"no quiet replay");
    old=s;s.limits[0].reset=3000;s.fetched=1001;s.limits[0].remaining=100;check(a.apply(s,old,false).size()==1,"confirmed reset");check(a.severity[1]==2,"independent windows");
    old=s;s.limits[0].reset=4000;s.fetched=1100;check(a.apply(s,old,false).empty(),"timestamp shift is not reset");
    s.credits=1;check(a.apply(s,s,false).size()==1,"credit available");check(a.apply(s,s,false).empty(),"credit dedupe");s.credits=-1;check(a.apply(s,s,false).empty()&&a.credits==1,"unknown credits preserved");
    s.credits=2;check(a.apply(s,s,true).empty(),"quiet credits");check(a.apply(s,s,false).empty(),"credit no replay");
    AlertState b;s.limits[0].remaining=3;check(b.apply(s,empty,false).size()==3,"low first launch plus independent warning and credits");
    bool rejected=false;try{parseUsage(Json::object(),1);}catch(...){rejected=true;}check(rejected,"missing response rejected");
    auto bad=fixture;bad["rateLimits"]["primary"]["usedPercent"]=101;rejected=false;try{parseUsage(bad,1);}catch(...){rejected=true;}check(rejected,"out of range rejected");
    bad=fixture;bad["rateLimits"]["primary"]["usedPercent"]=nullptr;rejected=false;try{parseUsage(bad,1);}catch(...){rejected=true;}check(rejected,"null is not zero");
    bad=fixture;bad["rateLimits"]["secondary"]=nullptr;check(!parseUsage(bad,1).limits[1].valid,"absent allowance");
    bad=fixture;bad["rateLimitsByLimitId"]={{"codex",{{"primary",{{"usedPercent",50},{"windowDurationMins",300},{"resetsAt",1000}}}}}};check(parseUsage(bad,1).limits[0].remaining==50,"multi bucket precedence");
    AlertState early;auto before=parseUsage(fixture,100);before.limits[0].remaining=20;early.apply(before,empty,false);auto after=before;after.fetched=200;after.limits[0].remaining=100;after.limits[0].reset=3000;check(early.apply(after,before,false).size()==1,"early redeemed reset");
    AlertState plan;plan.apply(before,empty,false);after.limits[0].minutes=600;check(plan.apply(after,before,false).empty(),"plan window change is not a reset");
    auto cached=encode(before);cached["limits"][0]["remaining"]=-1;rejected=false;try{decode(cached);}catch(...){rejected=true;}check(rejected,"corrupt cache rejected");
    ExpressionThresholds thresholds;Snapshot view=s;view.limits[0].valid=true;view.limits[1].valid=true;view.limits[1].remaining=90;
    auto mood=[&](double remaining){view.limits[0].remaining=remaining;return chooseMood(view,thresholds,false,false,false,false);};
    check(mood(50.1)==Mood::Happy,"happy above 50");check(mood(50)==Mood::Moderate,"50 attentive");check(mood(25.1)==Mood::Moderate,"attentive above 25");check(mood(25)==Mood::Low,"25 worried");check(mood(10.1)==Mood::Low,"worried above 10");check(mood(10)==Mood::Critical,"10 critical");check(mood(0)==Mood::Critical,"0 critical");
    view.limits[0].remaining=80;view.limits[1].remaining=8;check(chooseMood(view,thresholds,false,true,true,true)==Mood::Critical,"weekly critical beats transient states");
    view.limits[1].remaining=20;check(chooseMood(view,thresholds,false,true,true,true)==Mood::Low,"worried beats celebration or sleep");
    view.limits[1].remaining=90;check(chooseMood(view,thresholds,true,false,false,true)==Mood::Unknown,"stale never pretends to sleep");check(chooseMood(view,thresholds,false,true,false,false)==Mood::Processing,"fetch processing");check(chooseMood(view,thresholds,false,false,true,true)==Mood::Reset,"reset brief state");check(chooseMood(view,thresholds,false,false,false,true)==Mood::Sleeping,"inactive rests");
    Snapshot missing;check(chooseMood(missing,thresholds,false,false,false,false)==Mood::Unknown,"no data unknown");
    check(ExpressionThresholds::load(thresholds.json()).worried==25,"expression settings round trip");check(!ExpressionThresholds{10,25,50}.valid(),"reject reversed expression thresholds");check(!ExpressionThresholds{110,25,10}.valid(),"reject out of range expression thresholds");
    for(int i=0;i<8;i++)check(spriteFor((Mood)i,55)>=0&&spriteFor((Mood)i,55)<8,"sprite index bounded");
    check(spriteFor(Mood::Happy,53)==2&&spriteFor(Mood::Happy,0)==0&&spriteFor(Mood::Happy,8)==0,"stable idle and blink frames");check(!confirmedReset(after,after),"no replayed celebration");
    std::cout<<count<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
