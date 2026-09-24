#include "adventure_bridge.h"
#include "combat_session.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

namespace godot {
class UmmCombat : public RefCounted {
    GDCLASS(UmmCombat, RefCounted)
    CombatSession session_;
    static String encode(const CombatSession::Json& value) { return String::utf8(value.dump().c_str()); }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("begin_battle", "data_dir", "army_json", "encounter_json"), &UmmCombat::begin_battle);
        ClassDB::bind_method(D_METHOD("act", "action", "q", "r"), &UmmCombat::act);
        ClassDB::bind_method(D_METHOD("strike", "q", "r", "from_q", "from_r"), &UmmCombat::strike);
        ClassDB::bind_method(D_METHOD("acknowledge", "ticket"), &UmmCombat::acknowledge);
    }
public:
    String begin_battle(const String& dir, const String& army, const String& encounter) {
        try {
            return encode(session_.start(dir.utf8().get_data(), CombatSession::Json::parse(army.utf8().get_data()),
                CombatSession::Json::parse(encounter.utf8().get_data())));
        } catch (const std::exception& error) {
            return encode({{"ok", false}, {"error", error.what()}});
        }
    }
    String act(const String& action, int q, int r) {
        try { return encode(session_.command(action.utf8().get_data(), q, r)); }
        catch (const std::exception& error) { return encode({{"ok", false}, {"error", error.what()}}); }
    }
    // Move-and-attack: walk to (from_q, from_r), then strike the enemy on (q, r).
    String strike(int q, int r, int from_q, int from_r) {
        try { return encode(session_.command("strike", q, r, from_q, from_r)); }
        catch (const std::exception& error) { return encode({{"ok", false}, {"error", error.what()}}); }
    }
    bool acknowledge(int64_t ticket) { return session_.acknowledge(ticket); }
};

class UmmAdventure : public RefCounted {
    GDCLASS(UmmAdventure, RefCounted)
    AdventureBridge bridge_;
    template <typename F>
    static String guarded(F&& call) {
        try { return String::utf8(call().dump().c_str()); }
        catch (const std::exception& error) {
            return String::utf8(AdventureBridge::Json({{"ok", false}, {"error", error.what()}}).dump().c_str());
        }
    }
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("start", "map_path", "data_dir", "encounters_path", "seed", "triggers_path"), &UmmAdventure::start);
        ClassDB::bind_method(D_METHOD("accept_offer", "id"), &UmmAdventure::accept_offer);
        ClassDB::bind_method(D_METHOD("transfer", "q", "r", "unit_id", "count", "to_garrison"), &UmmAdventure::transfer);
        ClassDB::bind_method(D_METHOD("station", "id", "stay"), &UmmAdventure::station);
        ClassDB::bind_method(D_METHOD("save_game", "path", "extra_json"), &UmmAdventure::save_game);
        ClassDB::bind_method(D_METHOD("load_game", "path"), &UmmAdventure::load_game);
        ClassDB::bind_method(D_METHOD("add_item", "id"), &UmmAdventure::add_item);
        ClassDB::bind_method(D_METHOD("claim_chest", "gold"), &UmmAdventure::claim_chest);
        ClassDB::bind_method(D_METHOD("resolve_encounter", "victory"), &UmmAdventure::resolve_encounter);
        ClassDB::bind_method(D_METHOD("set_army", "stacks_json"), &UmmAdventure::set_army);
        ClassDB::bind_method(D_METHOD("companions_fell", "fallen_json", "lost"), &UmmAdventure::companions_fell);
        ClassDB::bind_method(D_METHOD("recruit", "q", "r", "unit_id", "count"), &UmmAdventure::recruit);
        ClassDB::bind_method(D_METHOD("snapshot"), &UmmAdventure::snapshot);
        ClassDB::bind_method(D_METHOD("preview", "q", "r"), &UmmAdventure::preview);
        ClassDB::bind_method(D_METHOD("travel", "q", "r"), &UmmAdventure::travel);
        ClassDB::bind_method(D_METHOD("end_day"), &UmmAdventure::end_day);
    }
public:
    String start(const String& map, const String& dir, const String& encounters, int64_t seed, const String& triggers) {
        return guarded([&] { return bridge_.start(map.utf8().get_data(), dir.utf8().get_data(),
                                                  encounters.utf8().get_data(), static_cast<uint32_t>(seed),
                                                  triggers.utf8().get_data()); });
    }
    String accept_offer(const String& id) { return guarded([&] { return bridge_.accept_offer(id.utf8().get_data()); }); }
    String save_game(const String& path, const String& extra) {
        return guarded([&] { return bridge_.save(path.utf8().get_data(), AdventureBridge::Json::parse(extra.utf8().get_data())); });
    }
    String load_game(const String& path) { return guarded([&] { return bridge_.load(path.utf8().get_data()); }); }
    String station(const String& id, bool stay) { return guarded([&] { return bridge_.station(id.utf8().get_data(), stay); }); }
    String transfer(int q, int r, const String& id, int count, bool to_garrison) {
        return guarded([&] { return bridge_.transfer(q, r, id.utf8().get_data(), count, to_garrison); });
    }
    String add_item(const String& id) { return guarded([&] { return bridge_.add_item(id.utf8().get_data()); }); }
    String claim_chest(bool gold) { return guarded([&] { return bridge_.claim_chest(gold); }); }
    String resolve_encounter(bool victory) { return guarded([&] { return bridge_.resolve_encounter(victory); }); }
    String companions_fell(const String& fallen, bool lost) {
        return guarded([&] { return bridge_.companions_fell(AdventureBridge::Json::parse(fallen.utf8().get_data()), lost); });
    }
    String set_army(const String& stacks) {
        return guarded([&] { return bridge_.set_army(AdventureBridge::Json::parse(stacks.utf8().get_data())); });
    }
    String recruit(int q, int r, const String& id, int count) {
        return guarded([&] { return bridge_.recruit(q, r, id.utf8().get_data(), count); });
    }
    String snapshot() { return guarded([&] { return bridge_.snapshot(); }); }
    String preview(int q, int r) { return guarded([&] { return bridge_.preview(q, r); }); }
    String travel(int q, int r) { return guarded([&] { return bridge_.travel(q, r); }); }
    String end_day() { return guarded([&] { return bridge_.end_day(); }); }
};

void initialize_umm(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    ClassDB::register_class<UmmCombat>();
    ClassDB::register_class<UmmAdventure>();
}
void uninitialize_umm(ModuleInitializationLevel) {}
}

extern "C" GDExtensionBool GDE_EXPORT umm_combat_init(GDExtensionInterfaceGetProcAddress address,
    GDExtensionClassLibraryPtr library, GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject init(address, library, initialization);
    init.register_initializer(godot::initialize_umm);
    init.register_terminator(godot::uninitialize_umm);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
