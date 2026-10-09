#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

class AdventureSession;

/*
 * Condition — the one way a story asks "does this hold right now?".
 *
 * A condition is a JSON tree. Combinators:
 *   {"all": [c, ...]}   every one holds (an empty list holds)
 *   {"any": [c, ...]}   at least one holds (an empty list does not)
 *   {"not": c}          c does not hold
 *   true / false        always / never
 * Leaves (each object takes exactly the keys shown; anything else is an error):
 *   {"flag": id}                     story flag set and truthy (not null/false/0/"")
 *   {"flag": id, "is": value}        story flag equals value (any JSON), e.g.
 *                                    {"flag": "choice.warrior", "is": "give_tag"}
 *   {"item": id}                     the hero carries that item or keepsake
 *   {"companion": id}                that companion is in the party
 *   {"companion": id, "present": true}  ... and travelling: not wounded, not stationed
 *   {"quest": id, "state": "active"|"done"}  given and open / completed
 *   {"cleared": name}                that guard camp was beaten or has left the map
 *   {"lore": title}                  that codex entry has been read
 *   {"trigger_fired": id}            that trigger is used up: it fired, was overtaken
 *                                    (when.unless) or is held (when.wait)
 *   {"trigger_fired": id, "days": {">=": n}}  ... and fired at least n days ago
 *   {"level": {">=": n}}             commander level
 *   {"day": {">=": n}}               the current day
 *   {"resource": "Gold", ">=": n}    treasury; any resource name from Resources.h
 *   {"route": true}                  a way onward is secured (old northern demo)
 * Comparisons take exactly one of ">=", ">", "<=", "<", "==" with an integer.
 *
 * validate() is run on every tree when a scenario loads, so a malformed tree
 * stops the load with its path ("triggers[3].if.all[1]: unknown key 'flg'")
 * instead of quietly holding or failing in play.
 */
namespace Condition {

using Json = nlohmann::json;

// nullopt = well formed; otherwise what is wrong, prefixed with 'where'.
std::optional<std::string> validate(const Json& cond, const std::string& where = "condition");
// Whether the tree holds for this session. Throws std::invalid_argument on a malformed tree.
bool eval(const Json& cond, const AdventureSession& s);
// As eval, against story flags alone; any non-flag leaf throws std::invalid_argument.
bool evalFlags(const Json& cond, const Json& flags);
// The legacy flag condition (when.flag, requires_flag): a name, a list of names
// (all set) or {"name": value, ...} (all equal), as a tree. Throws std::invalid_argument.
Json fromFlags(const Json& legacy);
// {"all": parts} without the wrapper when there is one part; true when there are none.
Json allOf(Json parts);

} // namespace Condition
