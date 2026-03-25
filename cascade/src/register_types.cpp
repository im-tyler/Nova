#include "register_types.h"
#include "cascade_compute_test.h"
#include "cascade_cloth.h"
#include "cascade_fluid.h"
#include "cascade_fracture.h"
#include "cascade_world.h"

using namespace godot;

void initialize_cascade_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<CascadeComputeTest>();
    ClassDB::register_class<CascadeCloth>();
    ClassDB::register_class<CascadeFluid>();
    ClassDB::register_class<CascadeFracture>();
    ClassDB::register_class<CascadeWorld>();
}

void uninitialize_cascade_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {

GDExtensionBool GDE_EXPORT cascade_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {

    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_cascade_module);
    init_obj.register_terminator(uninitialize_cascade_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}

}
