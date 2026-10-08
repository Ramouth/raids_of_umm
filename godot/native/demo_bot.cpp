// Scripted, omniscient route verification using production movement, economy and
// CombatEngine. No granted resources, forced victories or save-file teleports.
// Usage: demo_bot <repo_root> [conquest|bypass|slow|escort] [seed] [--transcript]
//        demo_bot <repo_root> campaign <campaign.json> <thorough|hasty> <seed> [--transcript]
//                 [--choose <choiceId>=<optionId> ...]
// The campaign mode plays every map of a campaign in order with objectives read
// from the story data (StoryObjectives), not a hand-written route; see playMap().
// --transcript: stdout carries only what a player is shown (dialogue, quests, lore,
// choices, visits, battles), as plain lines; debug output moves to stderr.
#include "adventure/AdventureSession.h"
#include "adventure/StoryObjectives.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
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
    void newMap() { day=0; quests.clear(); lore=0; choice.clear(); offers.clear(); ended=false; }   // companions and items carry
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
    auto path=s.map().findPathWeighted(s.heroPos(),goal,[](const HexCoord&){return false;},[&](const HexCoord& c){return s.opened().count(c)>0;});
    for(auto at:path) if(at!=s.heroPos() && s.isEncounter(at)) return at;
    return goal;
}
static bool openTranscript(int argc,char** argv) {
    for(int i=1;i<argc;++i) if(std::string(argv[i])=="--transcript") {
        // Keep the real stdout for the transcript; everything else written to fd 1 goes to stderr.
        std::fflush(stdout); std::cout.flush();
        int fd=dup(1);
        if(fd<0 || dup2(2,1)<0 || !(tx.out=fdopen(fd,"w"))) {std::cerr<<"cannot open transcript\n";return false;}
    }
    return true;
}

