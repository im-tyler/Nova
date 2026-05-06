#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

#include <vector>
#include <utility>

namespace godot {

// FractureBody3D: a MeshInstance3D that can be fractured into pieces.
// Pre-fractures the mesh at setup time using Voronoi decomposition.
// On damage, pieces separate and become RigidBody3D nodes.
class CascadeFracture : public MeshInstance3D {
    GDCLASS(CascadeFracture, MeshInstance3D)

public:
    CascadeFracture();
    ~CascadeFracture();

    void _ready() override;

    void set_num_pieces(int p);
    int get_num_pieces() const;

    void set_fracture_seed(int p);
    int get_fracture_seed() const;

    // Call this to fracture the mesh into pieces
    void fracture();

    // Apply damage at a world-space point with a radius
    // Pieces within the radius separate and become rigid bodies
    void apply_damage(Vector3 point, float radius, float force);

protected:
    static void _bind_methods();

private:
    int num_pieces = 8;
    int fracture_seed = 42;
    bool is_fractured = false;

    // Pre-computed fracture data
    struct FracturePiece {
        PackedVector3Array vertices;
        PackedVector3Array normals;
        Vector3 centroid;
        bool separated = false;
    };
    std::vector<FracturePiece> pieces;

    void _generate_voronoi_pieces();
    void _create_piece_rigidbody(int piece_idx, Vector3 impulse_dir, float force);
};

}
