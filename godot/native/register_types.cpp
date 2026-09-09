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
    bool acknowledge(int64_t ticket) { return session_.acknowledge(ticket); }
};

void initialize_umm(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<UmmCombat>();
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
