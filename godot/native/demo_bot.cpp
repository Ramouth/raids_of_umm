// Scripted, omniscient route verification using production movement, economy and
// CombatEngine. No granted resources, forced victories or save-file teleports.
// Usage: demo_bot <repo_root> [conquest|bypass|slow|escort] [seed] [--transcript]
// --transcript: stdout carries only what a player is shown (dialogue, quests, lore,
// choices, visits, battles), as plain lines; debug output moves to stderr.
#include "adventure/AdventureSession.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h>

using Stack = AdventureSession::Stack;

// Player-view transcript: diffs the session after each bot step and prints only
// display text. No ids, coordinates or power numbers.
struct Transcript {
    FILE* out=nullptr;   // null = transcript off
    int day=0;
    std::map<std::string,std::pair<std::string,bool>> quests;   // title -> text, done
    size_t lore=0;
    std::string choice;
    std::set<std::string> offers, companions, items;
    bool ended=false;
    static std::string flat(std::string t) {
        std::string r;
        for(size_t i=0;i<t.size();++i) {
            if(t[i]=='\r') continue;
            if(t[i]=='\n') { while(!r.empty() && r.back()==' ') r.pop_back(); r+=" / "; while(i+1<t.size() && t[i+1]==' ') ++i; }
            else r+=t[i];
        }
        for(;;) {   // no trailing spaces, no dangling separator from a final newline
            while(!r.empty() && r.back()==' ') r.pop_back();
            if(r.size()>=2 && r.compare(r.size()-2,2," /")==0) r.resize(r.size()-2); else break;
        }
        return r;
    }
    static std::string titled(const std::string& head,const std::string& text) { return text.empty()?head:head+": "+text; }
    void line(const std::string& t) { if(out) { std::fprintf(out,"%s\n",flat(t).c_str()); std::fflush(out); } }
    std::string itemName(const AdventureSession& s,const std::string& id) {
        auto it=s.resources().item(id);
        return it && !it->name.empty()?it->name:id;
    }
    void sync(AdventureSession& s) {
        if(!out) { s.scenario().drainLines(); return; }
        auto& sc=s.scenario();
        if(s.day()!=day) { day=s.day(); line("DAY "+std::to_string(day)); }
        for(const auto& l:sc.drainLines()) line("SAY "+l.speaker+": "+l.text);
        for(const auto& u:s.drainLevelUps()) line("LEVEL "+std::to_string(u.level)+(u.skill.empty()?"":" "+u.skill)+(u.text.empty()?"":" — "+u.text));
        for(const auto& q:sc.quests()) {
            auto it=quests.find(q.title);
            if(it==quests.end()) {
                line("QUEST+ "+std::string(q.main?"main":"side")+" "+q.title+": "+q.text);
                it=quests.emplace(q.title,std::make_pair(q.text,false)).first;
            } else if(it->second.first!=q.text) {
                line("QUEST~ "+q.title+": "+q.text);
                it->second.first=q.text;
            }
            if(q.done && !it->second.second) { line("QUEST✓ "+q.title); it->second.second=true; }
        }
        for(;lore<sc.lore().size();++lore) line("LORE "+sc.lore()[lore].title+": "+sc.lore()[lore].text);
        std::set<std::string> now;
        for(const auto& sp:s.specials()) {
            now.insert(sp.name);
            if(!companions.count(sp.name)) line("JOINS "+sp.name);
        }
        for(const auto& n:companions) if(!now.count(n)) line("LEAVES "+n);
        companions=now;
        for(const auto& id:s.items()) if(items.insert(id).second) line("ITEM "+itemName(s,id));
        if(sc.awaitingChoice()) {
            auto key=sc.choice().dump();
            if(key!=choice) {
                choice=key;
                for(const auto& l:sc.choice().value("lines",Scenario::Json::array()))   // a conversation leading to the choice
                    if(l.is_array() && l.size()>=2) line("SAY "+l[0].get<std::string>()+": "+l[1].get<std::string>());
                line(titled("CHOICE "+sc.choice().value("title",""),sc.choice().value("text","")));
                for(const auto& o:sc.choice().value("options",Scenario::Json::array())) {
                    std::string detail=o.value("detail","");
                    line("  OPTION "+o.value("label","")+(detail.empty()?"":" — "+detail));
                }
            }
        } else choice.clear();
        if(auto obj=s.map().objectAt(s.heroPos()))
            for(const auto* o:sc.offersAt(obj->name))
                if(offers.insert(o->id).second) line("OFFER "+obj->name+": "+o->label);
        if(!ended && !sc.outcome().empty()) {
            ended=true;
            line(titled("ENDING "+sc.outcome().value("heading",""),sc.outcome().value("body","")));
        }
    }
    void chose(AdventureSession& s,const std::string& id,const std::optional<std::string>& err) {
        if(!out) return;
        if(err) { line("REFUSED "+*err); return; }
        for(const auto& o:Scenario::Json::parse(choice.empty()?"{}":choice).value("options",Scenario::Json::array()))
            if(o.value("id","")==id) line("CHOSE "+o.value("label",""));
        sync(s);
    }
};
static Transcript tx;
static HexCoord cellOf(const AdventureSession& s, const std::string& name) {
    for (const auto& o : s.map().objects()) if (o.name == name) return o.pos;
    throw std::runtime_error("Missing destination: " + name);
}
static std::vector<Stack> guards(const AdventureSession& s, HexCoord at) {
    std::vector<Stack> out;
    if (auto e = s.encounterAt(at))
        for (const auto& g : Scenario::Json::parse(e->guardsJson)) out.push_back({g.at("id"),g.at("count")});
    return out;
}
static void recruit(AdventureSession& s) {
    auto t = s.town(s.heroPos());
    if (!t || s.owner(s.heroPos()) != 1) return;
    std::vector<std::pair<int,std::string>> order;
    for (const auto& [id,n] : t->recruitPool) order.push_back({-s.resources().unit(id)->tier,id});
    std::sort(order.begin(),order.end());
    for (const auto& [tier,id] : order)
        for (int n=t->recruitPool.at(id); n>0; --n) if (!s.recruit(s.heroPos(),id,n)) break;
}
static void build(AdventureSession& s) {
    for (const auto& o : s.map().objects()) {
        if (o.type!=ObjType::Town || s.owner(o.pos)!=1 || s.treasury()[Resource::Gold]<3000) continue;
        for (const char* id : {"town_hall","armoury","citadel","horse_lines","drill_yard","castle","knights_hall"})
            if (s.buildBlocker(o.pos,id).empty()) { s.build(o.pos,id); break; }
    }
}
static void fight(AdventureSession& s) {
    auto at=*s.pendingEncounter();
    auto enemy=guards(s,at);
    auto obj=s.map().objectAt(at);
    auto band=s.rivalAt(at);
    const std::string foe=obj?obj->name:band?band->name:"field army";
    tx.sync(s);
    std::cout<<"BATTLE day="<<s.day()<<" at="<<(obj?obj->name:"field army")<<" power="<<int(s.power(s.army()))<<" enemy="<<int(s.power(enemy))<<"\n";
    for (const auto& id:s.backpack()) s.equip(id);
    auto r=s.autoBattle(s.army(),enemy,s.battleCompanions(),s.armyBonus());
    s.setArmy(r.attacker);
    s.companionsFell(r.fallen,!r.attackerWon);
    s.resolveEncounter(r.attackerWon,r.defender);
    tx.line("BATTLE "+foe+" -> "+(r.attackerWon?"victory":"defeat"));
    tx.sync(s);
    std::cout<<"RESULT "<<(r.attackerWon?"victory":"defeat")<<" remaining="<<int(s.power(s.army()))<<"\n";
}
static HexCoord nextStop(AdventureSession& s,HexCoord goal) {
    if (s.heroPos()==goal || !s.route(goal).empty()) return goal;
    auto path=s.map().findPathWeighted(s.heroPos(),goal,[](const HexCoord&){return false;});
    for(auto at:path) if(at!=s.heroPos() && s.isEncounter(at)) return at;
    return goal;
}
int main(int argc,char** argv) {
    const std::string root=argc>1?argv[1]:".";
    const std::string mode=argc>2?argv[2]:"bypass";
    const int seed=argc>3?std::stoi(argv[3]):1;
    for(int i=1;i<argc;++i) if(std::string(argv[i])=="--transcript") {
        // Keep the real stdout for the transcript; everything else written to fd 1 goes to stderr.
        std::fflush(stdout); std::cout.flush();
        int fd=dup(1);
        if(fd<0 || dup2(2,1)<0 || !(tx.out=fdopen(fd,"w"))) {std::cerr<<"cannot open transcript\n";return 1;}
    }
    AdventureSession s;
    if(auto e=s.start(root+"/data/maps/old_passage.json",root+"/data",root+"/data/maps/old_passage.encounters.json",seed,root+"/data/maps/old_passage.triggers.json")) {std::cerr<<*e;return 1;}
    s.setArmy({{"woad_runner",24},{"cruth_slinger",10},{"painted_blade",3}});
    tx.sync(s);
    auto endDay=[&] { tx.sync(s); auto ev=s.endDay(); if(!ev.empty()) tx.line("NOTE "+ev); tx.sync(s); };
    auto claim=[&] { auto found=s.claimChest(true); if(!found.empty()) tx.line("NOTE "+found); tx.sync(s); };
    std::string lastSite;
    auto arrived=[&] {
        auto obj=s.map().objectAt(s.heroPos());
        std::string here=obj?obj->name:"";
        if(!here.empty() && here!=lastSite) tx.line("VISIT "+here);
        lastSite=here;
        tx.sync(s);
    };
    if(mode=="slow") for(int n=0;n<14;++n) endDay();
    std::vector<std::string> plan={"Varenhold Wall Rune Stone","Hill Wolves","Den Wolves","Hermit's Wolves",
        "Varen Gold Mine","Log Pile","Pinewood Sawmill","Vale Quarry","Bridge Wardens","Coldwater Bridge Rune Stone","Hallowmere",
        "Hallow Quarry","Mere Sawmill","Assembly Rune Stone"};
    if(mode=="conquest") {
        plan.push_back("Greyfang Watch");
        plan.push_back("Aldren's Camp");
        plan.push_back("Lord Edric Rook");
        plan.push_back("Rookwatch");
    }
    else if(mode=="escort") plan.push_back("Greyfang Watch");
    else plan.push_back("Troll Gate");
    plan.push_back("Aldren's Camp"); plan.push_back("Old Mine of Kaldur");
    size_t objective=0;
    double wanted=0;
    bool restock=false;
    int stalls=0;
    for(int tick=0;tick<3000 && s.day()<140 && !s.won() && !s.lost() && !s.army().empty();++tick) {
        if(s.pendingEncounter()) { fight(s); continue; }
        if(s.pendingChest()) claim();
        if(s.needsPath()) {
            s.choosePath("marshal");
            for(const auto& p:s.heroPaths()) if(p.id==s.heroPath()) tx.line("PATH "+p.name);
            tx.sync(s);
        }
        if(s.scenario().awaitingChoice()) {
            const auto id=s.scenario().choice().value("id","");
            std::string pick;
            if(id=="ushari_at_night") pick="stand_by_aldren";
            else if(id=="rook_passage") pick=mode=="escort"?"ask_rook_passage":"decline_rook_passage";
            else if(id=="at_the_passage") pick=mode=="slow"?"return_to_father":"follow_ushari";
            else if(id=="passage_gate" && s.heroPos()==*s.passageMine()) pick="esh_varen_tal_eren_orun_ishara";
            std::optional<std::string> e;
            if(!pick.empty()) { e=s.scenario().choose(s,pick); tx.chose(s,pick,e); }
            if(e) {std::cerr<<"CHOICE ERROR "<<*e<<"\n";break;}
            if(id!="passage_gate") continue;
        }
        recruit(s);
        if(s.heroPos()==cellOf(s,"Hallowmere")) {
            for(const char* id:{"rookwatch_supplies","hire_hale"}) {
                auto o=s.scenario().offer(id);
                if(!o) continue;
                const bool open=!o->taken;
                const std::string label=o->label;
                auto err=s.acceptOffer(id);
                if(open && !err) tx.line("ACCEPT "+label);   // the bot retries; only a success is shown
                tx.sync(s);
            }
        }
        if(restock && s.power(s.army())>=wanted) restock=false;
        if(objective>=plan.size()) break;
        bool hunting = plan[objective]=="Lord Edric Rook";
        const AdventureSession::Rival* lord=nullptr;
        for(const auto& r:s.rivals()) if(r.name=="Lord Edric Rook" && r.alive) lord=&r;
        if(hunting && !lord) {++objective; restock=false; continue;}
        HexCoord objectiveCell=hunting?lord->pos:cellOf(s,plan[objective]);
        HexCoord goal=objectiveCell;
        if(!restock && s.heroPos()==goal && !s.isEncounter(goal)) {
            std::cout<<"VISIT day="<<s.day()<<" "<<plan[objective]<<"\n";
            ++objective; stalls=0;
            if(plan[objective-1]=="Hermit's Wolves" && s.specials().empty()) endDay();
            continue;
        }
        if(restock) {
            double best=1e9;
            for(const auto& o:s.map().objects()) if(o.type==ObjType::Town && s.owner(o.pos)==1) {
                auto p=s.route(o.pos);
                if(o.pos!=s.heroPos() && p.empty()) continue;
                double cost=s.routeCost(p);
                if(cost<best) {best=cost;goal=o.pos;}
            }
        }
        auto target=nextStop(s,goal);
        auto path=s.route(target);
        if(!restock) for(auto at:path) {
            auto enemy=s.isEncounter(at)?std::optional<HexCoord>(at):s.guardZoneAt(at);
            if(!enemy) continue;
            wanted=s.power(guards(s,*enemy))*1.5;
            if(s.power(s.army())<wanted) {
                restock=true;
                std::cout<<"RESUPPLY day="<<s.day()<<" power="<<int(s.power(s.army()))<<" need="<<int(wanted)<<" for="<<plan[objective]<<" blocker="<<(s.map().objectAt(*enemy)?s.map().objectAt(*enemy)->name:"field army")<<"\n";
            }
            break;
        }
        if(restock && goal==objectiveCell) continue;
        auto before=s.heroPos();
        s.travel(target);
        arrived();
        if(s.pendingEncounter()) { fight(s); arrived(); }
        if(s.pendingChest()) claim();
        if(s.heroPos()==before || s.moves()<1) {
            build(s); endDay(); ++stalls;
            if(stalls%7==0) std::cout<<"WAIT day="<<s.day()<<" at="<<s.heroPos().q<<","<<s.heroPos().r<<" goal="<<plan[objective]<<" power="<<int(s.power(s.army()))<<" gold="<<s.treasury()[Resource::Gold]<<"\n";
        } else stalls=0;
        tx.sync(s);
    }
    tx.sync(s);
    std::cout<<"RUN mode="<<mode<<" seed="<<seed<<" result="<<(s.won()?"WON":s.army().empty()?"ARMY LOST":"INCOMPLETE")<<" day="<<s.day()<<" power="<<int(s.power(s.army()))<<" approach="<<s.departureRoute()<<" objective="<<objective<<"/"<<plan.size()<<"\n";
    if(s.won()) std::cout<<"OUTCOME "<<s.scenario().outcome().dump()<<"\n";
    return s.won()?0:1;
}
