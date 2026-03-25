// ===========================================================================
// Cascade Fracture System
//
// Simple Voronoi-based mesh fracture. Pre-fractures at setup time,
// runtime damage separates pieces into RigidBody3D nodes.
//
// This is a self-contained implementation (no Blast SDK dependency).
// For advanced features (hierarchical destruction, stress solver,
// damage shaders), Blast SDK integration is planned for Phase 2.
// ===========================================================================

#include "cascade_fracture.h"

#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/classes/mesh_data_tool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>

using namespace godot;

CascadeFracture::CascadeFracture() {}
CascadeFracture::~CascadeFracture() {}

void CascadeFracture::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_num_pieces", "n"), &CascadeFracture::set_num_pieces);
    ClassDB::bind_method(D_METHOD("get_num_pieces"), &CascadeFracture::get_num_pieces);
    ClassDB::bind_method(D_METHOD("set_fracture_seed", "s"), &CascadeFracture::set_fracture_seed);
    ClassDB::bind_method(D_METHOD("get_fracture_seed"), &CascadeFracture::get_fracture_seed);
    ClassDB::bind_method(D_METHOD("fracture"), &CascadeFracture::fracture);
    ClassDB::bind_method(D_METHOD("apply_damage", "point", "radius", "force"), &CascadeFracture::apply_damage);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "num_pieces", PROPERTY_HINT_RANGE, "2,64,1"), "set_num_pieces", "get_num_pieces");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "fracture_seed"), "set_fracture_seed", "get_fracture_seed");
}

void CascadeFracture::set_num_pieces(int p) { num_pieces = CLAMP(p, 2, 64); }
int CascadeFracture::get_num_pieces() const { return num_pieces; }
void CascadeFracture::set_fracture_seed(int p) { fracture_seed = p; }
int CascadeFracture::get_fracture_seed() const { return fracture_seed; }

void CascadeFracture::_ready() {
    UtilityFunctions::print("[Cascade] CascadeFracture ready. Pieces: ", num_pieces);
}

// -------------------------------------------------------------------
// Voronoi fracture: assign each source vertex to its nearest Voronoi
// site, then group triangles by which site owns all three vertices.
// Triangles spanning multiple sites are split or assigned to the
// site owning the majority of vertices.
// -------------------------------------------------------------------

void CascadeFracture::fracture() {
    Ref<Mesh> source_mesh = get_mesh();
    if (!source_mesh.is_valid() || source_mesh->get_surface_count() == 0) {
        UtilityFunctions::printerr("[Cascade] No mesh to fracture.");
        return;
    }

    _generate_voronoi_pieces();
    is_fractured = true;
    UtilityFunctions::print("[Cascade] Fractured into ", (int)pieces.size(), " pieces.");
}

void CascadeFracture::_generate_voronoi_pieces() {
    Ref<Mesh> source_mesh = get_mesh();
    pieces.clear();
    pieces.resize(num_pieces);

    // Extract vertices from mesh surface 0
    Array surface_arrays = source_mesh->surface_get_arrays(0);
    PackedVector3Array src_verts = surface_arrays[Mesh::ARRAY_VERTEX];
    PackedVector3Array src_normals;
    if (surface_arrays[Mesh::ARRAY_NORMAL].get_type() != Variant::NIL) {
        src_normals = surface_arrays[Mesh::ARRAY_NORMAL];
    }
    PackedInt32Array src_indices;
    if (surface_arrays[Mesh::ARRAY_INDEX].get_type() != Variant::NIL) {
        src_indices = surface_arrays[Mesh::ARRAY_INDEX];
    }

    if (src_verts.size() == 0) return;

    // Compute bounding box
    AABB bounds;
    bounds.position = src_verts[0];
    for (int i = 1; i < src_verts.size(); i++) {
        bounds.expand_to(src_verts[i]);
    }

    // Generate Voronoi sites within bounding box
    std::mt19937 rng(fracture_seed);
    std::uniform_real_distribution<float> dist_x(bounds.position.x, bounds.position.x + bounds.size.x);
    std::uniform_real_distribution<float> dist_y(bounds.position.y, bounds.position.y + bounds.size.y);
    std::uniform_real_distribution<float> dist_z(bounds.position.z, bounds.position.z + bounds.size.z);

    std::vector<Vector3> sites(num_pieces);
    for (int i = 0; i < num_pieces; i++) {
        sites[i] = Vector3(dist_x(rng), dist_y(rng), dist_z(rng));
    }

    // Assign each vertex to nearest Voronoi site
    std::vector<int> vertex_site(src_verts.size());
    for (int i = 0; i < src_verts.size(); i++) {
        float best_dist = 1e30f;
        int best_site = 0;
        for (int s = 0; s < num_pieces; s++) {
            float d = src_verts[i].distance_squared_to(sites[s]);
            if (d < best_dist) {
                best_dist = d;
                best_site = s;
            }
        }
        vertex_site[i] = best_site;
    }

    // Build triangles per piece
    // If mesh has indices, use them; otherwise assume every 3 verts is a triangle
    int tri_count;
    if (src_indices.size() > 0) {
        tri_count = src_indices.size() / 3;
    } else {
        tri_count = src_verts.size() / 3;
    }

    for (int t = 0; t < tri_count; t++) {
        int i0, i1, i2;
        if (src_indices.size() > 0) {
            i0 = src_indices[t * 3];
            i1 = src_indices[t * 3 + 1];
            i2 = src_indices[t * 3 + 2];
        } else {
            i0 = t * 3;
            i1 = t * 3 + 1;
            i2 = t * 3 + 2;
        }

        // Assign triangle to the site that owns the majority of vertices
        int s0 = vertex_site[i0];
        int s1 = vertex_site[i1];
        int s2 = vertex_site[i2];

        int site;
        if (s0 == s1 || s0 == s2) site = s0;
        else if (s1 == s2) site = s1;
        else site = s0; // all different — assign to first vertex's site

        Vector3 v0 = src_verts[i0];
        Vector3 v1 = src_verts[i1];
        Vector3 v2 = src_verts[i2];

        pieces[site].vertices.push_back(v0);
        pieces[site].vertices.push_back(v1);
        pieces[site].vertices.push_back(v2);

        if (src_normals.size() > 0) {
            pieces[site].normals.push_back(src_normals[i0]);
            pieces[site].normals.push_back(src_normals[i1]);
            pieces[site].normals.push_back(src_normals[i2]);
        }
    }

    // Compute centroids
    for (int p = 0; p < num_pieces; p++) {
        Vector3 sum;
        for (int i = 0; i < pieces[p].vertices.size(); i++) {
            sum += pieces[p].vertices[i];
        }
        if (pieces[p].vertices.size() > 0) {
            pieces[p].centroid = sum / (float)pieces[p].vertices.size();
        }
    }
}

