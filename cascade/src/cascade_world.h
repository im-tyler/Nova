#pragma once

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

// CascadeWorld: coordinator node that manages the simulation timestep
// for all Cascade physics nodes in the scene.
//
// Based on Paper #1 (Dynamic Duo, SIGGRAPH 2024) IMEX time splitting:
// each solver type runs at its natural timestep within a frame.
// - Cloth (XPBD): 1/60s (stable at large steps due to implicit constraints)
// - Fluid (SPH): 1/120s (needs smaller steps for stability)
// - Destruction: event-driven (only processes when damage occurs)
//
// Place this node as a parent or sibling of CascadeCloth, CascadeFluid,
// and CascadeFracture nodes. It coordinates their simulation timing.
class CascadeWorld : public Node3D {
    GDCLASS(CascadeWorld, Node3D)

public:
    CascadeWorld();
    ~CascadeWorld();

    void _ready() override;
    void _physics_process(double delta) override;

    void set_cloth_substeps(int p);
    int get_cloth_substeps() const;
    void set_fluid_substeps(int p);
    int get_fluid_substeps() const;
    void set_time_scale(float p);
    float get_time_scale() const;
    void set_paused(bool p);
    bool get_paused() const;

protected:
    static void _bind_methods();

private:
    int cloth_substeps = 1;    // cloth steps per physics frame
    int fluid_substeps = 2;    // fluid steps per physics frame (2x cloth rate)
    float time_scale = 1.0f;
    bool paused = false;

    double accumulated_time = 0.0;

    void _step_all(double dt);
};

}
