#pragma once
#include "hex/HexCoord.h"
#include <string>
#include <vector>

class AdventureSession;

/*
 * StoryObjectives — where the story wants the hero to go, read from the map's
 * triggers and open quests instead of a hand-written route (demo_bot's
 * campaign mode plays any map with it).
 *
 * Every trigger that has not fired and waits on a place yields one objective
 * per place it names:
 *   visit / capture / encounter_won / engage   when.name (a map object)
 *   cleared                                    each of when.names still unbeaten
 *   rival_beaten                               when.name (a living war-band)
 *   enter_q                                    each open tile in column q_min, named
 *                                              "the road east (q,r)"
 * Places already done for that event drop out: a captured object, a beaten
 * guard, a vanished object. A trigger overtaken by its when.unless is gone.
 *
 * armed = the trigger's story conditions hold now (when.after fired, its
 * when.delay has passed, when.flag matches, when.unless_flag does not, its
 * "if" holds; Scenario::armed, the same evaluator that fires it), so
 * getting there would fire it. Unarmed objectives are listed for context.
 *
 * Open (not done) quests add event "quest" objectives (armed, no trigger) for
 * every map object whose name appears in the quest's title or text. The
 * session cannot tell whether such a place was "done"; the caller remembers
 * which ones it has been to.
 *
 * One objective per (name, event, trigger); order follows the triggers file,
 * then the quests.
 */
struct StoryObjective {
    std::string name;      // map object or war-band
    HexCoord    at;        // where it is now
    std::string event;     // visit | capture | encounter_won | engage | cleared | rival_beaten | enter_q | quest
    std::string trigger;   // trigger id; "" for a quest mention
    bool        armed = false;
};

std::vector<StoryObjective> storyObjectives(const AdventureSession& s);