// -------------------------------------------------------------------
// Damage: pieces within radius become RigidBody3D
// -------------------------------------------------------------------

void CascadeFracture::apply_damage(Vector3 point, float radius, float force) {
    if (!is_fractured) {
        fracture();
    }

    Transform3D global_xform = get_global_transform();
    Vector3 local_point = global_xform.affine_inverse().xform(point);
    Node *parent = get_parent();
    if (!parent) parent = this;

    int separated_count = 0;

    for (int p = 0; p < (int)pieces.size(); p++) {
        if (pieces[p].separated || pieces[p].vertices.size() == 0) continue;

        float dist = pieces[p].centroid.distance_to(local_point);
        if (dist > radius) continue;

        _create_piece_rigidbody(p, (pieces[p].centroid - local_point).normalized(), force);
        pieces[p].separated = true;
        separated_count++;
    }

    // If all pieces separated, hide the original mesh
    bool all_separated = true;
    for (auto &pc : pieces) {
        if (!pc.separated && pc.vertices.size() > 0) {
            all_separated = false;
            break;
        }
    }
    if (all_separated) {
        set_visible(false);
    }

    if (separated_count > 0) {
        UtilityFunctions::print("[Cascade] Damage: ", separated_count, " pieces separated.");
    }
}

void CascadeFracture::_create_piece_rigidbody(int piece_idx, Vector3 impulse_dir, float force) {
    auto &piece = pieces[piece_idx];

    // Build mesh for this piece
    Ref<SurfaceTool> st;
    st.instantiate();
    st->begin(Mesh::PRIMITIVE_TRIANGLES);

    // Center vertices around the piece centroid
    for (int i = 0; i < piece.vertices.size(); i++) {
        Vector3 local_v = piece.vertices[i] - piece.centroid;
        if (piece.normals.size() > i) {
            st->set_normal(piece.normals[i]);
        }
        st->add_vertex(local_v);
    }

    if (piece.normals.size() == 0) {
        st->generate_normals();
    }

    Ref<ArrayMesh> piece_mesh = st->commit();

    // Create RigidBody3D with the piece mesh
    RigidBody3D *rb = memnew(RigidBody3D);

    MeshInstance3D *mi = memnew(MeshInstance3D);
    mi->set_mesh(piece_mesh);

    // Copy material from source
    Ref<Material> mat = get_material_override();
    if (mat.is_valid()) {
        mi->set_material_override(mat);
    }

    rb->add_child(mi);

    // Add collision shape (convex hull from the piece vertices)
    CollisionShape3D *col = memnew(CollisionShape3D);
    Ref<ConvexPolygonShape3D> shape;
    shape.instantiate();

    PackedVector3Array hull_points;
    for (int i = 0; i < piece.vertices.size(); i++) {
        hull_points.push_back(piece.vertices[i] - piece.centroid);
    }
    shape->set_points(hull_points);
    col->set_shape(shape);
    rb->add_child(col);

    // Position at the piece centroid in world space
    Transform3D global_xform = get_global_transform();
    Vector3 world_centroid = global_xform.xform(piece.centroid);
    rb->set_global_position(world_centroid);
    rb->set_mass(1.0f / (float)num_pieces);

    // Add to scene
    Node *parent = get_parent();
    if (parent) {
        parent->add_child(rb);
    }

    // Apply impulse
    rb->apply_central_impulse(impulse_dir * force);

    // Auto-free after 10 seconds
    rb->set_meta("_cascade_debris", true);
}
