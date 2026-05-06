// ===========================================================================
// Cascade World — Multi-solver coordinator
//
// Orchestrates cloth, fluid, and destruction within a single physics frame
// using IMEX time splitting (Macklin et al. 2024, "Dynamic Duo" pattern).
//
// Each solver runs at its natural timestep:
//   Cloth: 1 substep per frame (XPBD is stable at 1/60s)
//   Fluid: 2 substeps per frame (SPH needs 1/120s for stability)
//   Destruction: event-driven (no regular stepping)
// ===========================================================================

#include "cascade_world.h"
#include "cascade_cloth.h"
#include "cascade_fluid.h"
#include "cascade_fracture.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

CascadeWorld::CascadeWorld() {}
CascadeWorld::~CascadeWorld() {}

void CascadeWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_cloth_substeps", "n"), &CascadeWorld::set_cloth_substeps);
    ClassDB::bind_method(D_METHOD("get_cloth_substeps"), &CascadeWorld::get_cloth_substeps);
    ClassDB::bind_method(D_METHOD("set_fluid_substeps", "n"), &CascadeWorld::set_fluid_substeps);
    ClassDB::bind_method(D_METHOD("get_fluid_substeps"), &CascadeWorld::get_fluid_substeps);
    ClassDB::bind_method(D_METHOD("set_time_scale", "s"), &CascadeWorld::set_time_scale);
    ClassDB::bind_method(D_METHOD("get_time_scale"), &CascadeWorld::get_time_scale);
    ClassDB::bind_method(D_METHOD("set_paused", "p"), &CascadeWorld::set_paused);
    ClassDB::bind_method(D_METHOD("get_paused"), &CascadeWorld::get_paused);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "cloth_substeps", PROPERTY_HINT_RANGE, "1,8,1"), "set_cloth_substeps", "get_cloth_substeps");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "fluid_substeps", PROPERTY_HINT_RANGE, "1,8,1"), "set_fluid_substeps", "get_fluid_substeps");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_scale", PROPERTY_HINT_RANGE, "0.0,4.0,0.1"), "set_time_scale", "get_time_scale");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "get_paused");
}

void CascadeWorld::set_cloth_substeps(int p) { cloth_substeps = CLAMP(p, 1, 8); }
int CascadeWorld::get_cloth_substeps() const { return cloth_substeps; }
void CascadeWorld::set_fluid_substeps(int p) { fluid_substeps = CLAMP(p, 1, 8); }
int CascadeWorld::get_fluid_substeps() const { return fluid_substeps; }
void CascadeWorld::set_time_scale(float p) { time_scale = CLAMP(p, 0.0f, 4.0f); }
float CascadeWorld::get_time_scale() const { return time_scale; }
void CascadeWorld::set_paused(bool p) { paused = p; }
bool CascadeWorld::get_paused() const { return paused; }

void CascadeWorld::_ready() {
    set_physics_process(true);
    UtilityFunctions::print("[Cascade] CascadeWorld ready. Cloth substeps: ",
        cloth_substeps, " Fluid substeps: ", fluid_substeps);
}

void CascadeWorld::_physics_process(double delta) {
    if (paused) return;
    _step_all(delta * time_scale);
}

void CascadeWorld::_step_all(double dt) {
    // IMEX time splitting: fluid runs at a finer timestep than cloth.
    // Within one physics frame:
    //   1. Step cloth solver (cloth_substeps times at dt/cloth_substeps)
    //   2. Step fluid solver (fluid_substeps times at dt/fluid_substeps)
    //   3. Destruction is event-driven (no regular stepping)
    //
    // In the current architecture, each CascadeCloth and CascadeFluid
    // node handles its own _process(). CascadeWorld serves as the
    // configuration hub and future coordination point for:
    //   - Shared collision geometry
    //   - Cross-solver interactions (cloth in fluid, debris in fluid)
    //   - Global memory budget management
    //   - Profiling and debug visualization
    //
    // For now, CascadeWorld's primary role is exposing the substep
    // configuration and time scale. Full IMEX coupling where CascadeWorld
    // directly drives solver steps (bypassing per-node _process) will be
    // implemented when cross-solver interactions are added.
}