// ── Campaign mode ─────────────────────────────────────────────────────────────
// Plays each map of a campaign until its ending, then carries the company on
// (AdventureSession::carryOut → Campaign::finishMap/advance). Nothing names a
// place: the bot pursues what StoryObjectives reads from the map's triggers and
// quests, cheapest reachable first, re-evaluated after every step.
//   hasty     only story objectives; offers are skipped
//   thorough  first every reachable pickup, mine, site, winnable guard and
//             affordable offer, then the story objectives
// Choices take the first option the story accepts unless --choose names one.
namespace campaign {

constexpr double MARGIN         = 1.5;   // attack only with this much more power than the enemy
constexpr int    MAP_DAY_LIMIT  = 150;   // a map not ended by then is INCOMPLETE
constexpr int    IDLE_DAY_LIMIT = 30;    // days in a row with nothing to pursue
constexpr int    EXTRA_DAYS     = 10;    // days a thorough route spends on one side target before giving up

struct Policy { bool thorough=false; uint32_t seed=1; std::map<std::string,std::string> choose; };
enum class End { Finished, Incomplete, Lost };
struct Target { HexCoord at; std::string name, why; bool extra=false, bounce=false; };

static std::vector<HexCoord> openPath(const AdventureSession& s,HexCoord goal) {   // ignores guards
    auto p=s.map().findPathWeighted(s.heroPos(),goal,[](const HexCoord&){return false;},[&](const HexCoord& c){return s.opened().count(c)>0;});
    if(!p.empty()) p.erase(p.begin());
    return p;
}
static double need(const AdventureSession& s,HexCoord enemy) { return s.power(guards(s,enemy))*MARGIN; }
static bool winnable(const AdventureSession& s,HexCoord enemy) { return s.power(s.army())>=need(s,enemy); }
// Steps the hero would take toward 'goal' (through guards if it must); empty = unreachable.
static std::vector<HexCoord> way(const AdventureSession& s,HexCoord goal) {
    auto p=s.route(goal);
    return p.empty()?openPath(s,goal):p;
}
// The first guard (camp, zone of control or war-band) on the way, if any.
static std::optional<HexCoord> firstEnemy(const AdventureSession& s,const std::vector<HexCoord>& path) {
    for(auto at:path) {
        if(s.isEncounter(at)) return at;
        if(auto zone=s.guardZoneAt(at)) return zone;
    }
    return std::nullopt;
}
static double cost(const AdventureSession& s,HexCoord goal) {   // < 0 = unreachable
    if(goal==s.heroPos()) return 0;
    auto p=way(s,goal);
    if(p.empty()) return -1;
    double c=s.routeCost(p);
    for(auto at:p) if(s.isEncounter(at)) c+=5;
    return c;
}
static bool affordable(const AdventureSession& s,const ResourcePool& price) {
    for(int i=0;i<RESOURCE_COUNT;++i) if(s.treasury().amounts[i]<price.amounts[i]) return false;
    return true;
}
static bool hasOffer(const AdventureSession& s,const std::string& at) {
    for(const auto* o:s.scenario().offersAt(at)) if(!o->taken && affordable(s,o->cost)) return true;
    return false;
}
// What a thorough route also wants there; "" = nothing.
static std::string sideReason(const AdventureSession& s,const MapObjectDef& o) {
    if(hasOffer(s,o.name)) return "offer";
    switch(o.type) {
        case ObjType::Pickup: case ObjType::Artifact:
            return s.pickupAt(o.pos)?"pickup":"";
        case ObjType::GoldMine: case ObjType::CrystalMine: case ObjType::Sawmill:
        case ObjType::Quarry: case ObjType::ObsidianVent: case ObjType::Dwelling:
            return s.owner(o.pos)!=1?"mine":"";
        case ObjType::Mill: case ObjType::Stables: case ObjType::Watchtower:
        case ObjType::LearningStone: case ObjType::Obelisk:
            return s.siteUsed(o.pos)?"":"site";
        case ObjType::Guard:
            return s.isEncounter(o.pos) && winnable(s,o.pos)?"guard":"";
        default: return "";
    }
}

struct Run {
    AdventureSession& s;
    const Policy& policy;
    std::unordered_set<HexCoord> sideDone;           // side targets visited or given up
    std::set<std::string> been;              // objects the hero has stood on
    std::unordered_map<HexCoord,int> daysOn;         // days spent heading for a side target
    std::string lastGoal, lastSite;
    bool restock=false;
    double wanted=0;
    std::string refused;                     // storySignature() when the open choice refused every option

