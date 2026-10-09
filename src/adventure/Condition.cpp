// Condition.cpp — the story's condition trees (see Condition.h).
#include "Condition.h"
#include "AdventureSession.h"
#include <algorithm>
#include <initializer_list>
#include <stdexcept>

namespace Condition {
namespace {

const char* const kOps[] = {">=", ">", "<=", "<", "=="};

bool isOp(const std::string& key) {
    return std::find(std::begin(kOps), std::end(kOps), key) != std::end(kOps);
}

bool compare(int value, const std::string& op, int n) {
    if (op == ">=") return value >= n;
    if (op == ">")  return value > n;
    if (op == "<=") return value <= n;
    if (op == "<")  return value < n;
    return value == n;
}

std::optional<std::string> checkComparison(const Json& c, const std::string& where) {
    if (!c.is_object() || c.size() != 1 || !isOp(c.begin().key()))
        return where + ": wants exactly one of \">=\", \">\", \"<=\", \"<\", \"==\"";
    if (!c.begin().value().is_number_integer()) return where + ": compares with an integer";
    return std::nullopt;
}

bool comparison(int value, const Json& c) { return compare(value, c.begin().key(), c.begin().value().get<int>()); }

int resourceIndex(const std::string& name) {
    for (int i = 0; i < RESOURCE_COUNT; ++i)
        if (name == resourceName(static_cast<Resource>(i))) return i;
    return -1;
}

// The keys a leaf may carry beside its own, and whether its own value must be a string.
struct Leaf { const char* key; std::initializer_list<const char*> extra; bool stringValue; };
const Leaf kLeaves[] = {
    {"flag", {"is"}, true},        {"item", {}, true},        {"companion", {"present"}, true},
    {"quest", {"state"}, true},    {"cleared", {}, true},     {"lore", {}, true},
    {"trigger_fired", {"days"}, true}, {"level", {}, false},  {"day", {}, false},
    {"resource", {">=", ">", "<=", "<", "=="}, true},         {"route", {}, false},
};

const Leaf* leafOf(const Json& c) {
    for (const auto& leaf : kLeaves) if (c.contains(leaf.key)) return &leaf;
    return nullptr;
}

// c is valid (checked once by the caller).
bool holds(const Json& c, const Json& flags, const AdventureSession* s) {
    if (c.is_boolean()) return c.get<bool>();
    if (c.contains("all")) return std::all_of(c["all"].begin(), c["all"].end(), [&](const Json& p) { return holds(p, flags, s); });
    if (c.contains("any")) return std::any_of(c["any"].begin(), c["any"].end(), [&](const Json& p) { return holds(p, flags, s); });
    if (c.contains("not")) return !holds(c["not"], flags, s);
    if (c.contains("flag")) {
        const std::string name = c["flag"].get<std::string>();
        if (!flags.is_object() || !flags.contains(name)) return false;
        const Json& v = flags[name];
        if (c.contains("is")) return v == c["is"];
        return !(v.is_null() || v == false || v == 0 || v == "");
    }
    if (!s) throw std::invalid_argument("only flag conditions can be read from flags alone");
    const Scenario& sc = s->scenario();
    if (c.contains("item")) return s->hasItem(c["item"].get<std::string>());
    if (c.contains("companion")) {
        for (const auto& sp : s->specials())
            if (sp.id == c["companion"].get<std::string>())
                return !c.value("present", false) || (!s->isWounded(sp) && !sp.stationed);
        return false;
    }
    if (c.contains("quest")) {
        for (const auto& q : sc.quests())
            if (q.id == c["quest"].get<std::string>()) return c["state"] == "done" ? q.done : !q.done;
        return false;
    }
    if (c.contains("cleared")) return sc.cleared(c["cleared"].get<std::string>());
    if (c.contains("lore")) {
        const std::string title = c["lore"].get<std::string>();
        return std::any_of(sc.lore().begin(), sc.lore().end(), [&](const Scenario::Lore& l) { return l.title == title; });
    }
    if (c.contains("trigger_fired")) {
        const std::string id = c["trigger_fired"].get<std::string>();
        if (!sc.fired(id)) return false;
        return !c.contains("days") || comparison(s->day() - std::max(0, sc.firedOn(id)), c["days"]);
    }
    if (c.contains("level")) return comparison(s->heroProgress().level, c["level"]);
    if (c.contains("day")) return comparison(s->day(), c["day"]);
    if (c.contains("resource")) {
        for (const auto& [key, n] : c.items())
            if (key != "resource")
                return compare(s->treasury()[static_cast<Resource>(resourceIndex(c["resource"].get<std::string>()))], key, n.get<int>());
    }
    if (c.contains("route")) return !s->departureRoute().empty();
    throw std::invalid_argument("unreachable condition");
}

} // namespace

std::optional<std::string> validate(const Json& c, const std::string& where) {
    if (c.is_boolean()) return std::nullopt;
    if (!c.is_object() || c.empty()) return where + ": a condition is an object (or true / false)";
    for (const char* op : {"all", "any"})
        if (c.contains(op)) {
            if (c.size() != 1) return where + ": \"" + op + "\" stands alone";
            if (!c[op].is_array()) return where + "." + op + ": wants a list";
            for (size_t i = 0; i < c[op].size(); ++i)
                if (auto err = validate(c[op][i], where + "." + op + "[" + std::to_string(i) + "]")) return err;
            return std::nullopt;
        }
    if (c.contains("not")) {
        if (c.size() != 1) return where + ": \"not\" stands alone";
        return validate(c["not"], where + ".not");
    }
    const Leaf* leaf = leafOf(c);
    if (!leaf) return where + ": unknown condition '" + c.begin().key() + "'";
    for (const auto& [key, value] : c.items()) {
        if (key == leaf->key) continue;
        if (std::none_of(leaf->extra.begin(), leaf->extra.end(), [&](const char* k) { return key == k; }))
            return where + ": unknown key '" + key + "' beside \"" + leaf->key + "\"";
    }
    const std::string key = leaf->key;
    const Json& v = c[key];
    if (leaf->stringValue && (!v.is_string() || v.get<std::string>().empty()))
        return where + "." + key + ": wants a name";
    if (key == "companion" && c.contains("present") && !c["present"].is_boolean())
        return where + ".present: wants true or false";
    if (key == "quest" && c.value("state", "") != "active" && c.value("state", "") != "done")
        return where + ".state: wants \"active\" or \"done\"";
    if (key == "trigger_fired" && c.contains("days")) return checkComparison(c["days"], where + ".days");
    if (key == "level" || key == "day") return checkComparison(v, where + "." + key);
    if (key == "resource") {
        if (resourceIndex(v.get<std::string>()) < 0) return where + ".resource: unknown resource '" + v.get<std::string>() + "'";
        Json cmp = c;
        cmp.erase("resource");
        return checkComparison(cmp, where);
    }
    if (key == "route" && v != true) return where + ".route: wants true";
    return std::nullopt;
}

bool eval(const Json& cond, const AdventureSession& s) {
    if (auto err = validate(cond)) throw std::invalid_argument(*err);
    return holds(cond, s.flags(), &s);
}

bool evalFlags(const Json& cond, const Json& flags) {
    if (auto err = validate(cond)) throw std::invalid_argument(*err);
    return holds(cond, flags, nullptr);
}

Json fromFlags(const Json& legacy) {
    if (legacy.is_string()) return {{"flag", legacy}};
    Json parts = Json::array();
    if (legacy.is_array()) {
        for (const auto& name : legacy) {
            if (!name.is_string()) throw std::invalid_argument("a flag list holds names");
            parts.push_back({{"flag", name}});
        }
    } else if (legacy.is_object()) {
        for (const auto& [name, value] : legacy.items()) parts.push_back({{"flag", name}, {"is", value}});
    } else {
        throw std::invalid_argument("a flag condition is a name, a list of names or {name: value}");
    }
    return {{"all", parts}};
}

Json allOf(Json parts) {
    if (parts.empty()) return true;
    if (parts.size() == 1) return parts[0];
    return {{"all", std::move(parts)}};
}

} // namespace Condition