    // Changes whenever the story moves (a trigger fires, lore, flags, items, companions).
    std::string storySignature() const {
        int fired=0;
        for(const auto& t:s.scenario().triggers()) fired+=s.scenario().fired(t.value("id",""));
        return s.scenario().choice().dump()+"|"+std::to_string(fired)+"|"+std::to_string(s.scenario().lore().size())+"|"
              +s.flags().dump()+"|"+std::to_string(s.items().size())+"|"+std::to_string(s.specials().size());
    }
    void endDay() { tx.sync(s); auto ev=s.endDay(); if(!ev.empty()) tx.line("NOTE "+ev); tx.sync(s); }
    void arrived() {
        auto obj=s.map().objectAt(s.heroPos());
        std::string here=obj?obj->name:"";
        if(!here.empty() && here!=lastSite) tx.line("VISIT "+here);
        if(obj) { been.insert(obj->name); sideDone.insert(obj->pos); }
        lastSite=here;
        tx.sync(s);
    }
    void claim() { auto found=s.claimChest(true); if(!found.empty()) tx.line("NOTE "+found); tx.sync(s); }
    void buildSomething() {
        for(const auto& o:s.map().objects()) {
            if(o.type!=ObjType::Town || s.owner(o.pos)!=1 || s.treasury()[Resource::Gold]<3000) continue;
            for(const auto* b:s.resources().buildingsFor(s.townRoster(o.pos)))
                if(s.buildBlocker(o.pos,b->id).empty()) { s.build(o.pos,b->id); break; }
        }
    }
    void takeOffers() {
        auto obj=s.map().objectAt(s.heroPos());
        if(!obj || !policy.thorough) return;
        std::vector<std::string> ids;
        for(const auto* o:s.scenario().offersAt(obj->name)) if(!o->taken && affordable(s,o->cost)) ids.push_back(o->id);
        for(const auto& id:ids) {
            const std::string label=s.scenario().offer(id)->label;
            if(!s.acceptOffer(id)) tx.line("ACCEPT "+label);
            tx.sync(s);
        }
    }
    // Resolves the open choice: --choose's pick first, else the first option the
    // story accepts (locked ones refuse). false = none was accepted.
    bool decide() {
        tx.sync(s);
        const auto c=s.scenario().choice();
        const std::string id=c.value("id","");
        std::vector<std::string> order;
        const auto wish=policy.choose.find(id);
        if(wish!=policy.choose.end()) order.push_back(wish->second);
        for(const auto& o:c.value("options",Scenario::Json::array())) order.push_back(o.value("id",""));
        for(const auto& pick:order) {
            if(auto err=s.scenario().choose(s,pick)) {
                if(wish!=policy.choose.end() && pick==wish->second) std::cout<<"CHOOSE refused "<<id<<"="<<pick<<": "<<*err<<"\n";
                continue;
            }
            std::cout<<"CHOICE "<<id<<"="<<pick<<"\n";
            tx.chose(s,pick,std::nullopt);
            return true;
        }
        return false;
    }
    std::optional<Target> sideTarget() {
        std::optional<Target> best;
        double bestCost=1e18;
        for(const auto& o:s.map().objects()) {
            if(sideDone.count(o.pos) || s.scenario().vanished().count(o.name)) continue;
            std::string why=sideReason(s,o);
            if(why.empty()) continue;
            if(o.pos==s.heroPos()) { sideDone.insert(o.pos); continue; }
            auto p=way(s,o.pos);
            if(p.empty()) continue;
            if(auto e=firstEnemy(s,p); e && *e!=o.pos && !winnable(s,*e)) continue;   // not reachable yet
            double c=cost(s,o.pos);
            if(c>=0 && c<bestCost) { bestCost=c; best=Target{o.pos,o.name,why,true,false}; }
        }
        return best;
    }
    // The cheapest armed objective the army can reach now; if every one is behind
    // a guard too strong for it, the one behind the weakest guard (resupply for it).
    std::optional<Target> storyTarget() {
        std::optional<Target> best;
        std::pair<bool,double> bestKey{true,1e18};   // (blocked, blocked ? guard's strength : route cost)
        for(const auto& o:storyObjectives(s)) {
            if(!o.armed || (o.event=="quest" && been.count(o.name))) continue;
            const bool bounce=o.at==s.heroPos();
            if(bounce && o.event!="visit") continue;   // standing on it: only a visit needs to step off and back
            const double c=bounce?1:cost(s,o.at);
            if(c<0) continue;
            const auto enemy=bounce?std::nullopt:firstEnemy(s,way(s,o.at));
            const bool blocked=enemy && !winnable(s,*enemy);
            const std::pair<bool,double> key{blocked,blocked?need(s,*enemy):c};
            if(!best || key<bestKey) { bestKey=key; best=Target{o.at,o.name,o.event+(o.trigger.empty()?"":" "+o.trigger),false,bounce}; }
        }
        return best;
    }
    std::optional<Target> townTarget() {
        std::optional<Target> best;
        double bestCost=1e18;
        for(const auto& o:s.map().objects()) if(o.type==ObjType::Town && s.owner(o.pos)==1) {
            double c=cost(s,o.pos);
            if(c>=0 && c<bestCost) { bestCost=c; best=Target{o.pos,o.name,"resupply",false,false}; }
        }
        return best;
    }
    // One step off the hero's cell, so that walking back on fires a visit.
    bool stepOff() {
        for(int d=0;d<6;++d) {
            HexCoord n=s.heroPos().neighbor(d);
            const MapTile* t=s.map().tileAt(n);
            if(!t || !t->passable || s.isEncounter(n) || s.guardZoneAt(n) || s.map().objectAt(n) || s.route(n).size()!=1) continue;
            s.travel(n);
            if(s.heroPos()==n) { lastSite.clear(); return true; }
        }
        return false;
    }
    void stall(const std::optional<Target>& goal) {
        buildSomething(); endDay();
        if(goal && goal->extra && ++daysOn[goal->at]>EXTRA_DAYS) sideDone.insert(goal->at);
    }

    End play() {
        int idle=0;
        for(int tick=0;tick<20000;++tick) {
            if(s.won()) return End::Finished;
            if(s.lost()) return End::Lost;
            if(s.day()>MAP_DAY_LIMIT) return End::Incomplete;
            if(s.pendingEncounter()) {
                const bool armed=!s.army().empty();
                fight(s);
                arrived();
                if(armed && s.army().empty() && !s.won()) return End::Lost;
                continue;
            }
            if(s.pendingChest()) claim();
            if(s.needsPath() && !s.heroPaths().empty()) {
                s.choosePath(s.heroPaths().front().id);
                for(const auto& p:s.heroPaths()) if(p.id==s.heroPath()) tx.line("PATH "+p.name);
                tx.sync(s);
            }
            std::optional<Target> goal;
            if(s.scenario().awaitingChoice()) {
                const auto c=s.scenario().choice();
                HexCoord at=s.heroPos();
                if(c.contains("at")) at=HexCoord(c["at"][0].get<int>(),c["at"][1].get<int>());
                const bool retry=refused!=storySignature();   // the story moved since every option refused
                if(at!=s.heroPos()) { if(retry) goal=Target{at,"the choice","choice",false,false}; }
                else if(retry && decide()) continue;
                else if(s.scenario().blocksAdventure()) { std::cout<<"STUCK choice="<<c.value("id","")<<" no option accepted\n"; return End::Incomplete; }
                else refused=storySignature();
            }
            recruit(s);
            takeOffers();
            if(restock && s.power(s.army())>=wanted) restock=false;
            if(restock && !goal) {
                goal=townTarget();
                if(!goal) restock=false;   // nowhere to recruit: go as we are
            }
            if(!goal && policy.thorough) goal=sideTarget();
            if(!goal) goal=storyTarget();
            if(!goal) {
                stall(goal);
                if(++idle>IDLE_DAY_LIMIT) { std::cout<<"STUCK day="<<s.day()<<" nothing to pursue\n"; return End::Incomplete; }
                continue;
            }
            idle=0;
            const std::string label=goal->why+" "+goal->name;
            if(label!=lastGoal) { std::cout<<"GOAL day="<<s.day()<<" "<<label<<"\n"; lastGoal=label; }
            if(goal->at==s.heroPos()) {
                if(restock) { stall(goal); continue; }   // in town, waiting for recruits
                if(goal->bounce && stepOff()) continue;
                if(goal->bounce) { stall(goal); continue; }
                been.insert(goal->name); sideDone.insert(goal->at);
                continue;
            }
            auto target=nextStop(s,goal->at);
            // Resupply needs a town to recruit in; with none, go as we are.
            if(!restock && townTarget()) if(auto enemy=firstEnemy(s,way(s,target)); enemy && !winnable(s,*enemy)) {
                if(goal->extra) { sideDone.insert(goal->at); continue; }
                restock=true; wanted=need(s,*enemy);
                const auto* blocker=s.map().objectAt(*enemy);
                std::cout<<"RESUPPLY day="<<s.day()<<" power="<<int(s.power(s.army()))<<" need="<<int(wanted)<<" for="<<goal->name<<" blocker="<<(blocker?blocker->name:"field army")<<"\n";
                continue;
            }
            auto before=s.heroPos();
            s.travel(target);
            if(s.heroPos()!=before) arrived();
            if(s.pendingEncounter()) continue;
            if(s.pendingChest()) claim();
            if(s.heroPos()==before || s.moves()<1) stall(goal);
            tx.sync(s);
        }
        return End::Incomplete;
    }
};

static int campaignMain(const std::string& root,int argc,char** argv) {
    if(argc<6) { std::cerr<<"usage: demo_bot <repo_root> campaign <campaign.json> <thorough|hasty> <seed> [--transcript] [--choose <choiceId>=<optionId> ...]\n"; return 2; }
    std::string file=argv[3];
    if(file.empty() || file.front()!='/') file=root+"/"+file;
    Policy policy;
    const std::string style=argv[4];
    if(style!="thorough" && style!="hasty") { std::cerr<<"style must be thorough or hasty\n"; return 2; }
    policy.thorough=style=="thorough";
    policy.seed=static_cast<uint32_t>(std::stoul(argv[5]));
    for(int i=6;i<argc;++i) {
        if(std::string(argv[i])!="--choose") continue;
        const std::string pair=i+1<argc?argv[++i]:"";
        const auto eq=pair.find('=');
        if(eq==std::string::npos || eq==0) { std::cerr<<"--choose wants <choiceId>=<optionId>\n"; return 2; }
        policy.choose[pair.substr(0,eq)]=pair.substr(eq+1);
    }
    Campaign campaign;
    if(auto err=campaign.load(file,root+"/data")) { std::cerr<<*err<<"\n"; return 2; }
    std::string lastMap;
    int lastDay=0;
    auto finish=[&](const std::string& result) {
        std::cout<<"CAMPAIGN result="<<result<<" map="<<lastMap<<" day="<<lastDay<<"\n";
        std::cout.flush();
        return result=="FINISHED"?0:1;
    };
    while(const Campaign::MapEntry* entry=campaign.current()) {
        const CampaignState carry=campaign.startState();
        AdventureSession s;
        lastMap=entry->id; lastDay=0;
        const std::string header="MAP "+entry->id+" "+entry->title;   // the title card
        std::cout<<header<<"\n";
        tx.newMap();
        tx.line(header);
        if(auto err=s.start(campaign.resolve(entry->map),campaign.dataDir(),campaign.resolve(entry->encounters),
                            policy.seed,campaign.resolve(entry->triggers),carry)) {
            std::cerr<<*err<<"\n";
            return finish("INCOMPLETE");
        }
        tx.sync(s);
        Run run{s,policy};
        const End end=run.play();
        tx.sync(s);
        lastDay=s.day();
        // Experience is conserved: carried in + earned here = held now + taken by the cap.
        const int lost=s.xpLostToCap();
        const int earned=s.heroProgress().xp+lost-(carry.carried?carry.xp:0);
        const int reached=s.capReachedDay();
        const bool trivial=(reached>=0 && reached*3<s.day()*2) || lost*4>earned;
        std::cout<<"CAP map="<<entry->id<<" cap="<<s.levelCap()<<" reached_day="<<(reached<0?std::string("never"):std::to_string(reached))
                 <<" map_days="<<s.day()<<" xp_lost="<<lost<<" xp_earned="<<earned<<" trivial="<<(trivial?"yes":"no")<<"\n";
        if(end==End::Lost) return finish("ARMY LOST");
        if(end!=End::Finished) return finish("INCOMPLETE");
        campaign.finishMap(s.carryOut());
        auto step=campaign.advance(s.nextMap());
        if(step.error) { std::cerr<<*step.error<<"\n"; return finish("INCOMPLETE"); }
        if(step.finished) return finish("FINISHED");
    }
    return finish("INCOMPLETE");
}

} // namespace campaign
using campaign::campaignMain;
int main(int argc,char** argv) {
    const std::string root=argc>1?argv[1]:".";
    const std::string mode=argc>2?argv[2]:"bypass";
    if(!openTranscript(argc,argv)) return 1;
    if(mode=="campaign") return campaignMain(root,argc,argv);
    const int seed=argc>3?std::stoi(argv[3]):1;
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
